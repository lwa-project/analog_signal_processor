# -*- coding: utf-8 -*-
"""
Module for storing the miscellaneous functions used by asp_cmnd for running ASP.
"""

import os
import time
import ctypes
import logging
import threading

from aspRS485 import *
from aspI2C import *
from aspThreads import *


__version__ = '0.6'
__all__ = ['modeDict', 'commandExitCodes', 'AnaloglProcessor']


aspFunctionsLogger = logging.getLogger('__main__')


modeDict = {1: 'AT1', 2: 'AT2', 3: 'ATS'}


commandExitCodes = {0x00: 'Process accepted without error', 
                    0x01: 'Invalid number of ARX boards', 
                    0x02: 'Invalid stand', 
                    0x03: 'Invalid polarization', 
                    0x04: 'Invalid filter code', 
                    0x05: 'Invalid attenuator setting', 
                    0x06: 'Invalid power setting', 
                    0x07: 'Invalid command arguments', 
                    0x08: 'Blocking operation in progress', 
                    0x09: 'Subsytem already initalized', 
                    0x0A: 'Subsystem needs to be initialized', 
                    0x0B: 'Command not implemented'}

subsystemErrorCodes = {0x00: 'Subsystem operating normally', 
                       0x01: 'PS over temperature', 
                       0x02: 'PS under temperature', 
                       0x03: 'PS over voltage', 
                       0x04: 'PS under voltage', 
                       0x05: 'PS over current', 
                       0x06: 'PS module fault error',
                       0x07: 'Failed to process SPI commands',
                       0x08: 'Failed to process I2C commands', 
                       0x09: 'Board count mis-match',
                       0x0A: 'Temperature over TempMax',
                       0x0B: 'Temperature under TempMin',
                       0x0C: 'Power supplies off',
                       0x0D: 'Temperature warning'}


class AnalogProcessor(object):
    """
    Class for interacting with the Analog Signal Processor subsystem.
    
    A note about exit codes from control commands (FIL, AT1, etc.):
      * See commandExitCodes
    """
    
    def __init__(self, config):
        self.config = config
        
        # ASP system information
        self.subSystem = 'ASP'
        self.serialNumber = self.config['serial_number']
        self.version = str(__version__)
        
        # ASP system state
        self.currentState = {}
        self.currentState['status'] = 'SHUTDWN'
        self.currentState['info'] = 'Need to INI ASP'
        self.currentState['lastLog'] = 'Welcome to ASP S/N %s, version %s' % (self.serialNumber, self.version)
        
        ## Operational state
        self.currentState['ready'] = False
        self.currentState['activeProcess'] = []
        
        ## Operational state - ASP
        self.currentState['config']  = [{} for i in range(2*self.config['max_boards']*self.config['stands_per_board'])]
        
        ## Monitoring and background threads
        self.currentState['serviceThread'] = None
        self.currentState['tempThread'] = None
        self.currentState['chassisThreads'] = None
        
        # Board and stand counts
        self.num_boards = 0
        self.num_stands = 0
        self.num_chpairs = 0
        
        # Update the configuration
        self.updateConfig()
        
    def updateConfig(self, config=None):
        """
        Update the stored configuration.
        """
        
        if config is not None:
            self.config = config
        return True
        
    def getState(self):
        """
        Return the current system state as a dictionary.
        """
        
        return self.currentState
        
    def __getStandConfig(self, stand):
        if stand == 0:
            return self.currentState['config']
        else:
            return self.currentState['config'][2*(stand-1)+0:2*(stand-1)+2]
            
    def ini(self, nBoards, config=None):
        """
        Initialize ASP (in a seperate thread).
        """
        
        # Check for other operations in progress that ccould be blocking (INI or SHT)
        if 'INI' in self.currentState['activeProcess'] or 'SHT' in self.currentState['activeProcess']:
            aspFunctionsLogger.warning("INI command rejected due to process list %s", ' '.join(self.currentState['activeProcess']))
            self.currentState['lastLog'] = 'INI: %s - %s is active and blocking' % (commandExitCodes[0x08], self.currentState['activeProcess'])
            return False, 0x08
            
        # Check to see if there is a valid number of boards
        if nBoards < 0 or nBoards > self.config['max_boards']:
            aspFunctionsLogger.warning("INI command rejected due to invalid board count")
            self.currentState['lastLog'] = 'INI: %s' % commandExitCodes[0x01]
            return False, 0x01
            
        # Update the configuration
        self.updateConfig(config=config)
        
        # Start the process in the background
        thread = threading.Thread(target=self.__iniProcess, args=(nBoards,))
        thread.setDaemon(1)
        thread.start()
        
        return True, 0
        
    def __iniProcess(self, nBoards):
        """
        Thread base to initialize ASP.  Update the current system state as needed.
        """
        
        # Start the timer
        tStart = time.time()
        
        # Update system state
        self.currentState['ready'] = False
        self.currentState['status'] = 'BOOTING'
        self.currentState['info'] = 'Running INI sequence'
        self.currentState['activeProcess'].append('INI')
        
        # Stop the backend service thread.  If it doesn't exist create it
        if self.currentState['serviceThread'] is not None:
            self.currentState['serviceThread'].stop()
        else:
            self.currentState['serviceThread'] = BackendService(ASPCallbackInstance=self)
            
        # Start the backend service thread
        self.currentState['serviceThread'].start()
        
        # Make sure the RS485 is present
        if os.system('lsusb -d 10c4: >/dev/null') == 0:
            # Good, we can continue
            
            # Board check - found vs. expected from INI
            boardsFound = rs485CountBoards(self.config['antenna_mapping'],
                                           maxRetry=self.config['max_rs485_retry'],
                                           waitRetry=self.config['wait_rs485_retry'])
            if boardsFound == nBoards:
                # Board and stand counts.  NOTE: Stand counts are capped at 260
                self.num_boards = nBoards
                self.num_stands = nBoards * self.config['stands_per_board']
                if self.num_stands > self.config['max_stands']:
                    self.num_stands = self.config['max_stands']
                self.num_chpairs = nBoards * self.config['stands_per_board']
                aspFunctionsLogger.info('Starting ASP with %i boards (%i stands)', self.num_boards, self.num_stands)
                    
                # Stop the non-service threads.  If the don't exist yet, create them.
                if self.currentState['tempThread'] is not None:
                    self.currentState['tempThread'].stop()
                    self.currentState['tempThread'].updateConfig(self.config)
                else:
                    self.currentState['tempThread'] = TemperatureSensors(self.config, ASPCallbackInstance=self)
                if self.currentState['chassisThreads'] is not None:
                    for t in self.currentState['chassisThreads']:
                        t.stop()
                        t.updateConfig(self.config)
                else:
                    self.currentState['chassisThreads'] = []
                    self.currentState['chassisThreads'].append( ChassisStatus(self.config, ASPCallbackInstance=self) )
                    
                # Do the RS485 bus stuff
                status = rs485Reset(self.config['antenna_mapping'],
                                    maxRetry=self.config['max_rs485_retry'],
                                    waitRetry=self.config['wait_rs485_retry'])
                
                # Update the analog signal chain state
                self.currentState['config'] = rs485Get(0, self.config['antenna_mapping'],
                                                       maxRetry=self.config['max_rs485_retry'],
                                                       waitRetry=self.config['wait_rs485_retry'])
                
                # Start the non-service threads
                self.currentState['tempThread'].start()
                for t in self.currentState['chassisThreads']:
                    t.start()
                    
                if status:
                    self.currentState['status'] = 'NORMAL'
                    self.currentState['info'] = 'System operating normally'
                    self.currentState['lastLog'] = 'INI: finished in %.3f s' % (time.time() - tStart,)
                    self.currentState['ready'] = True
                    
                else:
                    self.currentState['status'] = 'ERROR'
                    self.currentState['info'] = 'SUMMARY! 0x%02X %s - Failed after %i attempts' % (0x07, subsystemErrorCodes[0x07], self.config['max_rs485_retry'])
                    self.currentState['lastLog'] = 'INI: finished with error'
                    self.currentState['ready'] = False
                    
                    aspFunctionsLogger.critical("INI failed sending SPI bus commands after %i attempts", self.config['max_rs485_retry'])
            else:
                self.currentState['status'] = 'ERROR'
                self.currentState['info'] = 'SUMMARY! 0x%02X %s - Found %i boards, expected %i' % (0x09, subsystemErrorCodes[0x09], boardsFound, nBoards)
                self.currentState['lastLog'] = 'INI: finished with error'
                self.currentState['ready'] = False
                
                aspFunctionsLogger.critical("INI failed; found %i boards, expected %i", boardsFound, nBoards)
                
        else:
            # Oops, the SUB-20 is missing...
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'SUMMARY! 0x%02X %s - SUB-20 device not found' % (0x07, subsystemErrorCodes[0x07])
            self.currentState['lastLog'] = 'INI: finished with error'
            self.currentState['ready'] = False
            
            aspFunctionsLogger.critical("INI failed due to missing SUB-20 device(s)")
        
        # Update the current state
        aspFunctionsLogger.info("Finished the INI process in %.3f s", time.time() - tStart)
        self.currentState['activeProcess'].remove('INI')
        
        return True, 0
    
    def sht(self, mode=''):
        """
        Issue the SHT command to ASP.
        """
        
        # Check for other operations in progress that could be blocking (INI and SHT)
        if 'INI' in self.currentState['activeProcess'] or 'SHT' in self.currentState['activeProcess']:
            self.currentState['lastLog'] = 'SHT: %s - %s is active and blocking' % (commandExitCodes[0x08], self.currentState['activeProcess'])
            return False, 0x08
        
        # Validate SHT options
        if mode not in ("", "SCRAM", "RESTART", "SCRAM RESTART"):
            self.currentState['lastLog'] = 'SHT: %s - unknown mode %s' % (commandExitCodes[0x07], mode)
            return False, 0x07
            
        thread = threading.Thread(target=self.__shtProcess, kwargs={'mode': mode})
        thread.setDaemon(1)
        thread.start()
        return True, 0
        
    def __shtProcess(self, mode=""):
        """
        Thread base to shutdown ASP.  Update the current system state as needed.
        """
        
        # Start the timer
        tStart = time.time()
        
        # Update system state
        self.currentState['status'] = 'SHUTDWN'
        self.currentState['info'] = 'System is shutting down'
        self.currentState['activeProcess'].append('SHT')
        self.currentState['ready'] = False
        
        # Stop all threads except for the service thread.
        if self.currentState['tempThread'] is not None:
            self.currentState['tempThread'].stop()
        if self.currentState['chassisThreads'] is not None:
            for t in self.currentState['chassisThreads']:
                t.stop()
                
        status = True
        if status:
            self.currentState['status'] = 'SHUTDWN'
            self.currentState['info'] = 'System has been shut down'
            self.currentState['lastLog'] = 'System has been shut down'
            
        else:
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'SUMMARY! 0x%02X %s - Failed after %i attempts' % (0x07, subsystemErrorCodes[0x07], self.config['max_rs485_retry'])
            self.currentState['lastLog'] = 'SHT: failed in %.3f s' % (time.time() - tStart,)
            self.currentState['ready'] = False
            
            aspFunctionsLogger.critical("SHT failed sending SPI bus commands after %i attempts", self.config['max_rs485_retry'])
        
        # Stop the service thread
        if self.currentState['serviceThread'] is not None:
            self.currentState['serviceThread'].stop()
            
        # Update the current state
        aspFunctionsLogger.info("Finished the SHT process in %.3f s", time.time() - tStart)
        self.currentState['activeProcess'].remove('SHT')
        
        return True, 0
        
    def setFilter(self, stand, filterCode):
        """
        Set the filter on a given stand.
        """
        
        # Check the operational status of the system
        if self.currentState['status'] == 'SHUTDWN' or not self.currentState['ready']:
            self.currentState['lastLog'] = 'FIL: %s' % commandExitCodes[0x0A]
            return False, 0x0A
        if 'FIL' in self.currentState['activeProcess']:
            self.currentState['lastLog'] = 'FIL: %s' % commandExitCodes[0x08]
            return False, 0x08
            
        # Validate inputs
        if stand < 0 or stand > self.num_stands:
            self.currentState['lastLog'] = 'FIL: %s' % commandExitCodes[0x02]
            return False, 0x02
        if filterCode < 0 or filterCode > 5:
            self.currentState['lastLog'] = 'FIL: %s' % commandExitCodes[0x04]
            return False, 0x04
            
        # Block other FIL requests
        self.currentState['activeProcess'].append('FIL')
        
        # Process in the background
        thread = threading.Thread(target=self.__filProcess, args=(stand, filterCode))
        thread.setDaemon(1)
        thread.start()
        
        return True, 0
        
    def __filProcess(self, stand, filterCode):
        """
        Background process for FIL commands so that other commands can keep on running.
        """
        
        # Do RS485 bus stuff
        status = True
        config = self.__getStandConfig(stand)
        for c in config:
            if filterCode > 3:
                # Set 3 MHz mode
                c['narrow_lpf'] = True
                c['sig_on'] = True
            else:
                # Set 10 MHz mode
                c['narrow_lpf'] = False
                c['sig_on'] = True
                
            if filterCode == 0 or filterCode == 4:
                # Set Filter to Split Bandwidth
                c['narrow_hpf'] = True
                c['sig_on'] = True
            elif filterCode == 1 or filterCode == 5:
                # Set Filter to Full Bandwidth
                c['narrow_hpf'] = False
                c['sig_on'] = True
            elif filterCode == 2:
                # Set Filter to Reduced Bandwidth
                c['narrow_hpf'] = True
                c['sig_on'] = True
            elif filterCode == 3:
                # Set Filters OFF
                c['sig_on'] = False
        status = rs485Send(stand, config, self.config['antenna_mapping'],
                           maxRetry=self.config['max_rs485_retry'],
                           waitRetry=self.config['wait_rs485_retry'])
        
        if status:
            self.currentState['lastLog'] = 'FIL: Set filter to %02i for stand %i' % (filterCode, stand)
            aspFunctionsLogger.debug('FIL - Set filter to %02i for stand %i', filterCode, stand)
        
            if stand == 0:
                self.currentState['config'] = config
            else:
                self.currentState['config'][2*(stand-1)+0] = config[0]
                self.currentState['config'][2*(stand-1)+1] = config[1]
        else:
            # Something failed, report
            self.currentState['lastLog'] = 'FIL: Failed to set filter to %02i for stand %i' % (filterCode, stand)
            aspFunctionsLogger.error('FIL - Failed to set filter to %02i for stand %i', filterCode, stand)
            
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'SUMMARY! 0x%02X %s - Failed after %i attempts' % (0x07, subsystemErrorCodes[0x07], self.config['max_rs485_retry'])
            self.currentState['ready'] = False
            
        # Cleanup and save the state of FIL
        self.currentState['activeProcess'].remove('FIL')
        
        return True, 0
        
    def setAttenuator(self, mode, stand, attenSetting):
        """
        Set one of the attenuators for a given stand.  The attenuators are:
          1. AT1
          2. AT2
          3. ATS
        """
        
        # Check the operational status of the system
        if self.currentState['status'] == 'SHUTDWN'or not self.currentState['ready']:
            self.currentState['lastLog'] = '%s: %s' % (modeDict[mode], commandExitCodes[0x0A])
            return False, 0x0A
        if 'ATN' in self.currentState['activeProcess']:
            self.currentState['lastLog'] = '%s: %s' % (modeDict[mode], commandExitCodes[0x08])
            return False, 0x08
            
        # Validate inputs
        if mode  == 3:
            self.currentState['lastLog'] = '%s: %s' % (modeDict[mode], 'ATS Setting Depreciated')
            return False, 0x05
        if stand < 0 or stand > self.num_stands:
            self.currentState['lastLog'] = '%s: %s' % (modeDict[mode], commandExitCodes[0x02])
            return False, 0x02
        if attenSetting < 0 or attenSetting > self.config['max_atten']:
            self.currentState['lastLog'] = '%s: %s' % (modeDict[mode], commandExitCodes[0x05])
            return False, 0x05
            
        # Block other FIL requests
        self.currentState['activeProcess'].append('ATN')
        
        # Process in the background
        thread = threading.Thread(target=self.__atnProcess, args=(mode, stand, attenSetting))
        thread.setDaemon(1)
        thread.start()
        
        return True, 0
    
    def __atnProcess(self, mode, stand, attenSetting):
        """
        Background process for AT1/AT2/ATS commands so that other commands can keep on running.
        """
        
        # Do RS485 bus stuff
        setting = 2*attenSetting
        setting = int(round(setting*2))*0.5
        key = 'first_atten'
        if mode == 2:
            key = 'second_atten'
        else:
            key = 'split_atten'
            
        config = self.__getStandConfig(stand)
        for c in config:
            c[key] = setting
        status = rs485Send(stand, config, self.config['antenna_mapping'],
                           maxRetry=self.config['max_rs485_retry'],
                           waitRetry=self.config['wait_rs485_retry'])
        
        if status:
            self.currentState['lastLog'] = '%s: Set attenuator to %02i for stand %i' % (modeDict[mode], attenSetting, stand)
            aspFunctionsLogger.debug('%s - Set attenuator to %02i for stand %i', modeDict[mode], attenSetting, stand)
            
            if stand == 0:
                self.currentState['config'] = config
            else:
                self.currentState['config'][2*(stand-1)+0] = config[0]
                self.currentState['config'][2*(stand-1)+1] = config[1]
        else:
            # Something failed, report
            self.currentState['lastLog'] = '%s: Failed to set attenuator to %02i for stand %i' % (modeDict[mode], attenSetting, stand)
            aspFunctionsLogger.error('%s - Failed to set attenuator to %02i for stand %i', modeDict[mode], attenSetting, stand)
            
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'SUMMARY! 0x%02X %s - Failed after %i attempts' % (0x07, subsystemErrorCodes[0x07], self.config['max_rs485_retry'])
            self.currentState['ready'] = False
            
        # Cleanup
        self.currentState['activeProcess'].remove('ATN')
        
        return True, 0
        
    def setFEEPowerState(self, stand, pol, state):
        """
        Set the FEE power state for a given stand/pol.
        """
        
        # Check the operational status of the system
        if self.currentState['status'] == 'SHUTDWN'or not self.currentState['ready']:
            self.currentState['lastLog'] = 'FPW: %s' % commandExitCodes[0x0A]
            return False, 0x0A
        if 'FPW' in self.currentState['activeProcess']:
            self.currentState['lastLog'] = 'FPW: %s' % commandExitCodes[0x08]
            return False, 0x08
            
        # Validate inputs
        if stand < 0 or stand > self.num_stands:
            self.currentState['lastLog'] = 'FPW: %s' % commandExitCodes[0x02]
            return False, 0x02
        if pol < 1 or pol > 2:
            self.currentState['lastLog'] = 'FPW: %s' % commandExitCodes[0x03]
            return False, 0x03
        if state not in (0, 11):
            self.currentState['lastLog'] = 'FPW: %s' % commandExitCodes[0x06]
            return False, 0x06
            
        # Block other FIL requests
        self.currentState['activeProcess'].append('FPW')
        
        # Process in the background
        thread = threading.Thread(target=self.__fpwProcess, args=(stand, pol, state))
        thread.setDaemon(1)
        thread.start()
        
        return True, 0
        
    def __fpwProcess(self, stand, pol, state):
        """
        Background process for FPW commands so that other commands can keep on running.
        """
        
        # Do SPI bus stuff
        status = True
        config = self.__getStandConfig(stand)
        for i,c in enumerate(config):
            if state == 11:
                if i%2 == (pol-1):
                    c['dc_on'] = True
            elif state == 0:
                if i%2 == (pol-1):
                    c['dc_on'] = False
        status = rs485Send(stand, config, self.config['antenna_mapping'],
                           maxRetry=self.config['max_rs485_retry'],
                           waitRetry=self.config['wait_rs485_retry'])
        
        if status:
            self.currentState['lastLog'] = 'FPW: Set FEE power to %02i for stand %i, pol. %i' % (state, stand, pol)
            aspFunctionsLogger.debug('FPW - Set FEE power to %02i for stand %i, pol. %i', state, stand, pol)
        
            if stand == 0:
                self.currentState['config'] = config
            else:
                self.currentState['config'][2*(stand-1)+0] = config[0]
                self.currentState['config'][2*(stand-1)+1] = config[1]
        else:
            # Something failed, report
            self.currentState['lastLog'] = 'FPW: Failed to set FEE power to %02i for stand %i, pol. %i' % (state, stand, pol)
            aspFunctionsLogger.error('FPW - Failed to set FEE power to %02i for stand %i, pol. %i', state, stand, pol)
            
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'SUMMARY! 0x%02X %s - Failed after %i attempts' % (0x07, subsystemErrorCodes[0x07], self.config['max_rs485_retry'])
            self.currentState['ready'] = False
            
        # Cleanup
        self.currentState['activeProcess'].remove('FPW')
        
        return True, 0
        
    def setARXPowerState(self, state):
        """
        Set the ARX power supply power state.
        """
        
        # Check the operational status of the system
        ##if self.currentState['status'] == 'SHUTDWN':
        ##    self.currentState['lastLog'] = 'RXP: %s' % commandExitCodes[0x0A]
        ##    return False, 0x0A
        if 'RXP' in self.currentState['activeProcess']:
            self.currentState['lastLog'] = 'RXP: %s' % commandExitCodes[0x08]
            return False, 0x08
            
        # Validate inputs
        if state not in (0, 11):
            self.currentState['lastLog'] = 'RXP: %s' % commandExitCodes[0x06]
            return False, 0x06
            
        # Block other FIL requests
        self.currentState['activeProcess'].append('RXP')
        
        # Process in the background
        thread = threading.Thread(target=self.__rxpProcess, args=(state,))
        thread.setDaemon(1)
        thread.start()
        
        return True, 0
        
    def __rxpProcess(self, state, internal=False):
        """
        Background function to toggle the power state of the ARX power 
        supply.
        """
        
        status = False
        
        if status:
            aspFunctionsLogger.debug('RXP - Set ARX power supplies to state %02i', state)
            
            if state == 0 and not internal:
                # Now that the ARX power supply is off, we need to be in error
                self.currentState['status'] = 'ERROR'
                self.currentState['info'] = 'ARXSUPPLY! 0x%02X %s' % (0x0C, subsystemErrorCodes[0x0C])
                self.currentState['ready'] = False
        else:
            aspFunctionsLogger.error('RXP - Failed to change ARX power supply status')
            
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'ARXSUPPLY! 0x%02X %s' % (0x08, subsystemErrorCodes[0x08])
            if not internal:
                self.currentState['lastLog'] = 'RXP: Failed to change ARX power supply status'
            
        # Cleanup
        if not internal:
            self.currentState['activeProcess'].remove('RXP')
            
        return True, 0
        
    def setFPWPowerState(self, state):
        """
        Set the FEE power supply power state.
        """
        
        # Check the operational status of the system
        ##if self.currentState['status'] == 'SHUTDWN':
        ##    self.currentState['lastLog'] = 'FEP: %s' % commandExitCodes[0x0A]
        ##    return False, 0x0A
        if 'FEP' in self.currentState['activeProcess']:
            self.currentState['lastLog'] = 'FEP: %s' % commandExitCodes[0x08]
            return False, 0x08
            
        # Validate inputs
        if state not in (0, 11):
            self.currentState['lastLog'] = 'FEP: %s' % commandExitCodes[0x06]
            return False, 0x06
            
        # Block other FIL requests
        self.currentState['activeProcess'].append('FEP')
        
        # Process in the background
        thread = threading.Thread(target=self.__fepProcess, args=(state,))
        thread.setDaemon(1)
        thread.start()
        
        return True, 0
        
    def __fepProcess(self, state, internal=False):
        """
        Background function to toggle the power state of the FEE power 
        supply.
        """
        
        status = False
        
        if status:
            aspFunctionsLogger.debug('FEP - Set FEE power supplies to state %02i', state)
            
            if state == 0 and not internal:
                # Now that the FEE power supply is off, we need to be in error
                self.currentState['status'] = 'ERROR'
                self.currentState['info'] = 'FEESUPPLY! 0x%02X %s' % (0x0C, subsystemErrorCodes[0x0C])
                self.currentState['ready'] = False
        else:
            aspFunctionsLogger.error('FEP - Failed to change FEE power supply status')
            
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'FEESUPPLY! 0x%02X %s' % (0x08, subsystemErrorCodes[0x08])
            if not internal:
                self.currentState['lastLog'] = 'FEP: Failed to change FEE power supply status'
                
        # Cleanup
        if not internal:
            self.currentState['activeProcess'].remove('FEP')
            
        return True, 0
        
    def getFilter(self, stand):
        """
        Return the filter code for a given stand as a two-element tuple (success, value) 
        where success is a boolean related to if the filter value was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        if stand > 0 and stand <= self.num_stands:
            config = self.currentState['config'][2*(stand-1)+0]
            filt = 2*config['narrow_hpf'] + 3*config['narrow_lpf']
            if not config['sig_on']:
                filt = 3
            return True, filt
            
        else:
            self.currentState['lastLog'] = 'Invalid stand ID (%i)' % stand
            return False, 0
    
    def getAttenuators(self, stand):
        """
        Return the attenuator settings (AT1, AT2, ATS) for a given stand as a two-element 
        tuple (success, values) where success is a boolean related to if the attenuator 
        values were found.  See the currentState['lastLog'] entry for the reason for 
        failure if the returned success value is False.
        """
        
        if  stand > 0 and stand <= self.num_stands:
            config = self.currentState['config'][2*(stand-1)+0]
            at1 = int(config['first_atten']/2)
            at2 = int(config['second_atten']/2)
            ats = 0
            return True, (at1, at2, ats)
            
        else:
            self.currentState['lastLog'] = 'Invalid stand ID (%i)' % stand
            return False, ()
    
    def getFEEPowerState(self, stand):
        """
        Return the FEE power state (pol 1, pol 2) for a given stand as a two-element tuple 
        (success, values) where success is a boolean related to if the attenuator values were 
        found.  See the currentState['lastLog'] entry for the reason for failure if the 
        returned success value is False.
        """
        
        if stand > 0 and stand <= self.num_stands:
            config0 = self.currentState['config'][2*(stand-1)+0]
            config1 = self.currentState['config'][2*(stand-1)+1]
            return True, tuple([config0['dc_on'], config1['dc_on']])
            
        else:
            self.currentState['lastLog'] = 'Invalid stand ID (%i)' % stand
            return False, ()
            
    def getFEECurrentDraw(self, stand):
        """
        Return the FEE current draw (pol 1, pol 2) for a given stand as a two-element tuple 
        (success, values) where success is a boolean related to if the attenuator values were 
        found.  See the currentState['lastLog'] entry for the reason for failure if the 
        returned success value is False.
        """
        
        if stand > 0 and stand <= self.num_stands:
            if self.currentState['chassisThreads'] is None:
                self.currentState['lastLog'] = 'FEEPOL1CUR: Monitoring processes are not running'
                return False, ()
                
            fees = self.currentState['chassisThreads'][0].getFEECurrent(stand)
            return True, tuple(fees)
        else:
            self.currentState['lastLog'] = 'Invalid stand ID (%i)' % stand
            return False, ()
            
    def getRFPower(self, stand):
        if stand > 0 and stand <= self.num_stands:
            if self.currentState['chassisThreads'] is None:
                self.currentState['lastLog'] = 'RFPWR: Monitoring processes are not running'
                return False, ()
                
            rf_power = self.currentState['chassisThreads'][0].getRFPower(stand)
            return True, tuple(rf_power)
        else:
            self.currentState['lastLog'] = 'Invalid stand ID (%i)' % stand
            return False, ()
            
    def getARXPowerSupplyStatus(self):
        """
        Now depreciated
        
        Old: Return the overall ARX power supply status as a two-element tuple (success, values) 
        where success is a boolean related to if the status was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        self.currentState['lastLog'] = 'ARXSUPPLY: This function is depreciated'
        return False, 'UNK'
            
    def getARXPowerSupplyCount(self):
        """
        Now depreciated
        
        Old: Return the number of ARX power supplies being polled as a two-element tuple (success, 
        value) where success is a boolean related to if the status was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        self.currentState['lastlog'] = 'ARXSUPPLY-NO: This function is depreciated'    
        return True, 0
        
    def getARXPowerSupplyInfo(self, psNumb):
        """
        Now depreciated
        
        Old: Return information (name - status) about the  various ARX power supplies as a two-
        element tuple (success, values) where success is a boolean related to if the values 
        were found.  See the currentState['lastLog'] entry for the reason for failure if 
        the returned success value is False.
        """
        
        self.currentState['lastLog'] = 'ARXPWRUNIT_%s: This function is now depreciated' % psNumb
        return False, None
            
    def getARXPowerSupplyCurrentDraw(self):
        """
        Now depreciated
        
        Old: Return the ARX current draw (in mA) as a two-element tuple (success, values) where 
        success is a boolean related to if the current value was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        self.currentState['lastLog'] = 'ARXCURR: This function is now depreciated'
        return False, 0 
        
    def getARXPowerSupplyVoltage(self):
        """
        Now depreciated
        
        Old: Return the ARX output voltage (in V) as a two-element tuple (success, value) where
        success is a boolean related to if the current value was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        self.currentState['lastLog'] = 'ARXVOLT: This function is now depreciated'
        return False, 0.0
        
    def getFEEPowerSupplyStatus(self):
        """
        Now depreciated
        
        Old: Return the overall FEE power supply status as a two-element tuple (success, values) 
        where success is a boolean related to if the status was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        self.currentState['lastLog'] = 'FEESUPPLY: This function is now depreciated'
        return False, 'UNK'
            
    def getFEEPowerSupplyCount(self):
        """
        Now depreciated
        
        Old: Return the number of FEE power supplies being polled as a two-element tuple (success, 
        value) where success is a boolean related to if the status was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        self.currentState['lastLog'] = 'FEESUPPLY-NO: This function is now depreciated'
        return False, 0
        
    def getFEEPowerSupplyInfo(self, psNumb):
        """
        Now depreciated
        
        Old: Return information (name and status) about the  various FEE power supplies as a three-
        element tuple (success, name, status string) where success is a boolean related to if 
        the values were found.  See the currentState['lastLog'] entry for the reason for 
        failure if the returned success value is False.
        """
        
        self.currentState['lastLog'] = 'FEEPWRUNIT_%s: This function is now depreciated' % psNumb
        return False, None
            
    def getFEEPowerSupplyCurrentDraw(self):
        """
        Now depreciated
        
        Old: Return the FEE power supply current draw (in mA) as a two-element tuple (success, values) 
        where success is a boolean related to if the current value was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        self.currentState['lastLog'] = 'FEECURR: This function is now depreciated'
        return False, 0
        
    def getFEEPowerSupplyVoltage(self):
        """
        Now depreciated
        
        Old: Return the ARX output voltage (in V) as a two-element tuple (success, value) where
        success is a boolean related to if the current value was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        self.currentState['lastLog'] = 'FEEVOLT: This function is now depreciated'
        return False, 0.0
        
    def getTemperatureStatus(self):
        """
        Return the summary status (IN_RANGE, OVER_TEMP, UNDER_TEMP) for ASP as a two-element
        tuple (success, summary)  where success is a boolean related to if the temperature 
        values were found.  See the currentState['lastLog'] entry for the reason for failure 
        if the returned success value is False.
        """
        
        if self.currentState['tempThread'] is None:
            self.currentState['lastLog'] = 'TEMP-STATUS: Monitoring process is not running'
            return False, 'UNK'
            
        else:
            summary = self.currentState['tempThread'].getOverallStatus()
                
            return True, summary
            
    def getTempSensorCount(self):
        """
        Return the number of temperature sensors currently being polled as a two-element
        tuple (success, values) where success is a boolean related to if the values were 
        found.  See the currentState['lastLog'] entry for the reason for failure if the 
        returned success value is False.
        """
        
        if self.currentState['tempThread'] is None:
            self.currentState['lastLog'] = 'TEMP-SENSE-NO: Monitoring process is not running'
            return False, 0
            
        else:
            return True, self.currentState['tempThread'].getSensorCount()
    
    def getTempSensorInfo(self, sensorNumb):
        """
        Return information (name) about the  various temperature sensors as a two-element 
        tuple (success, values) where success is a boolean related to if the values were 
        found.  See the currentState['lastLog'] entry for the reason for failure if the 
        returned success value is False.
        """
        
        
        if self.currentState['tempThread'] is None:
            self.currentState['lastLog'] = 'SENSOR-NAME-%i: Monitoring process is not running' % sensorNumb
            return False, 'UNK'
            
        else:
            if sensorNumb > 0 and sensorNumb <= self.currentState['tempThread'].getSensorCount():
                name = self.currentState['tempThread'].getDescription(sensorNumb-1)
            
                return True, name
                
            else:
                self.currentState['lastLog'] = 'SENSOR-NAME-%i: Invalid temperature sensor' % sensorNumb
                return False, None
                
    def getTempSensorData(self, sensorNumb):
        """
        Return information (temp.) about the  various temperature sensors as a two-element 
        tuple (success, values) where success is a boolean related to if the values were 
        found.  See the currentState['lastLog'] entry for the reason for failure if the 
        returned success value is False.
        """
        
        
        if self.currentState['tempThread'] is None:
            self.currentState['lastLog'] = 'SENSOR-DATA-%i: Monitoring process is not running' % sensorNumb
            return False, 0.0
            
        else:
            if sensorNumb > 0 and sensorNumb <= self.currentState['tempThread'].getSensorCount():
                value = self.currentState['tempThread'].getTemperature(sensorNumb-1)
            
                return True, value
                
            else:
                self.currentState['lastLog'] = 'SENSOR-NAME-%i: Invalid temperature sensor' % sensorNumb
                return False, 0.0
                
    def processNoBackendService(self, running):
        """
        Function to set ASP to ERROR if the backend service is not running when
        it should be.
        """
        
        if not running:
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'SUMMARY! 0x%02X %s' % (0x07, subsystemErrorCodes[0x07])
            self.currentState['lastLog'] = 'ASP backend service not running'
            self.currentState['ready'] = False
            
        return True
        
    def processWarningTemperature(self, clear=False):
        """
        Function to set ASP to WARNING if the temperature is creeping up.  This 
        function also clears the WARNING condition if things have returned to 
        normal.
        """
        
        if clear:
            if self.currentState['status'] == 'WARNING':
                self.currentState['status'] = 'NORMAL'
                self.currentState['info'] = 'Warning condition cleared, system operating normally'
            
        else:
            if self.currentState['status'] in ('NORMAL', 'WARNING'):
                self.currentState['status'] = 'WARNING'
                self.currentState['info'] = 'TEMP-STATUS! 0x%02X %s' % (0x0D, subsystemErrorCodes[0x0D])
            
        return True
        
    def processCriticalTemperature(self, high=False, low=False):
        """
        Function to set ASP to ERROR and turn off the power supplies if there is a 
        temperature problem.
        """
        
        if high:
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'TEMP-STATUS! 0x%02X %s' % (0x0A, subsystemErrorCodes[0x0A])
            self.currentState['lastLog'] = 'ASP over temperature'
            self.currentState['ready'] = False
            
        elif low:
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'TEMP-STATUS! 0x%02X %s' % (0x0B, subsystemErrorCodes[0x0B])
            self.currentState['lastLog'] = 'ASP under temperature'
            self.currentState['ready'] = False
            
        return True
        
    def processUnconfiguredChassis(self, antennas):
        """
        Function to put the system into ERROR if one of the chassis appears to 
        be unconfigured.
        """
        
        dStart, dStop = antennas[0]
        
        if self.currentState['status'] != 'ERROR':
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'SUMMARY! 0x%02X %s - Antennas %i through %i are unconfigured ' % (0x09, subsystemErrorCodes[0x09], dStart, dStop)
            self.currentState['lastLog'] = 'Antennas %i through %i are unconfigured' % (dStart, dStop)
            self.currentState['ready'] = False
        else:
            # This condition overrides the ARXSUPPLY ERROR...
            if self.currentState['info'].find('ARXSUPPLY!') != -1:
                self.currentState['status'] = 'ERROR'
                self.currentState['info'] = 'SUMMARY! 0x%02X %s - Antennas %i through %i are unconfigured ' % (0x09, subsystemErrorCodes[0x09], dStart, dStop)
                self.currentState['lastLog'] = 'Antennas %i through %i are unconfigured' % (dStart, dStop)
                self.currentState['ready'] = False
                
        return True
