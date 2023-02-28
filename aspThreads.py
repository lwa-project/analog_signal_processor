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

from aspRS485 import rs485Check, rs485Get, rs485Power, rs485Temperature
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
        service = subprocess.Popen([os.path.join(path, 'lwaARXserial')], cwd=path, 
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
                    line = service.stdout.readline()
                    try:
                        line = line.decode()
                    except AttributeError:
                        # Python2 catch
                        pass
                    aspThreadsLogger.debug("%s: serviceThread - %s", type(self).__name__, line.rstrip())
                    while watch_out.poll(1) and self.alive.isSet():
                        line = service.stdout.readline()
                        try:
                            line = line.decode()
                        except AttributeError:
                            # Python2 catch
                            pass
                        aspThreadsLogger.debug("%s: serviceThread - %s", type(self).__name__, line.rstrip())
                        
                ## Is there anything to read on stderr?
                if watch_err.poll(1):
                    ### Ugh, read in all that we can
                    line = service.stderr.readline()
                    try:
                        line = line.decode()
                    except AttributeError:
                        # Python2 catch
                        pass
                    aspThreadsLogger.debug("%s: serviceThread - %s", type(self).__name__, line.rstrip())
                    while watch_err.poll(1) and self.alive.isSet():
                        line = service.stderr.readline()
                        try:
                            line = line.decode()
                        except AttributeError:
                            # Python2 catch
                            pass
                        aspThreadsLogger.debug("%s: serviceThread - %s", type(self).__name__, line.rstrip())
                        
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
        try:
            watch_out.unregister(service.stdout)
        except KeyError:
            pass
        try:
            watch_out.unregister(service.stderr)
        except KeyError:
            pass
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
            
        self.antennaMapping = config['antenna_mapping']
        self.monitorPeriod = config['temp_period']
        self.minTemp  = config['temp_min']
        self.warnTemp = config['temp_warn']
        self.maxTemp  = config['temp_max']
        try:
            self.influxdb = LWAInfluxClient.from_config(config)
        except ValueError:
            self.influxdb = None
            
    def start(self):
        """
        Start the monitoring thread.
        """
        
        if self.thread is not None:
            self.stop()
            
        self.nTemps = 3*len(list(self.antennaMapping.keys()))
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
        
        board_keys = list(self.antennaMapping.keys())
        
        while self.alive.isSet():
            tStart = time.time()
            
            try:
                status, temps = rs485Temperature(self.antennaMapping)
                if status:
                    for i,t in enumerate(temps):
                        board_key = board_keys[i//3]
                        self.description[i] = '%s %s' % (board_key, (i%3)+1)
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
                        
                    if self.influxdb is not None:
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
    
    def __init__(self, config, logfile='/data/power.txt', ASPCallbackInstance=None):
        logbase, logext = os.path.splitext(logfile)
        self.logfile_board = logbase+'-board'+logext
        self.logfile_fee = logbar+'-fee'+logext
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
            
        self.antennaMapping = config['antenna_mapping']
        self.monitorPeriod = config['power_period']
        try:
            self.influxdb = LWAInfluxClient.from_config(config)
        except ValueError:
            self.influxdb = None
            
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
                success, boards, fees = rs485Power(self.antennaMapping)
                if success:
                    self._boards = boards
                    self._fees = fees
                    
                try:
                    log = open(self.logfile_board, 'a+')
                    log.write('%s,' % time.time())
                    log.write('%s,' % sum(self._boards))
                    log.write('%s\n' % ','.join([str(v) for v in self._boards]))
                    log.flush()
                    log.close()
                except IOError:
                    aspThreadsLogger.error("%s: could not open flag logfile %s for writing", type(self).__name__, self.logfile_board)
                    pass
                    
                try:
                    log = open(self.logfile_fee, 'a+')
                    log.write('%s,' % time.time())
                    log.write('%s,' % sum(self._fees))
                    log.write('%s\n' % ','.join([str(v) for v in self._fees]))
                    log.flush()
                    log.close()
                except IOError:
                    aspThreadsLogger.error("%s: could not open flag logfile %s for writing", type(self).__name__, self.logfile_fees)
                    pass
                    
                # Deal with power supplies that are over temperature, current, or voltage; 
                # or under voltage; or has a module fault
                if self.ASPCallbackInstance is not None:
                    for modeOfFailure in ('OverTemperature', 'OverCurrent', 'OverVolt', 'UnderVolt', 'ModuleFault'):
                        if self.status.find(modeOfFailure) != -1:
                            aspThreadsLogger.critical('%s: monitorThread PS at 0x%02X is in %s', type(self).__name__, self.deviceAddress, modeOfFailure)
                            
                            self.ASPCallbackInstance.processCriticalPowerSupply(self.deviceAddress, modeOfFailure)
                            
                if self.influxdb is not None:
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
            
        self.antennaMapping = config['antenna_mapping']
        self.maxRetry = config['max_rs485_retry']
        self.waitRetry = config['wait_rs485_retry']
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
        
        config_failures = 0
        
        while self.alive.isSet():
            tStart = time.time()
            
            try:
                self.configured, failed = rs485Check(self.antennaMapping,
                                                     maxRetry=self.maxRetry,
                                                     waitRetry=self.waitRetry)
                if self.ASPCallbackInstance is not None:
                    if not self.configured:
                        self.ASPCallbackInstance.processUnconfiguredChassis(failed)
                        
                ## Detailed configuration check for the first 8 stands - only if
                ## we have access to the current requested configuration
                if self.ASPCallbackInstance is not None:
                    config_status = True
                    for stand in range(1, 8+1):
                        ### Configuration from ASP-MCS
                        req_config = self.ASPCallbackInstance.currentState['config'][2*(stand-1):2*(stand-1)+2]
                        ### Configuration from the board itself
                        act_config = rs485Get(stand, self.antennaMapping)
                        for pol in (0, 1):
                            for key in act_config[pol].keys():
                                if act_config[pol][key] != req_config[pol][key]:
                                    config_status = False
                                    break
                            if not config_status:
                                break
                                
                    if config_status:
                        config_failures = 0
                    else:
                        config_failures += 1
                        
                    ## If we have had more than two consecutive polling failures,
                    ## it's a problem
                    if config_failures > 1:
                        self.ASPCallbackInstance.processUnconfiguredChassis([[1, 8],])
                        
                status, boards, fees = rs485Power(self.antennaMapping,
                                                  maxRetry=self.maxRetry,
                                                  waitRetry=self.waitRetry)
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
