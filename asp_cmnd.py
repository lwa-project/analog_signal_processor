#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
asp_cmnd - Software for controlling ASP within the guidelines of the ASP and 
MCS ICDs.
"""

import os
import git
import sys
import json
import time
import signal
import socket
import string
import struct
import logging
import argparse
import json_minify
try:
    from logging.handlers import WatchedFileHandler
except ImportError:
    from logging import FileHandler as WatchedFileHandler
import traceback
from io import StringIO

from lwa_auth.tools import load_json_config

from MCS import *

from aspFunctions import  AnalogProcessor


__version__ = '0.5'
__all__ = ['DEFAULTS_FILENAME', 'MCSCommunicate']


#
# Default Configuration File
#
DEFAULTS_FILENAME = '/lwa/software/defaults.json'


class MCSCommunicate(Communicate):
    """
    Class to deal with the communcating with MCS.
    """
    
    def __init__(self, SubSystemInstance, config, opts):
            super(MCSCommunicate, self).__init__(SubSystemInstance, config, opts)
        
    def processCommand(self, data):
        """
        Interperate the data of a UDP packet as a SHL MCS command.
        """
        
        destination, sender, command, reference, datalen, mjd, mpm, data = self.parsePacket(data)
        self.logger.debug('Got command %s from %s with ref# %i', command, sender, reference)
        
        # check destination and sender
        if destination in (self.SubSystemInstance.subSystem, 'ALL'):
            # PNG
            if command == 'PNG':
                status = True
                packed_data = ''
            
            # Report various MIB entries
            elif command == 'RPT':
                status = True
                packed_data = ''
                
                ## General Info.
                if data == 'SUMMARY':
                    summary = self.SubSystemInstance.currentState['status'][:7]
                    self.logger.debug('summary = %s', summary)
                    packed_data = summary
                elif data == 'INFO':
                    ### Trim down as needed
                    if len(self.SubSystemInstance.currentState['info']) > 256:
                        infoMessage = "%s..." % self.SubSystemInstance.currentState['info'][:253]
                    else:
                        infoMessage = self.SubSystemInstance.currentState['info'][:256]
                        
                    self.logger.debug('info = %s', infoMessage)
                    packed_data = infoMessage
                elif data == 'LASTLOG':
                    ### Trim down as needed
                    if len(self.SubSystemInstance.currentState['lastLog']) > 256:
                        lastLogEntry = "%s..." % self.SubSystemInstance.currentState['lastLog'][:253]
                    else:
                        lastLogEntry =  self.SubSystemInstance.currentState['lastLog'][:256]
                    if len(lastLogEntry) == 0:
                        lastLogEntry = 'no log entry'
                    
                    self.logger.debug('lastlog = %s', lastLogEntry)
                    packed_data = lastLogEntry
                elif data == 'SUBSYSTEM':
                    self.logger.debug('subsystem = %s', self.SubSystemInstance.subSystem)
                    packed_data = self.SubSystemInstance.subSystem
                elif data == 'SERIALNO':
                    self.logger.debug('serialno = %s', self.SubSystemInstance.serialNumber)
                    packed_data = self.SubSystemInstance.serialNumber
                elif data == 'VERSION':
                    self.logger.debug('version = %s', self.SubSystemInstance.version)
                    packed_data = self.SubSystemInstance.version
                    
                ## Analog chain state - Filter
                elif data[0:7] == 'FILTER_':
                    stand = int(data[7:])
                    
                    status, filt = self.SubSystemInstance.getFilter(stand)
                    if status:
                        packed_data = str(filt)
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                    
                ## Analog chain state - Attenuators
                elif data[0:4] in ('AT1_', 'AT2_'):
                    stand = int(data[4:])
                    atten = int(data[2]) - 1
                    
                    status, attens = self.SubSystemInstance.getAttenuators(stand)
                    if status:
                        packed_data = str(attens[atten])
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                elif data[0:8] == 'ATSPLIT_':
                    stand = int(data[8:])
                    
                    status, attens = self.SubSystemInstance.getAttenuators(stand)
                    if status:
                        packed_data = str(attens[2])
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                    
                ## Analog gain state - FEE power
                elif data[0:11] in ('FEEPOL1PWR_', 'FEEPOL2PWR_'):
                    stand = int(data[11:])
                    pol = int(data[6]) - 1
                    
                    status, power = self.SubSystemInstance.getFEEPowerState(stand)
                    if status:
                        if power[pol]:
                            packed_data = 'ON '
                        else:
                            packed_data = 'OFF'
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                    
                ## Analog gain state - FEE current draw in mA
                elif data[0:11] in ('FEEPOL1CUR_', 'FEEPOL2CUR_'):
                    stand = int(data[11:])
                    pol = int(data[6]) - 1
                    
                    status, current = self.SubSystemInstance.getFEECurrentDraw(stand)
                    if status:
                        packed_data = "%.1f" % (current[pol]*1e3,)
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                    
                ## Analog gain state - RMS RF power into a 50 Ohm load
                elif data[0:6] == 'RFPWR_':
                    stand = int(data[6:])
                    
                    status, rf_power = self.SubSystemInstance.getRFPower(stand)
                    if status:
                        packed_data = "%.3f %.3f" % (rf_power[0]*1e6, rf_power[1]*1e6)
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                    
                ## ARX power supplies
                elif data == 'ARXSUPPLY':
                    status, value = self.SubSystemInstance.getARXPowerSupplyStatus()
                    if status:
                        packed_data = value
                        
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                elif data == 'ARXSUPPLY-NO':
                    status, value = self.SubSystemInstance.getARXPowerSupplyCount()
                    if status:
                        packed_data = (str(value))[:2]
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = %s' % (data, packed_data))
                elif data[0:11] == 'ARXPWRUNIT_':
                    psNumb = int(data[11:])
                    
                    status, value = self.SubSystemInstance.getARXPowerSupplyInfo(psNumb)
                    if status:
                        packed_data = value[:256]
                        
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                elif data == 'ARXCURR':
                    status, value = self.SubSystemInstance.getARXPowerSupplyCurrentDraw()
                    if status:
                        packed_data = "%-7i" % value
                        
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                elif data == 'ARXVOLT':
                    status, value = self.SubSystemInstance.getARXPowerSupplyVoltage()
                    if status:
                        packed_data = "%-7.3f" % value
                        
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                
                ## FEE power supplies
                elif data == 'FEESUPPLY':
                    status, value = self.SubSystemInstance.getFEEPowerSupplyStatus()
                    if status:
                        packed_data = value
                        
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                elif data == 'FEESUPPLY-NO':
                    status, value = self.SubSystemInstance.getFEEPowerSupplyCount()
                    if status:
                        packed_data = (str(value))[:2]
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = %s' % (data, packed_data))
                elif data[0:11] == 'FEEPWRUNIT_':
                    psNumb = int(data[11:])
                    
                    status, value = self.SubSystemInstance.getFEEPowerSupplyInfo(psNumb)
                    if status:
                        packed_data = value[:256]
                        
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                elif data == 'FEECURR':
                    status, value = self.SubSystemInstance.getFEEPowerSupplyCurrentDraw()
                    if status:
                        packed_data = "%-7i" % value
                        
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                elif data == 'FEEVOLT':
                    status, value = self.SubSystemInstance.getFEEPowerSupplyVoltage()
                    if status:
                        packed_data = "%-7.3f" % value
                        
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                    
                ## Temperatue sensors
                elif data == 'TEMP-STATUS':
                    status, value = self.SubSystemInstance.getTemperatureStatus()
                    if status:
                        packed_data = value[:256]
                        
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                elif data == 'TEMP-SENSE-NO':
                    status, value = self.SubSystemInstance.getTempSensorCount()
                    if status:
                        packed_data = (str(value))[:3]
                        
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = %s' % (data, packed_data))
                elif data[0:12] == 'SENSOR-NAME-':
                    sensorNumb = int(data[12:])
                    
                    status, value = self.SubSystemInstance.getTempSensorInfo(sensorNumb)
                    if status:
                        packed_data = value[:256]
                        
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                elif data[0:12] == 'SENSOR-DATA-':
                    sensorNumb = int(data[12:])
                    
                    status, value = self.SubSystemInstance.getTempSensorData(sensorNumb)
                    if status:
                        packed_data = "%-10.3f" % value
                        
                    else:
                        packed_data = self.SubSystemInstance.currentState['lastLog']
                        
                    self.logger.debug('%s = exited with status %s', data, str(status))
                
                else:
                    status = False
                    packed_data = 'Unknown MIB entry: %s' % data
                    
                    self.logger.debug('%s = exited with status %s', data, str(status))
            
            #
            # Control Commands
            #
            
            # INI
            elif command == 'INI':
                # Re-read in the configuration file
                config = load_json_config(self.opts.config)
                
                # Refresh the configuration for the communicator and ASP
                self.updateConfig(config)
                self.SubSystemInstance.updateConfig(config)
                
                # Go
                nBoards = int(data)
                status, exitCode = self.SubSystemInstance.ini(nBoards)
                if status:
                    packed_data = ''
                else:
                    packed_data = "0x%02X! %s" % (exitCode, self.SubSystemInstance.currentState['lastLog'])
            
            # SHT
            elif command == 'SHT':
                status, exitCode = self.SubSystemInstance.sht(mode=data)
                if status:
                    packed_data = ''
                else:
                    packed_data = "0x%02X! %s" % (exitCode, self.SubSystemInstance.currentState['lastLog'])
                    
            # FIL
            elif command == 'FIL':
                stand = int(data[:-2])
                filterCode = int(data[-2:])
                
                status, exitCode = self.SubSystemInstance.setFilter(stand, filterCode)
                if status:
                    packed_data = ''
                else:
                    packed_data = "0x%02X! %s" % (exitCode, self.SubSystemInstance.currentState['lastLog'])
                    
            # AT1
            elif command == 'AT1':
                mode = 1
                stand = int(data[:-2])
                attenSetting = int(data[-2:])
                
                status, exitCode = self.SubSystemInstance.setAttenuator(mode, stand, attenSetting)
                if status:
                    packed_data = ''
                else:
                    packed_data = "0x%02X! %s" % (exitCode, self.SubSystemInstance.currentState['lastLog'])
                    
            # AT2
            elif command == 'AT2':
                mode = 2
                stand = int(data[:-2])
                attenSetting = int(data[-2:])
                
                status, exitCode = self.SubSystemInstance.setAttenuator(mode, stand, attenSetting)
                if status:
                    packed_data = ''
                else:
                    packed_data = "0x%02X! %s" % (exitCode, self.SubSystemInstance.currentState['lastLog'])
                    
            # ATS
            elif command == 'ATS':
                mode = 3
                stand = int(data[:-2])
                attenSetting = int(data[-2:])
                
                status, exitCode = self.SubSystemInstance.setAttenuator(mode, stand, attenSetting)
                if status:
                    packed_data = ''
                else:
                    packed_data = "0x%02X! %s" % (exitCode, self.SubSystemInstance.currentState['lastLog'])
                    
            # FPW
            elif command == 'FPW':
                stand = int(data[:-3])
                pol = int(data[-3])
                state = int(data[-2:])
                
                status, exitCode = self.SubSystemInstance.setFEEPowerState(stand, pol, state)
                if status:
                    packed_data = ''
                else:
                    packed_data = "0x%02X! %s" % (exitCode, self.SubSystemInstance.currentState['lastLog'])
                    
            # RXP
            elif command == 'RXP':
                state = int(data)
                
                status, exitCode = self.SubSystemInstance.setARXPowerState(state)
                if status:
                    packed_data = ''
                else:
                    packed_data = "0x%02X! %s" % (exitCode, self.SubSystemInstance.currentState['lastLog'])
                    
            elif command == 'FEP':
                state = int(data)
                
                status, exitCode = self.SubSystemInstance.setFPWPowerState(state)
                if status:
                    packed_data = ''
                else:
                    packed_data = "0x%02X! %s" % (exitCode, self.SubSystemInstance.currentState['lastLog'])
                    
            # 
            # Unknown command catch
            #

            else:
                status = False
                self.logger.debug('%s = error, unknown command', command)
                packed_data = 'Unknown command: %s' % command

            # Return status, command, reference, and the result
            return sender, status, command, reference, packed_data        


def main(args):
    """
    Main function of asp_cmnd.py.  This sets up the various configuation options 
    and start the UDP command handler.
    """
    
    # Setup logging
    logger = logging.getLogger(__name__)
    logFormat = logging.Formatter('%(asctime)s [%(levelname)-8s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
    logFormat.converter = time.gmtime
    if args.log is None:
        logHandler = logging.StreamHandler(sys.stdout)
    else:
        logHandler = WatchedFileHandler(args.log)
    logHandler.setFormatter(logFormat)
    logger.addHandler(logHandler)
    if args.debug:
        logger.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.INFO)
    
    # Get current MJD and MPM
    mjd, mpm = getTime()
    
    # Git information
    try:
        repo = git.Repo(os.path.basename(os.path.abspath(__file__)))
        branch = repo.active_branch.name
        hexsha = repo.active_branch.commit.hexsha
        shortsha = hexsha[-7:]
        dirty = ' (dirty)' if repo.is_dirty() else ''
    except git.exc.GitError:
        branch = 'unknown'
        hexsha = 'unknown'
        shortsha = 'unknown'
        dirty = ''
        
    # Report on who we are
    logger.info('Starting asp_cmnd.py with PID %i', os.getpid())
    logger.info('Version: %s', __version__)
    logger.info('Revision: %s.%s%s', branch, shortsha, dirty)
    logger.info('Current MJD: %i', mjd)
    logger.info('Current MPM: %i', mpm)
    logger.info('All dates and times are in UTC except where noted')
    
    # Read in the configuration file
    config = load_json_config(args.config)
    
    # Setup ASP control
    lwaASP = AnalogProcessor(config)

    # Setup the communications channels
    mcsComms = MCSCommunicate(lwaASP, config, args)
    mcsComms.start()

    # Setup handler for SIGTERM so that we aren't left in a funny state
    def HandleSignalExit(signum, frame, logger=logger, MCSInstance=mcsComms):
        logger.info('Exiting on signal %i', signum)

        # Shutdown ASP and close the communications channels
        tStop = time.time()
        logger.info('Shutting down ASP, please wait...')
        MCSInstance.SubSystemInstance.sht(mode='SCRAM')
        while MCSInstance.SubSystemInstance.currentState['info'] != 'System has been shut down':
            time.sleep(5)
            MCSInstance.SubSystemInstance.sht(mode='SCRAM')
        logger.info('Shutdown completed in %.3f seconds', time.time() - tStop)
        MCSInstance.stop()
        
        # Exit
        logger.info('Finished')
        logging.shutdown()
        sys.exit(0)
    
    # Hook in the signal handler - SIGTERM
    signal.signal(signal.SIGTERM, HandleSignalExit)
    
    # Loop and process the MCS data packets as they come in - exit if ctrl-c is 
    # received
    logger.info('Ready to communicate')
    while True:
        try:
            mcsComms.receiveCommand()
            
        except KeyboardInterrupt:
            logger.info('Exiting on ctrl-c')
            break
            
        except Exception as e:
            exc_type, exc_value, exc_traceback = sys.exc_info()
            logger.error("asp_cmnd.py failed with: %s at line %i", str(e), exc_traceback.tb_lineno)
                
            ## Grab the full traceback and save it to a string via StringIO
            fileObject = StringIO()
            traceback.print_tb(exc_traceback, file=fileObject)
            tbString = fileObject.getvalue()
            fileObject.close()
            ## Print the traceback to the logger as a series of DEBUG messages
            for line in tbString.split('\n'):
                logger.debug("%s", line)
                
    # If we've made it this far, we have finished so shutdown ASP and close the 
    # communications channels
    tStop = time.time()
    print('\nShutting down ASP, please wait...')
    logger.info('Shutting down ASP, please wait...')
    lwaASP.sht()
    time.sleep(5)
    while lwaASP.currentState['info'] != 'System has been shut down':
        time.sleep(5)
        lwaASP.sht()
    logger.info('Shutdown completed in %.3f seconds', time.time() - tStop)
    mcsComms.stop()
    
    # Exit
    logger.info('Finished')
    logging.shutdown()
    sys.exit(0)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description='control the ASP sub-system within the guidelines of the ASP and MCS ICDs',
        formatter_class=argparse.ArgumentDefaultsHelpFormatter
        )
    parser.add_argument('-c', '--config', type=str, default=DEFAULTS_FILENAME,
                        help='name of the ASP configuration file to use')
    parser.add_argument('-l', '--log', type=str,
                        help='name of the logfile to write logging information to')
    parser.add_argument('-d', '--debug', action='store_true',
                        help='print debug messages as well as info and higher')
    args = parser.parse_args()
    main(args)
    
