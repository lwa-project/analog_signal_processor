# -*- coding: utf-8 -*-

"""
Module implementing the various ASP monitoring threads.

$Rev$
$LastChangedBy$
$LastChangedDate$
"""

import os
import sys
import time
import logging
import threading
import traceback
import subprocess
try:
        import cStringIO as StringIO
except ImportError:
        import StringIO

__version__ = '0.2'
__revision__ = '$Rev$'
__all__ = ['TemperatureSensors', 'PowerStatus', '__version__', '__revision__', '__all__']


aspThreadsLogger = logging.getLogger('__main__')


class TemperatureSensors(object):
	"""
	Class for monitoring temperature for the power supplies via the I2C interface.
	"""
	
	def __init__(self, config, ASPCallbackInstance=None):
		self.updateConfig(config)
		
		# Setup the callback
		self.ASPCallbackInstance = ASPCallbackInstance
		
		# Setup temperature sensors
		self.nTemps = 0
		self.description = None
		self.temp = None
		self.lastError = None

		self.thread = None
		self.alive = threading.Event()
		
	def updateConfig(self, config=None):
		"""
		Update the current configuration.
		"""
		
		if config is None:
			return True
			
		self.monitorPeriod = config['TEMPPERIOD']
		self.minTemp = config['TEMPMIN']
		self.maxTemp = config['TEMPMAX']
		
	def start(self):
		"""
		Start the monitoring thread.
		"""

		if self.thread is not None:
			self.stop()
			
		self.nTemps = os.system("countThermometers") / 256
		self.description = ["UNK" for i in xrange(self.nTemps)]
		self.temp = [0.0 for i in xrange(self.nTemps)]
		
		self.thread = threading.Thread(target=self.monitorThread)
		self.thread.setDaemon(1)
		self.alive.set()
		self.thread.start()
		
		time.sleep(1)

	def stop(self):
		"""
		Stop the monitor thread, waiting until it's finished.
		"""

		os.system("pkill readThermometers")

		if self.thread is not None:
			self.alive.clear()          #clear alive event for thread
			self.thread.join()          #wait until thread has finished
			self.thread = None
			self.nTemps = 0
			self.lastError = None

	def monitorThread(self):
		"""
		Create a monitoring thread for the temperature.
		"""

		while self.alive.isSet():
			tStart = time.time()
			
			try:
				p = subprocess.Popen(['readThermometers',], shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
				p.wait()
					
				output, output2 = p.communicate()
				
				if p.returncode != 0:
					aspThreadsLogger.warning("readThermometers: command returned %i; '%s;%s'", p.returncode, output, output2)
					self.lastError = str(ouput2)
				
				for i,line in enumerate(output.split('\n')):
					if len(line) < 4:
						continue
					psu, desc, tempC = line.split(None, 2)
					self.description[i] = '%s %s' % (psu, desc)
					self.temp[i] = float(tempC)
				
			except Exception, e:
				exc_type, exc_value, exc_traceback = sys.exc_info()
				aspThreadsLogger.error("%s: monitorThread failed with: %s at line %i", type(self).__name__, str(e), traceback.tb_lineno(exc_traceback))
				
				## Grab the full traceback and save it to a string via StringIO
				fileObject = StringIO.StringIO()
				traceback.print_tb(exc_traceback, file=fileObject)
				tbString = fileObject.getvalue()
				fileObject.close()
				## Print the traceback to the logger as a series of DEBUG messages
				for line in tbString.split('\n'):
					aspThreadsLogger.debug("%s", line)
				
				self.temp = None
				self.lastError = str(e)
			
			# Make sure we aren't critical (on either side of good)
			if self.ASPCallbackInstance is not None and self.temp is not None:
				if max(self.temp) > maxTemp:
					aspThreadsLogger.critical('%s: monitorThread max. temperature is %.1f C', type(self).__name__, max(self.temp))
					
					self.ASPCallbackInstance.processCriticalTemperature(high=True)
					
				elif min(self.temp) < minTemp:
					aspThreadsLogger.critical('%s: monitorThread min. temperature is %.1f C', type(self).__name__, min(self.temp))
					
					self.ASPCallbackInstance.processCriticalTemperature(low=True)
					
			# Stop time
			tStop = time.time()
			aspThreadsLogger.debug('Finished updating temperatures in %.3f seconds', tStop - tStart)
			
			# Sleep for a bit
			sleepCount = 0
			sleepTime = self.monitorPeriod - (tStop - tStart)
			while (self.alive.isSet() and sleepCount < sleepTime):
				time.sleep(0.2)
				sleepCount += 0.2
				
	def getSensorCount(self):
		"""
		Convenience function to get the number of temperature sensors.
		"""
		
		return self.nTemps
		
	def getDescription(self, sensor=0):
		"""
		Convenience function to get the description of the temperature sensor.
		"""
		
		if self.temp is None:
			return None
			
		if sensor < 0 or sensor >= self.nTemps:
			return None
			
		return self.description[sensor]
		
	def getTemperature(self, sensor=0, DegreesF=False):
		"""
		Convenience function to get the temperature.
		"""

		if self.temp is None:
			return None
			
		if sensor < 0 or sensor >= self.nTemps:
			return None

		if DegreesF:
			return 1.8*self.temp[sensor] + 32
		else:
			return self.temp[sensor]


class PowerStatus(object):
	"""
	Class for monitoring output voltage and current as well as the status
	for the power supplies via the I2C interface.
	"""
	
	def __init__(self, deviceAddress, config, ASPCallbackInstance=None):
		self.deviceAddress = int(deviceAddress)
		self.updateConfig(config)
		
		# Setup the callback
		self.ASPCallbackInstance = ASPCallbackInstance
		
		# Setup voltage, current, and status
		self.nPSUs = 0
		self.description = None
		self.voltage = None
		self.current = None
		self.onoff = None
		self.status = None
		self.lastError = None

		self.thread = None
		self.alive = threading.Event()
		
	def updateConfig(self, config=None):
		"""
		Update the current configuration.
		"""
		
		if config is None:
			return True
			
		self.monitorPeriod = config['POWERPERIOD']
		
	def start(self):
		"""
		Start the monitoring thread.
		"""

		if self.thread is not None:
			self.stop()
			
		self.description = "UNK"
		self.voltage     = 0.0
		self.current     = 0.0
		self.onoff       = "UNK"
		self.status      = "UNK"
			
		self.thread = threading.Thread(target=self.monitorThread)
		self.thread.setDaemon(1)
		self.alive.set()
		self.thread.start()
		
		time.sleep(1)

	def stop(self):
		"""
		Stop the monitor thread, waiting until it's finished.
		"""

		if self.thread is not None:
			self.alive.clear()          #clear alive event for thread
			self.thread.join()          #wait until thread has finished
			self.thread = None
			self.nPSUs = 0
			self.lastError = None

	def monitorThread(self):
		"""
		Create a monitoring thread for the temperature.
		"""

		while self.alive.isSet():
			tStart = time.time()
			
			try:
				p = subprocess.Popen(['readPSU', "0x%02X" % self.deviceAddress], shell=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
				p.wait()
					
				output, output2 = p.communicate()
				
				if p.returncode != 0:
					aspThreadsLogger.warning("readPSU: command returned %i; '%s;%s'", p.returncode, output, output2)
					self.temp = None
					self.lastError = str(ouput2)
				
				psu, desc, onoffHuh, statusHuh, voltageV, currentA, = output.split(None, 5)
				self.description[i] = '%s %s' % (psu, desc)
				self.voltage[i] = float(voltageV)
				self.current[i] = float(currentA)
				self.onoff[i] = '%3s' % onoffHuh
				self.status[i] = statusHuh
				
			except Exception, e:
				exc_type, exc_value, exc_traceback = sys.exc_info()
				aspThreadsLogger.error("%s: monitorThread failed with: %s at line %i", type(self).__name__, str(e), traceback.tb_lineno(exc_traceback))
				
				## Grab the full traceback and save it to a string via StringIO
				fileObject = StringIO.StringIO()
				traceback.print_tb(exc_traceback, file=fileObject)
				tbString = fileObject.getvalue()
				fileObject.close()
				## Print the traceback to the logger as a series of DEBUG messages
				for line in tbString.split('\n'):
					aspThreadsLogger.debug("%s", line)
				
				self.voltage = 0.0
				self.current = 0.0
				self.onoff = "UNK"
				self.status = "UNK"
				self.lastError = str(e)
				
			# Deal with power supplies that are over temperature, current, or voltage; 
			# or under voltage; or has a module fault
			if self.ASPCallbackInstance is not None:
				if self.status in ('OverTemperature', 'OverCurrent', 'OverVolt', 'UnderVolt', 'ModuleFault'):
					self.ASPCallbackInstance.processCriticalPowerSupply(self.deviceAddress, self.status)
					
			# Stop time
			tStop = time.time()
			aspThreadsLogger.debug('Finished updating PSU status for 0x%02X in %.3f seconds', self.deviceAddress, tStop - tStart)
			
			# Sleep for a bit
			sleepCount = 0
			sleepTime = self.monitorPeriod - (tStop - tStart)
			while (self.alive.isSet() and sleepCount < sleepTime):
				time.sleep(0.2)
				sleepCount += 0.2
				
	def getDeviceAddress(self):
		"""
		Convenience function to get the I2C address of the PSU.
		"""
		
		return self.deviceAddress
		
	def getDescription(self):
		"""
		Convenience function to get the description of the module.
		"""
		
		return self.description
		
	def getVoltage(self):
		"""
		Convenience function to get the voltage in volts.
		"""

		return self.voltage
		
	def getCurrent(self):
		"""
		Convenience function to get the current in amps.
		"""

		return self.current
		
	def getOnOff(self):
		"""
		Convenience function to get the module on/off status as a human-readable 
		string.
		"""
		
		return self.onoff
		
	def getStatus(self):
		"""
		Convenience function to get the module status as a human-readable 
		string.
		"""

		return self.status
