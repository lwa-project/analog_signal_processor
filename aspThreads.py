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
        
import run


__version__ = '0.2'
__revision__ = '$Rev$'
__all__ = ['TemperatureSensors', 'PowerStatus', 'SendSPI', '__version__', '__revision__', '__all__']


aspThreadsLogger = logging.getLogger('__main__')


# Create a semaphore to make sure the monitoring threads don't fight over the SUB-20 device
SUB20Lock = threading.Semaphore(1)


class TemperatureSensors(object):
	"""
	Class for monitoring temperature for the power supplies via the I2C interface.
	"""
	
	def __init__(self, config, logfile='/data/temp.txt', ASPCallbackInstance=None):
		self.logfile = logfile
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
		self.minTemp  = config['TEMPMIN']
		self.warnTemp = config['TEMPWARN']
		self.maxTemp  = config['TEMPMAX']
		
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
			
			SUB20Lock.acquire()
			
			try:
				missingSUB20 = False
				
				p = subprocess.Popen(['readThermometers',], shell=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
				p.wait()
					
				output, output2 = p.communicate()
				
				if p.returncode != 0:
					aspThreadsLogger.warning("readThermometers: command returned %i; '%s;%s'", p.returncode, output, output2)
					self.lastError = str(output2)
					
					missingSUB20 = True
					
				else:
					for i,line in enumerate(output.split('\n')):
						if len(line) < 4:
							continue
						psu, desc, tempC = line.split(None, 2)
						self.description[i] = '%s %s' % (psu, desc)
						self.temp[i] = float(tempC)
						
				# Open the log file and save the temps
				try:
					log = open(self.logfile, 'a+')
					log.write('%s,' % time.time())
					log.write('%s\n' % ','.join(["%.2f" % t for t in self.temp]))
					log.flush()
					log.close()
				except IOError:
					aspThreadsLogger.error("%s: could not open flag logfile %s for writing", type(self).__name__, self.logfile)
					pass
			
				# Make sure we aren't critical (on either side of good)
				if self.ASPCallbackInstance is not None and self.temp is not None:
					if missingSUB20:
						self.ASPCallbackInstance.processMissingSUB20()
					
					if max(self.temp) > self.maxTemp:
						aspThreadsLogger.critical('%s: monitorThread max. temperature is %.1f C', type(self).__name__, max(self.temp))
						
						self.ASPCallbackInstance.processCriticalTemperature(high=True)
						
					elif min(self.temp) < self.minTemp:
						aspThreadsLogger.warning('%s: monitorThread min. temperature is %.1f C', type(self).__name__, min(self.temp))
						
						self.ASPCallbackInstance.processCriticalTemperature(low=True)
						
					elif max(self.temp) > self.warnTemp:
						aspThreadsLogger.warning('%s: monitorThread max. temperature is %.1f C', type(self).__name__, max(self.temp))
						
						self.ASPCallbackInstance.processWarningTemperature()
					else:
						self.ASPCallbackInstance.processWarningTemperature(clear=True)
					
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
				
			SUB20Lock.release()
			
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
			
	def getOverallStatus(self):
		"""
		Find out the overall temperature status.
		"""
		
		if self.temp is None:
			return None
			
		status = 'IN_RANGE'
		for t in self.temp:
			if t < self.minTemp:
				status = 'UNDER_TEMP'
				break
			elif t > self.maxTemp:
				status = 'OVER_TEMP'
				break
			else:
				pass
			
		return status


class PowerStatus(object):
	"""
	Class for monitoring output voltage and current as well as the status
	for the power supplies via the I2C interface.
	"""
	
	def __init__(self, deviceAddress, config, logfile='/data/psu.txt', ASPCallbackInstance=None):
		self.deviceAddress = int(deviceAddress)
		base, ext = logfile.rsplit('.', 1)
		self.logfile = '%s-0x%02X.%s' % (base, self.deviceAddress, ext)
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
			
			SUB20Lock.acquire()
			
			try:
				missingSUB20 = False
				
				p = subprocess.Popen(['readPSU', "0x%02X" % self.deviceAddress], shell=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
				p.wait()
					
				output, output2 = p.communicate()
				
				if p.returncode != 0:
					aspThreadsLogger.warning("readPSU: command returned %i; '%s;%s'", p.returncode, output, output2)
					self.voltage = 0.0
					self.current = 0.0
					self.onoff = "UNK"
					self.status = "UNK"
					self.lastError = str(output2)
					
					missingSUB20 = True
					
				else:
					psu, desc, onoffHuh, statusHuh, voltageV, currentA, = output.replace('\n', '').split(None, 5)
					self.description = '%s - %s' % (psu, desc)
					self.voltage = float(voltageV)
					self.current = float(currentA)
					self.onoff = '%-3s' % onoffHuh
					self.status = statusHuh
					
				try:
					log = open(self.logfile, 'a+')
					log.write('%s,' % time.time())
					log.write('%s\n' % ','.join(["%.2f" % self.voltage, "%.3f" % self.current, self.onoff, self.status]))
					log.flush()
					log.close()
				except IOError:
					aspThreadsLogger.error("%s: could not open flag logfile %s for writing", type(self).__name__, self.logfile)
					pass
					
				# Deal with power supplies that are over temperature, current, or voltage; 
				# or under voltage; or has a module fault
				if self.ASPCallbackInstance is not None:
					if missingSUB20:
						self.ASPCallbackInstance.processMissingSUB20()
						
					for modeOfFailure in ('OverTemperature', 'OverCurrent', 'OverVolt', 'UnderVolt', 'ModuleFault'):
						if self.status.find(modeOfFailure) != -1:
							aspThreadsLogger.critical('%s: monitorThread PS at 0x%02X is in %s', type(self).__name__, self.deviceAddress, modeOfFailure)
							
							self.ASPCallbackInstance.processCriticalPowerSupply(self.deviceAddress, modeOfFailure)
						
			except Exception, e:
				exc_type, exc_value, exc_traceback = sys.exc_info()
				aspThreadsLogger.error("%s: monitorThread 0x%02X failed with: %s at line %i", type(self).__name__, self.deviceAddress, str(e), traceback.tb_lineno(exc_traceback))
				
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
				
			SUB20Lock.release()
			
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


class SendSPI(object):
	def __init__(self, config, ASPCallbackInstance=None):
		self.config = config
		
		# Update the board configuration
		self.updateConfig()
		
		# Set the current file number
		self.SPIFileNumber = 1
		
		# Setup the callback
		self.ASPCallbackInstance = ASPCallbackInstance
		
		# Setup threading
		self.thread = None
		self.alive = threading.Event()
		self.lastError = None
		
	def updateConfig(self, config=None):
		"""
		Using the configuration file, update the configuration.
		"""
		
		# Update the current configuration
		if config is not None:
			self.config = config
		
	def start(self):
		"""
		Start the monitoring thread.
		"""

		if self.thread is not None:
			self.stop()
			
		self.thread = threading.Thread(target=self.sendCommands)
		self.thread.setDaemon(1)
		self.alive.set()
		self.thread.start()
		
		time.sleep(1)

	def stop(self):
		"""
		Stop the monitor thread, waiting until it's finished.
		"""

		os.system("pkill sendARXDeviceBatch")

		if self.thread is not None:
			self.alive.clear()          #clear alive event for thread
			self.thread.join()          #wait until thread has finished
			self.thread = None
			self.lastError = None
			
	def sendCommands(self):
		while self.alive.isSet():
			tStart = time.time()
			
			proc = None
			
			try:
				# Figure out which temp file we are on
				oldSPIFileNumber = self.SPIFileNumber
				self.SPIFileNumber = oldSPIFileNumber + 1
				if self.SPIFileNumber > 5:
					self.SPIFileNumber = 1
				
				SPIFilename = self.config['SPIFILE']
				tmpSPIFilename = '%s.tmp%i' % (self.config['SPIFILE'], self.SPIFileNumber)
				
				# Remove the old temp file
				try:
					os.unlink(tmpSPIFilename)
				except OSError:
					pass
				
				# Symbolic link to current temp file
				try:
					os.unlink(SPIFilename)
				except OSError:
					pass
				os.symlink(tmpSPIFilename, SPIFilename)
				
				# Check if file exists and has is non-empty
				SPIFilename = self.config['SPIFILE']
				tmpSPIFilename = '%s.tmp%i' % (SPIFilename, oldSPIFileNumber)
				
				if os.path.isfile(tmpSPIFilename) and os.path.getsize(tmpSPIFilename):
					spi_command = ['sendARXDeviceBatch', tmpSPIFilename]
					
					aspThreadsLogger.info("SendSPI: Sent %s", spi_command)
					proc = run.spawn_process('sendARXDeviceBatch', spi_command, '/home/ops/board.log')
					
			except Exception, e:
				exc_type, exc_value, exc_traceback = sys.exc_info()
				aspThreadsLogger.error("SendSPI: sendCommands failed with: %s at line %i", str(e), traceback.tb_lineno(exc_traceback))
				
				## Grab the full traceback and save it to a string via StringIO
				fileObject = StringIO.StringIO()
				traceback.print_tb(exc_traceback, file=fileObject)
				tbString = fileObject.getvalue()
				fileObject.close()
				## Print the traceback to the logger as a series of DEBUG messages
				for line in tbString.split('\n'):
					aspThreadsLogger.debug("%s", line)
				
				self.lastError = str(e)
				
			# Sleep
			if proc is not None:
				proc.wait()
				
				tStop = time.time()
				aspThreadsLogger.debug("SendSPI: sendCommands finished in %.1f seconds", tStop-tStart)
				
				if proc.returncode > 20:
					aspThreadsLogger.warning("SendSPI: %i commands had to be retried due to verification failures", proc.returncode-20)
					
					if self.ASPCallbackInstance is not None:
						self.ASPCallbackInstance.processRepeatedSPIErrors(proc.returncode-20)
				
			time.sleep(1.0)
			