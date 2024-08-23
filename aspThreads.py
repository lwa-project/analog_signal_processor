# -*- coding: utf-8 -*-

"""
Module implementing the various ASP monitoring threads.
"""

import os
import sys
import time
import select
import logging
import threading
import traceback
import subprocess
from io import StringIO
    
from lwainflux import LWAInfluxClient

from aspRS485 import rs485Check, rs485GetTime, rs485SetTime, rs485Get, rs485Power, rs485RFPower, rs485Temperature


__version__ = '1.0'
__all__ = ['TemperatureSensors', 'ChassisStatus']


aspThreadsLogger = logging.getLogger('__main__')


class TemperatureSensors(object):
    """
    Class for monitoring temperature for the ARX boards via the RS485 interface.
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
            
        self.portName = config['rs485_port']
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
                # Poll the boards
                status, temps = rs485Temperature(self.portName, self.antennaMapping)
                if not status:
                    raise RuntimeError("status != True")
                    
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
                initial_hot_count = self.hotCount*1
                if max(self.temp) > self.maxTemp:
                    self.hotCount += 1
                    aspThreadsLogger.warning('%s: monitorThread max. temperature of %.1f C is above the acceptable range (hot count is %i)', type(self).__name__, max(self.temp), self.hotCount)
                else:
                    self.hotCount -= 1
                    self.hotCount = max([0, self.hotCount])
                    
                initial_cold_count = self.coldCount*1
                if min(self.temp) < self.minTemp:
                    self.coldCount += 1
                    aspThreadsLogger.warning('%s: monitorThread min. temperature of %.1f C is below the acceptable range (cold count is %i)', type(self).__name__, max(self.temp), self.coldCount)
                else:
                    self.coldCount -= 1
                    self.coldCount = max([0, self.coldCount])
                    
                # Issue a warning if we need to
                if max(self.temp) <= self.maxTemp and max(self.temp) > self.warnTemp:
                    aspThreadsLogger.warning('%s: monitorThread max. temperature is %.1f C', type(self).__name__, max(self.temp))
                    
                # Make sure we aren't critical (on either side of good)
                if self.ASPCallbackInstance is not None and self.temp is not None:
                    if self.hotCount >= 3:
                        aspThreadsLogger.critical('%s: monitorThread max. temperature is %.1f C, notifying the system', type(self).__name__, max(self.temp))
                        
                        self.ASPCallbackInstance.processCriticalTemperature(temp=max(self.temp), high=True)
                        
                    if self.coldCount >= 3:
                        aspThreadsLogger.critical('%s: monitorThread min. temperature is %.1f C, notifying the system', type(self).__name__, min(self.temp))
                        
                        self.ASPCallbackInstance.processCriticalTemperature(temp=min(self.temp), low=True)
                        
                    if (self.hotCount == 0 and initial_hot_count > 0) or (self.coldCount == 0 and initial_cold_count > 0):
                        aspThreadsLogger.info('%s: monitorThread: temperature error condition cleared (DeltaH = %i; DeltaC = %i)', type(self).__name__, self.hotCount-initial_hot_count, self.coldCount-initial_cold_count)
                        
                        self.ASPCallbackInstance.processCriticalTemperature(clear=True)
                        
                    if max(self.temp) > self.warnTemp:
                        self.ASPCallbackInstance.processWarningTemperature(temp=max(self.temp))
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


class ChassisStatus(object):
    """
    Class for monitoring the configuration state of the boards to see if 
    the configuration has been lost.
    """
    
    def __init__(self, config, ASPCallbackInstance=None):
        self.updateConfig(config)
        self.board_time = None
        self.configured = False
        self.board_currents = []
        self.fee_currents = []
        self.rf_power = []
        
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
            
        self.standsPerBoard = config['stands_per_board']
        self.portName = config['rs485_port']
        self.antennaMapping = config['antenna_mapping']
        self.maxRetry = config['max_rs485_retry']
        self.waitRetry = config['wait_rs485_retry']
        self.monitorPeriod = config['chassis_period']
        
    def start(self, board_time):
        """
        Set the board initalization time and start the monitoring thread.
        """
        
        if self.thread is not None:
            self.stop()
            
        self.board_time = board_time
        
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
            self.board_time = None
            self.configured = False
            self.lastError = None
            
    def monitorThread(self):
        """
        Create a monitoring thread for the chassis status.
        """
        
        while self.alive.isSet():
            tStart = time.time()
            
            try:
                ## Check the board time for each board.  If it gets reset then
                ## we probably have a problem
                if self.board_time is not None:
                board_times = rs485GetTime(self.portName,
                                        self.antennaMapping,
                                        maxRetry=self.maxRetry,
                                        waitRetry=self.waitRetry)
                failed = []
                for i,board_time in enumerate(board_times):
                    if board_time != self.board_time:
                        failed.append([self.standsPerBoard*i+1,self.standsPerBoard*(i+1)])
                self.configured = (len(failed) == 0)
                if self.ASPCallbackInstance is not None:
                    if len(failed) > 0:
                        self.ASPCallbackInstance.processUnconfiguredChassis(failed)
                        
                ## Record the power consumption while we are at it
                status, boards, fees = rs485Power(self.portName,
                                                  self.antennaMapping,
                                                  maxRetry=self.maxRetry,
                                                  waitRetry=self.waitRetry)
                if status:
                    self.board_currents = boards
                    self.fee_currents = fees
                    
                    try:
                        with open('/data/board-power.txt', 'a') as fh:
                            fh.write('%s,' % time.time())
                            fh.write('%.3f,' % sum(self.board_currents))
                            fh.write('%s\n' % ','.join(['%.3f' % v for v in self.board_currents]))
                    except Exception as e:
                        aspThreadsLogger.error("%s: monitorThread failed to update board power log - %s", type(self).__name__, str(e))
                        
                    try:
                        with open('/data/fee-power.txt', 'a') as fh:
                            fh.write('%s,' % time.time())
                            fh.write('%.3f,' % sum(self.fee_currents))
                            fh.write('%s\n' % ','.join(['%.3f' % v for v in self.fee_currents]))
                    except Exception as e:
                        aspThreadsLogger.error("%s: monitorThread failed to update FEE power log - %s", type(self).__name__, str(e))
                        
                ## And the square law detector power
                status, rf_power = rs485RFPower(self.portName,
                                                self.antennaMapping,
                                                maxRetry=self.maxRetry,
                                                waitRetry=self.waitRetry)
                if status:
                    self.rf_power = rf_power
                    
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
            
    def getRFPower(self, stand):
        """
        Convenience function to get the RF power from the square law detector in
        Watts.
        """
        
        try:
            return self.rf_power[2*(stand-1):2*(stand-1)+2]
        except IndexError:
            return (None, None)
