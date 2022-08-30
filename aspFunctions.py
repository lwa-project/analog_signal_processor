# -*- coding: utf-8 -*-
"""
Module for storing the miscellaneous functions used by asp_cmnd for running ASP.
"""

import os
import time
import ctypes
import logging
import threading

from aspSUB20 import *
from aspThreads import *


__version__ = '0.6'
__all__ = ['modeDict', 'commandExitCodes', 'AnalogProcessor']


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
    
    class StandMapping(ctypes.LittleEndianStructure):
        """
        Class for helping store the mapping information needed for the new
        multi-SUB-20 architecture.
        """
        
        _fields_ = [("SUB20", ctypes.c_char*5), 
                    ("stand", ctypes.c_ushort),
                    ("redStand", ctypes.c_ushort)]
                  
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
        self.currentState['power']  = [[0,0] for i in range(self.config['max_boards']*self.config['stands_per_board'])]
        self.currentState['filter'] = [0     for i in range(self.config['max_boards']*self.config['stands_per_board'])]
        self.currentState['at1']    = [30    for i in range(self.config['max_boards']*self.config['stands_per_board'])]
        self.currentState['at2']    = [30    for i in range(self.config['max_boards']*self.config['stands_per_board'])]
        self.currentState['ats']    = [30    for i in range(self.config['max_boards']*self.config['stands_per_board'])]
        
        ## Monitoring and background threads
        self.currentState['tempThread'] = None
        self.currentState['powerThreads'] = None
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
        
        # Make sure the SUB-20 is present
        if os.system('lsusb -d 04d8: >/dev/null') == 0:
            # Good, we can continue
            
            # Turn off the power supplies
            self.__rxpProcess(00, internal=True)
            self.__fepProcess(00, internal=True)
            time.sleep(5)
            
            # Turn on the power supplies
            self.__rxpProcess(11, internal=True)
            self.__fepProcess(11, internal=True)
            time.sleep(1)
            
            # Board check - found vs. expected from INI
            boardsFound = spiCountBoards()
            if boardsFound == nBoards:
                # Board and stand counts.  NOTE: Stand counts are capped at 260
                self.num_boards = nBoards
                self.num_stands = nBoards * self.config['stands_per_board']
                if self.num_stands > self.config['max_stands']:
                    self.num_stands = self.config['max_stands']
                self.num_chpairs = nBoards * self.config['stands_per_board']
                aspFunctionsLogger.info('Starting ASP with %i boards (%i stands)', self.num_boards, self.num_stands)
                    
                # Stop all threads.  If the don't exist yet, create them.
                if self.currentState['powerThreads'] is not None:
                    for t in self.currentState['powerThreads']:
                        t.stop()
                        t.updateConfig(self.config)
                else:
                    self.currentState['powerThreads'] = []
                    self.currentState['powerThreads'].append( PowerStatus(self.config['sub20_i2c_mapping'], self.config['arx_ps_address'], self.config, ASPCallbackInstance=self) )
                    self.currentState['powerThreads'].append( PowerStatus(self.config['sub20_i2c_mapping'], self.config['fee_ps_address'], self.config, ASPCallbackInstance=self) )
                if self.currentState['tempThread'] is not None:
                    self.currentState['tempThread'].stop()
                    self.currentState['tempThread'].updateConfig(self.config)
                else:
                    self.currentState['tempThread'] = TemperatureSensors(self.config['sub20_i2c_mapping'], self.config, ASPCallbackInstance=self)
                if self.currentState['chassisThreads'] is not None:
                    for t in self.currentState['chassisThreads']:
                        t.stop()
                        t.updateConfig(self.config)
                else:
                    self.currentState['chassisThreads'] = []
                    self.currentState['chassisThreads'].append( ChassisStatus(self.config['sub20_i2c_mapping'], self.config, ASPCallbackInstance=self) )
                    
                # Update the analog signal chain state
                for i in range(self.num_stands):
                    self.currentState['power'][i] = [0,0]
                    self.currentState['filter'][i] = 0
                    self.currentState['at1'][i] = 30
                    self.currentState['at2'][i] = 30
                    self.currentState['ats'][i] = 30
                    
                # Do the SPI bus stuff
                status  = True
                status &= spiSend(0, SPI_cfg_shutdown)                   # Into sleep mode
                status &= spiSend(0, SPI_cfg_normal)                     # Out of sleep mode
                status &= spiSend(0, SPI_cfg_output_P12_13_14_15)        # Set outputs
                status &= spiSend(0, SPI_cfg_output_P16_17_18_19)        # Set outputs
                status &= spiSend(0, SPI_cfg_output_P20_21_22_23)        # Set outputs
                status &= spiSend(0, SPI_cfg_output_P24_25_26_27)        # Set outputs
                status &= spiSend(0, SPI_cfg_output_P28_29_30_31)        # Set outputs
                
                # Start the threads
                for t in self.currentState['powerThreads']:
                    t.start()
                self.currentState['tempThread'].start()
                for t in self.currentState['chassisThreads']:
                    t.start()
                    
                if status:
                    self.currentState['status'] = 'NORMAL'
                    self.currentState['info'] = 'System operating normally'
                    self.currentState['lastLog'] = 'INI: finished in %.3f s' % (time.time() - tStart,)
                    self.currentState['ready'] = True
                    
                    lcdSend(self.config['sub20_i2c_mapping'], 'ASP\nReady')
                    
                else:
                    self.currentState['status'] = 'ERROR'
                    self.currentState['info'] = 'SUMMARY! 0x%02X %s - Failed after %i attempts' % (0x07, subsystemErrorCodes[0x07], MAX_SPI_RETRY)
                    self.currentState['lastLog'] = 'INI: finished with error'
                    self.currentState['ready'] = False
                    
                    lcdSend(self.config['sub20_i2c_mapping'], 'ASP\nINI fail')
                    aspFunctionsLogger.critical("INI failed sending SPI bus commands after %i attempts", MAX_SPI_RETRY)
            else:
                self.currentState['status'] = 'ERROR'
                self.currentState['info'] = 'SUMMARY! 0x%02X %s - Found %i boards, expected %i' % (0x09, subsystemErrorCodes[0x09], boardsFound, nBoards)
                self.currentState['lastLog'] = 'INI: finished with error'
                self.currentState['ready'] = False
                
                lcdSend(self.config['sub20_i2c_mapping'], 'ASP\nINI fail')
                aspFunctionsLogger.critical("INI failed; found %i boards, expected %i", boardsFound, nBoards)
                
        else:
            # Oops, the SUB-20 is missing...
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'SUMMARY! 0x%02X %s - SUB-20 device not found' % (0x07, subsystemErrorCodes[0x07])
            self.currentState['lastLog'] = 'INI: finished with error'
            self.currentState['ready'] = False
            
            lcdSend(self.config['sub20_i2c_mapping'], 'ASP\nINI fail')
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
        
        # Stop all threads.
        if self.currentState['powerThreads'] is not None:
            for t in self.currentState['powerThreads']:
                t.stop()
        if self.currentState['tempThread'] is not None:
            self.currentState['tempThread'].stop()
        if self.currentState['chassisThreads'] is not None:
            for t in self.currentState['chassisThreads']:
                t.stop()
                
        # Do SPI bus stuff (only if the boards are on)
        if self.getARXPowerSupplyStatus()[1] == 'ON ':
            status = spiSend(0, SPI_cfg_shutdown)        # Into sleep mode
            time.sleep(5)
        status = True
        
        if status:
            # Power off the power supplies
            self.__rxpProcess(00, internal=True)
            self.__fepProcess(00, internal=True)

            lcdSend(self.config['sub20_i2c_mapping'], 'ASP\nshutdown')
            
            self.currentState['status'] = 'SHUTDWN'
            self.currentState['info'] = 'System has been shut down'
            self.currentState['lastLog'] = 'System has been shut down'
            
        else:
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'SUMMARY! 0x%02X %s - Failed after %i attempts' % (0x07, subsystemErrorCodes[0x07], MAX_SPI_RETRY)
            self.currentState['lastLog'] = 'SHT: failed in %.3f s' % (time.time() - tStart,)
            self.currentState['ready'] = False
            
            lcdSend(self.config['sub20_i2c_mapping'], 'ASP\nSHT fail')
            aspFunctionsLogger.critical("SHT failed sending SPI bus commands after %i attempts", MAX_SPI_RETRY)
        
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
        
        # Do SPI bus stuff
        status = True
        if filterCode > 3:
            # Set 3 MHz mode
            status &= spiSend(stand, SPI_P14_on )
            status &= spiSend(stand, SPI_P15_off)
        else:
            # Set 10 MHz mode
            status &= spiSend(stand, SPI_P14_off)
            status &= spiSend(stand, SPI_P15_on )
            
        if filterCode == 0 or filterCode == 4:
            # Set Filter to Split Bandwidth
            status &= spiSend(stand, SPI_P19_off)
            status &= spiSend(stand, SPI_P18_off)
        elif filterCode == 1 or filterCode == 5:
            # Set Filter to Full Bandwidth
            status &= spiSend(stand, SPI_P19_off)
            status &= spiSend(stand, SPI_P18_on )
        elif filterCode == 2:
            # Set Filter to Reduced Bandwidth
            status &= spiSend(stand, SPI_P19_on )
            status &= spiSend(stand, SPI_P18_off)
        elif filterCode == 3:
            # Set Filters OFF
            status &= spiSend(stand, SPI_P19_on )
            status &= spiSend(stand, SPI_P18_on )
            
        if status:
            self.currentState['lastLog'] = 'FIL: Set filter to %02i for stand %i' % (filterCode, stand)
            aspFunctionsLogger.debug('FIL - Set filter to %02i for stand %i', filterCode, stand)
        
            #lcdSend(self.config['sub20_i2c_mapping'], 'Stand%03i\nFIL=%02i' % (stand, filterCode))
            if stand != 0:
                self.currentState['filter'][stand-1] = filterCode
            else:
                for i in range(self.num_stands):
                    self.currentState['filter'][i] = filterCode
        else:
            # Something failed, report
            self.currentState['lastLog'] = 'FIL: Failed to set filter to %02i for stand %i' % (filterCode, stand)
            aspFunctionsLogger.error('FIL - Failed to set filter to %02i for stand %i', filterCode, stand)
            
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'SUMMARY! 0x%02X %s - Failed after %i attempts' % (0x07, subsystemErrorCodes[0x07], MAX_SPI_RETRY)
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
        
        # Do SPI bus stuff
        setting = 2*attenSetting
        
        if mode == 1:
            order = ((SPI_P27_on, SPI_P27_off), (SPI_P24_on, SPI_P24_off), (SPI_P25_on, SPI_P25_off), (SPI_P26_on, SPI_P26_off))
        elif mode == 2:
            order = ((SPI_P23_on, SPI_P23_off), (SPI_P21_on, SPI_P21_off), (SPI_P20_on, SPI_P20_off), (SPI_P22_on, SPI_P22_off))
        else:
            order = ((SPI_P31_on, SPI_P31_off), (SPI_P28_on, SPI_P28_off), (SPI_P29_on, SPI_P29_off), (SPI_P30_on, SPI_P30_off))
            
        status = True
        if setting >= 16:
            status &= spiSend(stand, order[0][0])
            setting -= 16
        else:
            status &= spiSend(stand, order[0][1])
            
        if setting >= 8:
            status &= spiSend(stand, order[1][0])
            setting -= 8
        else:
            status &= spiSend(stand, order[1][1])
            
        if setting >= 4:
            status &= spiSend(stand, order[2][0])
            setting -= 4
        else:
            status &= spiSend(stand, order[2][1])
            
        if setting >= 2:
            status &= spiSend(stand, order[3][0])
            setting -= 2
        else:
            status &= spiSend(stand, order[3][1])
            
        if status:
            self.currentState['lastLog'] = '%s: Set attenuator to %02i for stand %i' % (modeDict[mode], attenSetting, stand)
            aspFunctionsLogger.debug('%s - Set attenuator to %02i for stand %i', modeDict[mode], attenSetting, stand)
            
            #lcdSend(self.config['sub20_i2c_mapping'], 'Stand%03i\n%3s=%02i' % (stand, modeDict[mode], attenSetting))
            if stand != 0:
                self.currentState[modeDict[mode].lower()][stand-1] = attenSetting
            else:
                for i in range(self.num_stands):
                    self.currentState[modeDict[mode].lower()][i] = attenSetting
        else:
            # Something failed, report
            self.currentState['lastLog'] = '%s: Failed to set attenuator to %02i for stand %i' % (modeDict[mode], attenSetting, stand)
            aspFunctionsLogger.error('%s - Failed to set attenuator to %02i for stand %i', modeDict[mode], attenSetting, stand)
            
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'SUMMARY! 0x%02X %s - Failed after %i attempts' % (0x07, subsystemErrorCodes[0x07], MAX_SPI_RETRY)
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
        if pol < 0 or pol > 2:
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
        if state == 11:
            if pol == 1:
                status &= spiSend(stand, SPI_P17_on )
            elif pol == 2:
                status &= spiSend(stand, SPI_P16_on )
        elif state == 0:
            if pol == 1:
                status &= spiSend(stand, SPI_P17_off)
            elif pol == 2:
                status &= spiSend(stand, SPI_P16_off)
                
        if status:
            self.currentState['lastLog'] = 'FPW: Set FEE power to %02i for stand %i, pol. %i' % (state, stand, pol)
            aspFunctionsLogger.debug('FPW - Set FEE power to %02i for stand %i, pol. %i', state, stand, pol)
        
            #lcdSend(self.config['sub20_i2c_mapping'], 'Stand%03i\npol%1i=%3s'% (stand, pol, 'on ' if state else 'off'))
            if stand != 0:
                self.currentState['power'][stand-1][pol-1] = state
            else:
                for i in range(self.num_stands):
                    self.currentState['power'][i][pol-1] = state
        else:
            # Something failed, report
            self.currentState['lastLog'] = 'FPW: Failed to set FEE power to %02i for stand %i, pol. %i' % (state, stand, pol)
            aspFunctionsLogger.error('FPW - Failed to set FEE power to %02i for stand %i, pol. %i', state, stand, pol)
            
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'SUMMARY! 0x%02X %s - Failed after %i attempts' % (0x07, subsystemErrorCodes[0x07], MAX_SPI_RETRY)
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
        
        status = psuSend(self.config['sub20_i2c_mapping'], self.config['arx_ps_address'], state)
        
        if status:
            aspFunctionsLogger.debug('RXP - Set ARX power supplies to state %02i', state)
            
            lcdSend(self.config['sub20_i2c_mapping'], 'ARX PS\n%s' % ('OFF' if state == 0 else 'ON',))
            
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
        
        status = psuSend(self.config['sub20_i2c_mapping'], self.config['fee_ps_address'], state)
        
        if status:
            aspFunctionsLogger.debug('FEP - Set FEE power supplies to state %02i', state)
            
            lcdSend(self.config['sub20_i2c_mapping'], 'FEE PS\n%s' % ('OFF' if state == 0 else 'ON',))
            
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
            return True, self.currentState['filter'][stand-1]
            
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
            at1 = self.currentState['at1'][stand-1]
            at2 = self.currentState['at2'][stand-1]
            ats = self.currentState['ats'][stand-1]
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
            return True, tuple(self.currentState['power'][stand-1])
            
        else:
            self.currentState['lastLog'] = 'Invalid stand ID (%i)' % stand
            return False, ()
            
    def getARXPowerSupplyStatus(self):
        """
        Return the overall ARX power supply status as a two-element tuple (success, values) 
        where success is a boolean related to if the status was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        if self.currentState['powerThreads'] is None:
            self.currentState['lastLog'] = 'ARXSUPPLY: Monitoring processes are not running'
            return False, 'UNK'
            
        else:
            status = 'UNK'
            for t in self.currentState['powerThreads']:
                if t.getDeviceAddress() == self.config['arx_ps_address']:
                    status = t.getOnOff()
                
            return True, status
            
    def getARXPowerSupplyCount(self):
        """
        Return the number of ARX power supplies being polled as a two-element tuple (success, 
        value) where success is a boolean related to if the status was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        if self.currentState['powerThreads'] is None:
            self.currentState['lastLog'] = 'ARXSUPPLY-NO: Monitoring processes are not running'
            return False, 0
            
        else:
            return True, 1
        
    def getARXPowerSupplyInfo(self, psNumb):
        """
        Return information (name - status) about the  various ARX power supplies as a two-
        element tuple (success, values) where success is a boolean related to if the values 
        were found.  See the currentState['lastLog'] entry for the reason for failure if 
        the returned success value is False.
        """
        
        if self.currentState['powerThreads'] is None:
            self.currentState['lastLog'] = 'ARXPWRUNIT_%s: Monitoring processes are not running' % psNumb
            return False, None
            
        else:
            if psNumb > 0 and psNumb < 2:
                info = 'UNK - UNK'
                for t in self.currentState['powerThreads']:
                    if t.getDeviceAddress() == self.config['arx_ps_address']:
                        info1 = t.getDescription()
                        info2 = t.getStatus()
                        info = "%s - %s" % (info1, info2)
            
                return True, info
                
            else:
                self.currentState['lastLog'] = 'ARXPWRUNIT_%s: Invalid ARX power supply' % psNumb
                return False, None
            
    def getARXCurrentDraw(self):
        """
        Return the ARX current draw (in mA) as a two-element tuple (success, values) where 
        success is a boolean related to if the current value was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        if self.currentState['powerThreads'] is None:
            self.currentState['lastLog'] = 'ARXCURR: Monitoring processes are not running'
            return False, 0
            
        else:
            curr = 0.0
            for t in self.currentState['powerThreads']:
                if t.getDeviceAddress() == self.config['arx_ps_address']:
                    curr = t.getCurrent()
                    
            return True, curr*1000.0
        
    def getARXVoltage(self):
        """
        Return the ARX output voltage (in V) as a two-element tuple (success, value) where
        success is a boolean related to if the current value was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        if self.currentState['powerThreads'] is None:
            self.currentState['lastLog'] = 'ARXVOLT: Monitoring processes are not running'
            return False, 0.0
            
        else:
            volt = 0.0
            for t in self.currentState['powerThreads']:
                if t.getDeviceAddress() == self.config['arx_ps_address']:
                    volt = t.getVoltage()
                    
            return True, volt
        
    def getFEEPowerSupplyStatus(self):
        """
        Return the overall FEE power supply status as a two-element tuple (success, values) 
        where success is a boolean related to if the status was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        if self.currentState['powerThreads'] is None:
            self.currentState['lastLog'] = 'FEESUPPLY: Monitoring processes are not running'
            return False, 'UNK'
            
        else:
            status = 'UNK'
            for t in self.currentState['powerThreads']:
                if t.getDeviceAddress() == self.config['fee_ps_address']:
                    status = t.getOnOff()
                
            return True, status
            
    def getFEEPowerSupplyCount(self):
        """
        Return the number of FEE power supplies being polled as a two-element tuple (success, 
        value) where success is a boolean related to if the status was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        if self.currentState['powerThreads'] is None:
            self.currentState['lastLog'] = 'FEESUPPLY-NO: Monitoring processes are not running'
            return False, 0
            
        else:
            return True, 1
        
    def getFEEPowerSupplyInfo(self, psNumb):
        """
        Return information (name and status) about the  various FEE power supplies as a three-
        element tuple (success, name, status string) where success is a boolean related to if 
        the values were found.  See the currentState['lastLog'] entry for the reason for 
        failure if the returned success value is False.
        """
        
        if self.currentState['powerThreads'] is None:
            self.currentState['lastLog'] = 'FEEPWRUNIT_%s: Monitoring processes are not running' % psNumb
            return False, None
            
        else:
            if psNumb > 0 and psNumb < 2:
                info = 'UNK - UNK'
                for t in self.currentState['powerThreads']:
                    if t.getDeviceAddress() == self.config['fee_ps_address']:
                        info1 = t.getDescription()
                        info2 = t.getStatus()
                        info = "%s - %s" % (info1, info2)
            
                return True, info
                
            else:
                self.currentState['lastLog'] = 'Invalid ARX power supply (%i)' % psNumb
                return False, None
            
    def getFEECurrentDraw(self):
        """
        Return the FEE power supply current draw (in mA) as a two-element tuple (success, values) 
        where success is a boolean related to if the current value was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        if self.currentState['powerThreads'] is None:
            self.currentState['lastLog'] = 'FEECURR: Monitoring processes are not running'
            return False, 0
            
        else:
            curr = 0.0
            for t in self.currentState['powerThreads']:
                if t.getDeviceAddress() == self.config['fee_ps_address']:
                    curr = t.getCurrent()
                    
            return True, curr*1000.0
        
    def getFEEVoltage(self):
        """
        Return the ARX output voltage (in V) as a two-element tuple (success, value) where
        success is a boolean related to if the current value was found.  See the 
        currentState['lastLog'] entry for the reason for failure if the returned success 
        value is False.
        """
        
        if self.currentState['powerThreads'] is None:
            self.currentState['lastLog'] = 'FEEVOLT: Monitoring processes are not running'
            return False, 0.0
            
        else:
            volt = 0.0
            for t in self.currentState['powerThreads']:
                if t.getDeviceAddress() == self.config['fee_ps_address']:
                    volt = t.getVoltage()
                    
            return True, volt
        
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
            if self.getARXPowerSupplyStatus()[1] == 'ON ':
                self.__rxpProcess(00, internal=True)
                
            if self.getFEEPowerSupplyStatus()[1] == 'ON ':
                self.__fepProcess(00, internal=True)
            
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'TEMP-STATUS! 0x%02X %s' % (0x0A, subsystemErrorCodes[0x0A])
            self.currentState['lastLog'] = 'ASP over temperature - turning off power supplies'
            self.currentState['ready'] = False
            
        elif low:
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'TEMP-STATUS! 0x%02X %s' % (0x0B, subsystemErrorCodes[0x0B])
            self.currentState['lastLog'] = 'ASP under temperature'
            self.currentState['ready'] = False
            
        return True

    def processCriticalPowerSupply(self, deviceAddress, reason):
        """
        Function to shutdown critical power supplies and put the system into ERROR.
        """
        
        if reason == 'OverCurrent':
            code = 0x05
        elif reason == 'OverVolt':
            code = 0x03
        elif reason == 'UnderVolt':
            code = 0x04
        elif reason == 'OverTemperature':
            code = 0x01
        elif reason == 'ModuleFault':
            code = 0x06
        else:
            return False
        
        if deviceAddress == self.config['arx_ps_address']:
            if self.getARXPowerSupplyStatus()[1] == 'ON ':
                self.__rxpProcess(00, internal=True)
            
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'ARXPWRUNIT_1! 0x%02X %s - %s' % (code, subsystemErrorCodes[code], reason)
            self.currentState['lastLog'] = 'ARX power supply critical - %s - powered off' % reason
            self.currentState['ready'] = False
            
        elif deviceAddress == self.config['fee_ps_address']:
            if self.getFEEPowerSupplyStatus()[1] == 'ON ':
                self.__fepProcess(00, internal=True)
            
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'FEPPWRUNIT_1! 0x%02X %s - %s' % (code, subsystemErrorCodes[code], reason)
            self.currentState['lastLog'] = 'FEE power supply critical - %s - powered off' % reason
            self.currentState['ready'] = False
        
        return True
        
    def processUnconfiguredChassis(self, sub20SN):
        """
        Function to put the system into ERROR if one of the chassis appears to 
        be unconfigured.
        """
        
        dStart, dStop = SUB20_ANTENNA_MAPPING[sub20SN]
        
        if self.currentState['status'] != 'ERROR':
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'SUMMARY! 0x%02X %s - Antennas %i through %i are unconfigured ' % (0x09, subsystemErrorCodes[0x09], dStart, dStop)
            self.currentState['lastLog'] = 'Antennas %i through %i are unconfigured' % (dStart, dStop)
            self.currentState['ready'] = False
        else:
            # This condition overrides the ARXSUPPLY ERROR...
            if self.currentState['info'].find('ARXSUPPLY!') != -1:
                # ... if the power is on
                if self.getARXPowerSupplyStatus()[1] == 'ON ':
                    self.currentState['status'] = 'ERROR'
                    self.currentState['info'] = 'SUMMARY! 0x%02X %s - Antennas %i through %i are unconfigured ' % (0x09, subsystemErrorCodes[0x09], dStart, dStop)
                    self.currentState['lastLog'] = 'Antennas %i through %i are unconfigured' % (dStart, dStop)
                    self.currentState['ready'] = False
                    
        return True
        
    def processMissingSUB20(self):
        """
        Function to put the system into ERROR if the SUB-20 is missing or dead.
        """
        
        # Try it out
        if os.system('lsusb -d 04d8: >/dev/null') == 0:
            # Nope, it's really there
            return False
            
        else:
            # Yep, the SUB-20 is gone
            aspFunctionsLogger.critical('SUB-20 has disappeared from the list of USB devices')
            
            self.currentState['status'] = 'ERROR'
            self.currentState['info'] = 'SUMMARY! 0x%02X %s - SUB-20 device not found' % (0x07, subsystemErrorCodes[0x07])
            self.currentState['lastLog'] = 'SUB-20 device has disappeared'
            self.currentState['ready'] = False
            
        return True
