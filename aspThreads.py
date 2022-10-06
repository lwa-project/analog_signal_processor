# -*- coding: utf-8 -*-

"""
Module implementing the various ASP monitoring threads.
"""

from __future__ import division

import os
import sys
import time
import select
import logging
import threading
import traceback
import subprocess
try:
    from cStringIO import StringIO
except ImportError:
    from io import StringIO
    
from lwainflux import LWAInfluxClient

from aspRS485 import rs485Check, rs485Power
from aspI2C import psuRead


__version__ = '0.6'
__all__ = ['BackendService', 'TemperatureSensors', 'PowerStatus', 'ChassisStatus']


aspThreadsLogger = logging.getLogger('__main__')


class BackendService(object):
    """
    Class for managing the RS485 background service.
    """
    
    def __init__(self, ASPCallbackInstance=None):
        self.service_running = False
        
        # Setup the callback
        self.ASPCallbackInstance = ASPCallbackInstance
        
        self.thread = None
        self.alive = threading.Event()
        
    def start(self):
        """
        Start the background service thread.
        """
        
        if self.thread is not None:
            self.stop()
            
        self.thread = threading.Thread(target=self.serviceThread)
        self.thread.setDaemon(1)
        self.alive.set()
        self.thread.start()
        
        time.sleep(1)
        
    def stop(self):
        """
        Stop the background service thread, waiting until it's finished.
        """
        
        if self.thread is not None:
            self.alive.clear()          #clear alive event for thread
            self.thread.join()          #wait until thread has finished
            
    def serviceThread(self):
        # Determine the path for the service executable
        path = os.path.dirname(os.path.abspath(__file__))
        path = os.path.join(path, 'backend')
        
        # Start the service
        service = subprocess.Popen([os.path.join(path, 'lwaARXSerial')], cwd=path, 
                                   stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        watch_out = select.poll()
        watch_out.register(service.stdout)
        watch_err = select.poll()
        watch_err.register(service.stderr)
        
        while self.alive.isSet():
            try:
                ## Are we alive?
                if service.poll() is None:
                    self.service_running = True
                else:
                    self.service_running = False
                    if self.ASPCallbackInstance is not None:
                        self.ASPCallbackInstance.processNoBackendService(self.service_running)
                        
                ## Is there anything to read on stdout?
                if watch_out.poll(1):
                    ### Good, read in all that we can
                    aspThreadsLogger.debug("%s: serviceThread %s", type(self).__name__, service.stdout.readline())
                    while watch_out.poll(1):
                        aspThreadsLogger.debug("%s: serviceThread %s", type(self).__name__, service.stdout.readline())
                        
                ## Is there anything to read on stderr?
                if watch_err.poll(1):
                    ### Ugh, read in all that we can
                    aspThreadsLogger.error("%s: serviceThread %s", type(self).__name__, service.stderr.readline())
                    while watch_err.poll(1):
                        aspThreadsLogger.error("%s: serviceThread %s", type(self).__name__, service.stderr.readline())
                        
            except Exception as e:
                exc_type, exc_value, exc_traceback = sys.exc_info()
                aspThreadsLogger.error("%s: serviceThread failed with: %s at line %i", type(self).__name__, str(e), exc_traceback.tb_lineno)
                
                ## Grab the full traceback and save it to a string via StringIO
                fileObject = StringIO()
                traceback.print_tb(exc_traceback, file=fileObject)
                tbString = fileObject.getvalue()
                fileObject.close()
                ## Print the traceback to the logger as a series of DEBUG messages
                for line in tbString.split('\n'):
                    aspThreadsLogger.debug("%s", line)
                    
            ## Sleep for a bit to wait on new log entries
            time.sleep(1)
            
        # Clean up and get ready to exit
        watch_out.unregister(service.stdout)
        watch_out.unregister(service.stderr)
        try:
            service.kill()
        except OSError:
            pass
        self.service_running = False
        
    def isRunning(self):
        return self.service_running


class TemperatureSensors(object):
    """
    Class for monitoring temperature for the power supplies via the I2C interface.
    """
    
    def __init__(self, serialPort, config, logfile='/data/temp.txt', ASPCallbackInstance=None):
        self.port = port
        self.logfile = logfile
        self.updateConfig(config)
        
        # Setup the callback
        self.ASPCallbackInstance = ASPCallbackInstance
        
        # Setup temperature sensors
        self.nTemps = 0
        self.description = None
        self.temp = None
        self.lastError = None
        
        # Setup the lockout variables
        self.coldCount = 0
        self.hotCount = 0
        
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
        self.influxdb = LWAInfluxClient.from_config(config)
        
    def start(self):
        """
        Start the monitoring thread.
        """
        
        if self.thread is not None:
            self.stop()
            
        _, temps = psuTemperatureRead(self.port, 0x1F)
        self.nTemps = len(temps)
        self.description = ["UNK" for i in range(self.nTemps)]
        self.temp = [0.0 for i in range(self.nTemps)]
        self.coldCount = 0
        self.hotCount = 0
        
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
                status, temps = psuTemperatureRead(self.port, 0x1F)
                if status:
                    for i,t in enumerate(temps):
                        self.description[i] = '%s %s' % (0x1F, i+1)
                        self.temp[i] = t
                        
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
                    
                # Check the temperatures against the acceptable range
                if max(self.temp) > self.maxTemp:
                    self.hotCount += 1
                    aspThreadsLogger.warning('%s: monitorThread max. temperature of %.1f C is above the acceptable range (hot count is %i)', type(self).__name__, max(self.temp), self.hotCount)
                else:
                    self.hotCount = 0
                    
                if min(self.temp) < self.minTemp:
                    self.coldCount += 1
                    aspThreadsLogger.warning('%s: monitorThread min. temperature of %.1f C is below the acceptable range (cold count is %i)', type(self).__name__, max(self.temp), self.coldCount)
                else:
                    self.coldCount = 0
                    
                # Issue a warning if we need to
                if max(self.temp) <= self.maxTemp and max(self.temp) > self.warnTemp:
                    aspThreadsLogger.warning('%s: monitorThread max. temperature is %.1f C', type(self).__name__, max(self.temp))
                    
                # Make sure we aren't critical (on either side of good)
                if self.ASPCallbackInstance is not None and self.temp is not None:
                    if self.hotCount >= 3:
                        aspThreadsLogger.critical('%s: monitorThread max. temperature is %.1f C, notifying the system', type(self).__name__, max(self.temp))
                        
                        self.ASPCallbackInstance.processCriticalTemperature(high=True)
                        
                    if self.coldCount >= 3:
                        aspThreadsLogger.critical('%s: monitorThread min. temperature is %.1f C, notifying the system', type(self).__name__, min(self.temp))
                        
                        self.ASPCallbackInstance.processCriticalTemperature(low=True)
                        
                    if max(self.temp) > self.warnTemp:
                        self.ASPCallbackInstance.processWarningTemperature()
                    else:
                        self.ASPCallbackInstance.processWarningTemperature(clear=True)
                        
                    json = [{"measurement": "temperature",
                             "tags": {"subsystem": "asp",
                                      "monitorpoint": "temperature"},
                             "time": self.influxdb.now(),
                             "fields": {}},]
                    for i in range(self.nTemps):
                        json[0]['fields'][self.description[i].replace(' ', '_')] = self.temp[i]
                    self.influxdb.write(json)
                     
            except Exception as e:
                exc_type, exc_value, exc_traceback = sys.exc_info()
                aspThreadsLogger.error("%s: monitorThread failed with: %s at line %i", type(self).__name__, str(e), exc_traceback.tb_lineno)
                
                ## Grab the full traceback and save it to a string via StringIO
                fileObject = StringIO()
                traceback.print_tb(exc_traceback, file=fileObject)
                tbString = fileObject.getvalue()
                fileObject.close()
                ## Print the traceback to the logger as a series of DEBUG messages
                for line in tbString.split('\n'):
                    aspThreadsLogger.debug("%s", line)
                
                self.temp = None
                self.lastError = str(e)
                
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
    
    def __init__(self, port, deviceAddress, config, logfile='/data/psu.txt', ASPCallbackInstance=None):
        self.port = port
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
        self.influxdb = LWAInfluxClient.from_config(config)
        
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
                success, voltage, current, onoff, status = psuRead(self.port, self.deviceAddress)
                if success:
                    self.description = '%s - %s' % (self.port, self.deviceAddress)
                    elf.voltage = voltage
                    self.current = current
                    self.onoff = onoff
                    self.status = status
                    
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
                    for modeOfFailure in ('OverTemperature', 'OverCurrent', 'OverVolt', 'UnderVolt', 'ModuleFault'):
                        if self.status.find(modeOfFailure) != -1:
                            aspThreadsLogger.critical('%s: monitorThread PS at 0x%02X is in %s', type(self).__name__, self.deviceAddress, modeOfFailure)
                            
                            self.ASPCallbackInstance.processCriticalPowerSupply(self.deviceAddress, modeOfFailure)
                            
                json = [{"measurement": "power",
                         "tags": {"subsystem": "asp",
                                  "monitorpoint": "psu%s" % self.deviceAddress},
                         "time": self.influxdb.now(),
                         "fields": {"voltage": self.voltage,
                                    "current": self.current}},]
                json[0]['fields']['power'] = json[0]['fields']['voltage']*json[0]['fields']['current']
                self.influxdb.write(json)
                 
            except Exception as e:
                exc_type, exc_value, exc_traceback = sys.exc_info()
                aspThreadsLogger.error("%s: monitorThread 0x%02X failed with: %s at line %i", type(self).__name__, self.deviceAddress, str(e), exc_traceback.tb_lineno)
                
                ## Grab the full traceback and save it to a string via StringIO
                fileObject = StringIO()
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


class ChassisStatus(object):
    """
    Class for monitoring the configuration state of the boards to see if 
    the configuration has been lost.
    """
    
    def __init__(self, config, ASPCallbackInstance=None):
        self.updateConfig(config)
        self.configured = False
        self.board_currents = []
        self.fee_currents = []
        
        # Setup the callback
        self.ASPCallbackInstance = ASPCallbackInstance
        
        self.thread = None
        self.alive = threading.Event()
        
    def updateConfig(self, config=None):
        """
        Update the current configuration.
        """
        
        if config is None:
            return True
            
        self.monitorPeriod = config['CHASSISPERIOD']
        
    def start(self):
        """
        Start the monitoring thread.
        """
        
        if self.thread is not None:
            self.stop()
            
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
            self.configured = False
            self.lastError = None
            
    def monitorThread(self):
        """
        Create a monitoring thread for the temperature.
        """
        
        while self.alive.isSet():
            tStart = time.time()
            
            try:
                self.configured, failed = rs485Check()
                if self.ASPCallbackInstance is not None:
                    if not self.configured:
                        self.ASPCallbackInstance.processUnconfiguredChassis(failed)
                        
                status, boards, fees = rs485Power()
                if status:
                    self.board_currents = boards
                    self.fee_currents = fees
                    
            except Exception as e:
                exc_type, exc_value, exc_traceback = sys.exc_info()
                aspThreadsLogger.error("%s: monitorThread failed with: %s at line %i", type(self).__name__, str(e), exc_traceback.tb_lineno)
                
                ## Grab the full traceback and save it to a string via StringIO
                fileObject = StringIO()
                traceback.print_tb(exc_traceback, file=fileObject)
                tbString = fileObject.getvalue()
                fileObject.close()
                ## Print the traceback to the logger as a series of DEBUG messages
                for line in tbString.split('\n'):
                    aspThreadsLogger.debug("%s", line)
                    
                self.lastError = str(e)
                
            # Stop time
            tStop = time.time()
            aspThreadsLogger.debug('Finished updating chassis status in %.3f seconds', tStop - tStart)
            
            # Sleep for a bit
            sleepCount = 0
            sleepTime = self.monitorPeriod - (tStop - tStart)
            while (self.alive.isSet() and sleepCount < sleepTime):
                time.sleep(0.2)
                sleepCount += 0.2
                
    def getStatus(self):
        """
        Convenience function to get the chassis status as a string
        """
        
        if self.configured:
            return "Configured"
        else:
            return "Unconfigured"
            
    def getBoardCurrent(self, board):
        """
        Convenience function to get the current draw of a board in amps.
        """
        
        try:
            return self.board_currents[board]
        except IndexError:
            return None
            
    def getFEECurrent(self, stand):
        """
        Convenience function to get the current draw of a FEE in amps.
        """
        
        try:
            return self.fee_currents[2*(stand-1):2*(stand-1)+2]
        except IndexError:
            return (None, None)
