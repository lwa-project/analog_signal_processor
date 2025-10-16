
"""
Module implementing the various ASP monitoring threads.
"""

import os
import sys
import time
import logging
import threading
import traceback
import subprocess
try:
    from cStringIO import StringIO
except ImportError:
    from io import StringIO

from aspSUB20 import *


__version__ = '0.7'
__all__ = ['TemperatureSensors', 'PowerStatus', 'ChassisStatus']


aspThreadsLogger = logging.getLogger('__main__')


class TemperatureSensors(object):
    """
    Class for monitoring temperature for the power supplies via the I2C interface.
    """
    
    def __init__(self, sub20SN, config, logfile='/data/temp.txt', ASPCallbackInstance=None):
        self.sub20SN = str(sub20SN)
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
            
        self.monitorPeriod = config['temp_period']
        self.minTemp  = config['temp_min']
        self.warnTemp = config['temp_warn']
        self.maxTemp  = config['temp_max']
        
    def start(self):
        """
        Start the monitoring thread.
        """
        
        if self.thread is not None:
            self.stop()
            
        self.nTemps = psuCountTemperature(self.sub20SN)
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
                temps = psuTemperature(self.sub20SN)
                if temps:
                    missingSUB20 = False
                    
                    for i,entry in enumerate(temps):
                        self.description[i] = '%s %s' % (entry['address'], entry['description'])
                        self.temp[i] = entry['temp_C']
                else:
                    missingSUB20 = True
                    
                # Open the log file and save the temps
                try:
                    with open(self.logfile, 'a+') as log:
                        log.write('%s,' % time.time())
                        log.write('%s\n' % ','.join(["%.2f" % t for t in self.temp]))
                        log.flush()
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
                    if missingSUB20:
                        self.ASPCallbackInstance.processMissingSUB20()
                        
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
                
                self.temp = [None for temp in self.temp]
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
    
    def __init__(self, sub20SN, deviceAddress, config, logfile='/data/psu.txt', ASPCallbackInstance=None):
        self.sub20SN = str(sub20SN)
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
            
        self.monitorPeriod = config['power_period']
        
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
                data = psuRead(self.sub20SN, self.deviceAddress)
                if data:
                    missingSUB20 = False
                    
                    self.description = '%s - %s' % (data['address'], data['description'])
                    self.voltage = data['voltage']
                    self.current = data['current']
                    self.onoff = data['onoff']
                    self.status = data['status']
                else:
                    missingSUB20 = True
                    
                    self.voltage = 0.0
                    self.current = 0.0
                    self.onoff = "UNK"
                    self.status = "UNK"
                    self.lastError = 'No data returned'
                    
                try:
                    with open(self.logfile, 'a+') as log:
                        log.write('%s,' % time.time())
                        log.write('%s\n' % ','.join(["%.2f" % self.voltage, "%.3f" % self.current, self.onoff, self.status]))
                        log.flush()
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
    
    def __init__(self, sub20SN, config, temp_logfile='/data/board-temp.txt',
                       fee_logfile='/data/fee-power.txt', pic_monitoring=True, 
                       ASPCallbackInstance=None):
        self.sub20SN = str(sub20SN)
        self.register = 0x000C
        self.updateConfig(config)
        self.temp_logfile = temp_logfile
        self.fee_logfile = fee_logfile
        self.pic_monitoring = pic_monitoring
        
        # SPI setup and data variables
        self._spi = SPIProcessingThread(self.spi_mini_mapping)
        self.configured = False
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
            
        self.spi_mini_mapping = {self.sub20SN: config['sub20_antenna_mapping'][self.sub20SN]}
        self.rs485_mapping = config['sub20_rs485_mapping']
        self.monitorPeriod = config['chassis_period']
        
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
        
        loop_counter = 0
        
        while self.alive.isSet():
            tStart = time.time()
            
            try:
                resp = self._spi.read_register(1, self.register)
                if resp is not None:
                    missingSUB20 = False
                    
                    if resp == (self.register | 0x5500):
                        self.configured = True
                    else:
                        self.configured = False
                        
                        aspThreadsLogger.error("%s: SUB-20 S/N %s lost SPI port configuation", type(self).__name__, self.sub20SN)
                else:
                    missingSUB20 = True
                    
                    self.configured = False
                    
                if self.ASPCallbackInstance is not None:
                    if missingSUB20:
                        self.ASPCallbackInstance.processMissingSUB20()
                        
                    if not self.configured:
                        self.ASPCallbackInstance.processUnconfiguredChassis(self.sub20SN)
                        
                ## Record the board temperatures and power consumption while we are at it
                if self.pic_monitoring and loop_counter == 0:
                    #status, temps = rs485Temperature(self.rs485_mapping, maxRetry=MAX_RS485_RETRY)
                    status, temps = False, []
                    
                    if status:
                        try:
                            with open(self.temp_logfile, 'a') as log:
                                log.write('%s,' % time.time())
                                log.write('%s\n' % ','.join(['%.2f' % t for t in temps]))
                                log.flush()
                        except Exception as e:
                            aspThreadsLogger.error("%s: monitorThread failed to update board temperature log - %s", type(self).__name__, str(e))
                            
                    status, fees = rs485Power(self.rs485_mapping, maxRetry=MAX_RS485_RETRY)
                        
                    if status:
                        self.fee_currents = fees
                        
                        try:
                            with open(self.fee_logfile, 'a') as log:
                                log.write('%s,' % time.time())
                                log.write('%s\n' % ','.join(['%.3f' % v for v in self.fee_currents]))
                                log.flush()
                        except Exception as e:
                            aspThreadsLogger.error("%s: monitorThread failed to update FEE power log - %s", type(self).__name__, str(e))
                            
            except Exception as e:
                exc_type, exc_value, exc_traceback = sys.exc_info()
                aspThreadsLogger.error("%s: monitorThread SUB-20 S/N %s failed with: %s at line %i", type(self).__name__, self.sub20SN, str(e), exc_traceback.tb_lineno)
                
                ## Grab the full traceback and save it to a string via StringIO
                fileObject = StringIO()
                traceback.print_tb(exc_traceback, file=fileObject)
                tbString = fileObject.getvalue()
                fileObject.close()
                ## Print the traceback to the logger as a series of DEBUG messages
                for line in tbString.split('\n'):
                    aspThreadsLogger.debug("%s", line)
                    
                self.lastError = str(e)
                
            loop_counter += 1
            loop_counter %= 3
            
            # Stop time
            tStop = time.time()
            aspThreadsLogger.debug('Finished updating chassis status for SUB-20 S/N %s in %.3f seconds', self.sub20SN, tStop - tStart)
            
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
