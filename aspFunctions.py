# -*- coding: utf-8 -*-
"""
Module for storing the miscellaneous functions used by asp_cmnd for running ASP.

$Rev$
$LastChangedBy$
$LastChangedDate$
"""

import os
import time
import logging
import threading
import subprocess

from aspCommon import *
from aspSPI import *
from aspThreads import *


__version__ = '0.3'
__revision__ = '$Rev$'
__all__ = ['modeDict', 'commandExitCodes', 'AnaloglProcessor', '__version__', '__revision__', '__all__']


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
				   0x01: 'PS over temperature warning', 
				   0x02: 'PS under temperature warning', 
				   0x03: 'PS over voltage warning', 
				   0x04: 'PS under voltage warning', 
				   0x05: 'PS over current warning', 
				   0x06: 'PS module fault error',
				   0x07: 'Failed to process SPI commands',
				   0x08: 'Board count mis-match',}


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
		self.serialNumber = 'ASP01'
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
		self.currentState['power'] = [[0,0] for i in xrange(MAX_BOARDS*STANDS_PER_BOARD)]
		self.currentState['filter'] = [0 for i in xrange(MAX_BOARDS*STANDS_PER_BOARD)]
		self.currentState['at1'] = [30 for i in xrange(MAX_BOARDS*STANDS_PER_BOARD)]
		self.currentState['at2'] = [30 for i in xrange(MAX_BOARDS*STANDS_PER_BOARD)]
		self.currentState['ats'] = [30 for i in xrange(MAX_BOARDS*STANDS_PER_BOARD)]
		
		## Monitoring and background threads
		self.currentState['tempThread'] = None
		self.currentState['powerThread'] = None
		
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
			
		# Check to see if the system has already been initalized
		if self.currentState['ready']:
			aspFunctionsLogger.warning("INI command rejected due to system already running")
			self.currentState['lastLog'] = 'INI: %s' % commandExitCodes[0x09]
			return False, 0x09
			
		# Check to see if there is a valid number of boards
		if nBoards < 0 or nBoards > MAX_BOARDS:
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
		
		# Board check - found vs. expected from INI
		boardsFound = os.system("countBoards") / 256
		if boardsFound == nBoards:
			# Board and stand counts.  NOTE: Stand counts are capped at 260
			self.num_boards = nBoards
			self.num_stands = nBoards * STANDS_PER_BOARD
			if self.num_stands > 260:
				self.num_stands = 260
			self.num_chpairs = nBoards * STANDS_PER_BOARD
			aspFunctionsLogger.info('Starting ASP with %i boards (%i stands)', self.num_boards, self.num_stands)
				
			# Stop all threads.  If the don't exist yet, create them.
			if self.currentState['tempThread'] is not None:
				self.currentState['tempThread'].stop()
				self.currentState['tempThread'].upateConfig(self.config)
			else:
				self.currentState['tempThread'] = TemperatureSensors(self.config, ASPCallbackInstance=self)
			if self.currentState['powerThread'] is not None:
				self.currentState['powerThread'].stop()
				self.currentState['powerThread'].upateConfig(self.config)
			else:
				self.currentState['powerThread'] = PowerStatus(self.config, ASPCallbackInstance=self)
			
			# Update the analog signal chain state
			for i in xrange(self.num_stands):
				self.currentState['power'][i] = [0,0]
				self.currentState['filter'][i] = 0
				self.currentState['at1'][i] = 30
				self.currentState['at2'][i] = 30
				self.currentState['ats'][i] = 30
			
			# Do SPI bus stuff
			attempt = 0
			status = False
			while ( (not status) and (attempt < MAX_SPI_RETRY) ):
				if attempt != 0:
					time.sleep(WAIT_SPI_RETRY*attempt*attempt)
				
				status  = True
				status &= SPI_init_devices(self.num_chpairs, SPI_cfg_normal)				# Out of sleep mode
				status &= SPI_init_devices(self.num_chpairs, SPI_cfg_output_P16_17_18_19)		# Set outputs
				status &= SPI_init_devices(self.num_chpairs, SPI_cfg_output_P20_21_22_23)		# Set outputs
				status &= SPI_init_devices(self.num_chpairs, SPI_cfg_output_P24_25_26_27)		# Set outputs
				status &= SPI_init_devices(self.num_chpairs, SPI_cfg_output_P28_29_30_31)		# Set outputs
				
				attempt += 1
			
			# Start the threads
			self.currentState['tempThread'].start()
			self.currentState['powerThread'].start()
			
			if status:
				self.currentState['status'] = 'NORMAL'
				self.currentState['info'] = 'System operating normally'
				self.currentState['lastLog'] = 'INI: finished in %.3f s' % (time.time() - tStart,)
				self.currentState['ready'] = True
				
				LCD_Write('ASP\nReady')
				
			else:
				self.currentState['status'] = 'ERROR'
				self.currentState['info'] = 'SUMMARY! 0x%02X %s - Failed after %i attempts' % (0x07, subsystemErrorCodes[0x07], MAX_SPI_RETRY)
				self.currentState['lastLog'] = 'INI: finished with error'
				self.currentState['ready'] = False
				
				LCD_Write('ASP\nINI fail')
				aspFunctionsLogger.critical("INI failed sending SPI bus commands after %i attempts", MAX_SPI_RETRY)
		else:
			self.currentState['status'] = 'ERROR'
			self.currentState['info'] = 'SUMMARY! 0x%02X %s - Found %i boards, expected %i' % (0x08, subsystemErrorCodes[0x08], boardsFound, nBoards)
			self.currentState['lastLog'] = 'INI: finished with error'
			self.currentState['ready'] = False
			
			LCD_Write('ASP\nINI fail')
			aspFunctionsLogger.critical("INI failed sending SPI bus commands after %i attempts", MAX_SPI_RETRY)
		
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
			
		## Check if we can even run SHT
		#if not self.currentState['ready']:
			#self.currentState['lastLog'] = 'SHT: %s' % commandExitCodes[0x0A]
			#return False, 0x0A
		
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
		if self.currentState['tempThread'] is not None:
			self.currentState['tempThread'].stop()
		if self.currentState['powerThread'] is not None:
			self.currentState['powerThread'].stop()
			
		# Do SPI bus stuff
		attempt = 0
		status = False
		while ( (not status) and (attempt < MAX_SPI_RETRY) ):
			if attempt != 0:
				time.sleep(WAIT_SPI_RETRY*attempt*attempt)
			
			status  = True
			status &= SPI_config_devices(self.num_chpairs, SPI_cfg_shutdown)		# Into sleep mode
			
			attempt += 1
		
		if status:
			self.currentState['status'] = 'SHUTDWN'
			self.currentState['info'] = 'System has been shut down'
			self.currentState['lastLog'] = 'System has been shut down'
			
			LCD_Write('ASP\nshutdown')
			
		else:
			self.currentState['status'] = 'ERROR'
			self.currentState['info'] = 'SHT failed sending SPI bus commands after %i attempts' % MAX_SPI_RETRY
			self.currentState['lastLog'] = 'SHT: failed in %.3f s' % (time.time() - tStart,)
			self.currentState['ready'] = False
			
			LCD_Write('ASP\nSHT fail')
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
		if self.currentState['status'] == 'SHUTDWN':
			self.currentState['lastLog'] = 'FIL: %s' % commandExitCodes[0x0A]
			return False, 0x0A
		if 'FIL%03d' % stand in self.currentState['activeProcess']:
			self.currentState['lastLog'] = 'FIL: %s' % commandExitCodes[0x08]
			return False, 0x08
		if 'FIL000' in self.currentState['activeProcess']:
			self.currentState['lastLog'] = 'FIL: %s' % commandExitCodes[0x08]
			return False, 0x08
			
		# Validate inputs
		if stand < 0 or stand > self.num_stands:
			self.currentState['lastLog'] = 'FIL: %s' % commandExitCodes[0x02]
			return False, 0x02
		if filterCode < 0 or filterCode > 3:
			self.currentState['lastLog'] = 'FIL: %s' % commandExitCodes[0x04]
			return False, 0x04
			
		# Block other FIL requests
		self.currentState['activeProcess'].append('FIL%03d' % stand)
		
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
		attempt = 0
		status = False
		while ( (not status) and (attempt < MAX_SPI_RETRY) ):
			if attempt != 0:
				time.sleep(WAIT_SPI_RETRY*attempt*attempt)
			
			status = True
			if filterCode == 3:
				# Set Filters OFF
				status &= SPI_Send(self.num_chpairs, stand, SPI_P19_on)
				status &= SPI_Send(self.num_chpairs, stand, SPI_P18_on)
			elif filterCode == 1:
				# Set Filter to Full Bandwidth
				status &= SPI_Send(self.num_chpairs, stand, SPI_P19_off)
				status &= SPI_Send(self.num_chpairs, stand, SPI_P18_on )
			elif filterCode == 2:
				# Set Filter to Reduced Bandwidth
				status &= SPI_Send(self.num_chpairs, stand, SPI_P19_on )
				status &= SPI_Send(self.num_chpairs, stand, SPI_P18_off)
			elif filterCode == 0:
				# Set Filter to Split Bandwidth
				status &= SPI_Send(self.num_chpairs, stand, SPI_P19_off)
				status &= SPI_Send(self.num_chpairs, stand, SPI_P18_off)
				
			attempt += 1
		
		if status:
			# All of the commands worked, update the state
			self.currentState['lastLog'] = 'FIL: Set filter to %02i for stand %i' % (filterCode, stand)
			aspFunctionsLogger.debug('FIL - Set filter to %02i for stand %i', filterCode, stand)
			LCD_Write('Stand%03i\nFIL=%02i' % (stand, filterCode))
			
			if stand != 0:
				self.currentState['filter'][stand-1] = filterCode
			else:
				for i in xrange(self.num_stands):
					self.currentState['filter'][i] = filterCode
		else:
			# Something failed, report
			self.currentState['lastLog'] = 'FIL: Failed to set filter to %02i for stand %i' % (filterCode, stand)
			aspFunctionsLogger.error('FIL - Failed to set filter to %02i for stand %i', filterCode, stand)
		
		# Cleanup and save the state of FIL
		self.currentState['activeProcess'].remove('FIL%03d' % stand)
		
		return True, 0
		
	def setAttenuator(self, mode, stand, attenSetting):
		"""
		Set one of the attenuators for a given stand.  The attenuators are:
		  1. AT1
		  2. AT2
		  3. ATS
		"""
		
		# Check the operational status of the system
		if self.currentState['status'] == 'SHUTDWN':
			self.currentState['lastLog'] = '%s: %s' % (modeDict[mode], commandExitCodes[0x0A])
			return False, 0x0A
		if '%s%03d' % (modeDict[mode], stand) in self.currentState['activeProcess']:
			self.currentState['lastLog'] = '%s: %s' % (modeDict[mode], commandExitCodes[0x08])
			return False, 0x08
		if '%s000' % modeDict[mode] in self.currentState['activeProcess']:
			self.currentState['lastLog'] = '%s: %s' % (modeDict[mode], commandExitCodes[0x08])
			return False, 0x08
			
		# Validate inputs
		if stand < 0 or stand > self.num_stands:
			self.currentState['lastLog'] = '%s: %s' % (modeDict[mode], commandExitCodes[0x02])
			return False, 0x02
		if attenSetting < 0 or attenSetting > MAX_ATTEN:
			self.currentState['lastLog'] = '%s: %s' % (modeDict[mode], commandExitCodes[0x05])
			return False, 0x05
			
		# Block other FIL requests
		self.currentState['activeProcess'].append('%s%03d' % (modeDict[mode], stand))
		
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
		attempt = 0
		status = False
		while ( (not status) and (attempt < MAX_SPI_RETRY) ):
			if attempt != 0:
				time.sleep(WAIT_SPI_RETRY*attempt*attempt)
			
			status = True
			setting = 2*attenSetting
			
			if mode == 1:
				if setting >= 16:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P27_on )
					setting -= 16
				else:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P27_off)
					
				if setting >= 8:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P24_on )
					setting -= 8
				else:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P24_off)
					
				if setting >= 4:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P25_on )
					setting -= 4
				else:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P25_off)
					
				if setting >= 2:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P26_on )
					setting -= 2
				else:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P26_off)
					
			elif mode == 2:
				if setting >= 16:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P23_on )
					setting -= 16
				else:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P23_off)
					
				if setting >= 8:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P21_on )
					setting -= 8
				else:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P21_off)
					
				if setting >= 4:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P20_on )
					setting -= 4
				else:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P20_off)
					
				if setting >= 2:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P22_on )
					setting -= 2
				else:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P22_off)
					
			elif mode == 3:
				if setting >= 16:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P31_on )
					setting -= 16
				else:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P31_off)
					
				if setting >= 8:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P28_on )
					setting -= 8
				else:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P28_off)
					
				if setting >= 4:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P29_on )
					setting -= 4
				else:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P29_off)
					
				if setting >= 2:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P30_on )
					setting -= 2
				else:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P30_off)
					
			attempt += 1

		if status:
			# All of the commands worked, update the state
			self.currentState['lastLog'] = '%s: Set attenuator to %02i for stand %i' % (modeDict[mode], attenSetting, stand)
			aspFunctionsLogger.debug('%s - Set attenuator to %02i for stand %i', modeDict[mode], attenSetting, stand)
			LCD_Write('Stand%03i\n%3s=%02i' % (stand, modeDict[mode], attenSetting))
			
			if stand != 0:
				self.currentState[modeDict[mode].lower()][stand-1] = attenSetting
			else:
				for i in xrange(self.num_stands):
					self.currentState[modeDict[mode].lower()][i] = attenSetting
					
		else:
			# Something failed, report
			self.currentState['lastLog'] = '%s: Failed to set attenuator to %02i for stand %i' % (modeDict[mode], attenSetting, stand)
			aspFunctionsLogger.error('%s - Failed to set attenuator to %02i for stand %i', modeDict[mode], attenSetting, stand)
		
		# Cleanup
		self.currentState['activeProcess'].remove('%s%03d' % (modeDict[mode], stand))
		
		
		return True, 0
		
	def setFEEPowerState(self, stand, pol, state):
		"""
		Set the FEE power state for a given stand/pol.
		"""
		
		# Check the operational status of the system
		if self.currentState['status'] == 'SHUTDWN':
			self.currentState['lastLog'] = 'FPW: %s' % commandExitCodes[0x0A]
			return False, 0x0A
		if 'FPW%03d%1d' % (stand, pol) in self.currentState['activeProcess']:
			self.currentState['lastLog'] = 'FPW: %s' % commandExitCodes[0x08]
			return False, 0x08
		if 'FPW000%1d' % pol in self.currentState['activeProcess']:
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
		self.currentState['activeProcess'].append('FPW%03d%d' % (stand, pol))
		
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
		attempt = 0
		status = False
		while ( (not status) and (attempt < MAX_SPI_RETRY) ):
			if attempt != 0:
				time.sleep(WAIT_SPI_RETRY*attempt*attempt)
			
			status = True
			if state == 11:
				if pol == 1:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P17_on )
				elif pol == 2:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P16_on )
			elif state == 0:
				if pol == 1:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P17_off)
				elif pol == 2:
					status &= SPI_Send(self.num_chpairs, stand, SPI_P16_off)
					
			attempt += 1
		
		if status:
			# All of the commands worked, update the state
			self.currentState['lastLog'] = 'FPW: Set FEE power to %02i for stand %i, pol. %i' % (state, stand, pol)
			aspFunctionsLogger.debug('FPW - Set FEE power to %02i for stand %i, pol. %i', state, stand, pol)
			LCD_Write('Stand%03i\npol%1i=%3s'% (stand, pol, 'on ' if state else 'off'))
			
			if stand != 0:
				self.currentState['power'][stand-1][pol-1] = state
			else:
				for i in xrange(self.num_stands):
					self.currentState['power'][i][pol-1] = state
					
		else:
			# Something failed, report
			self.currentState['lastLog'] = 'FPW: Failed to set FEE power to %02i for stand %i, pol. %i' % (state, stand, pol)
			aspFunctionsLogger.error('FPW - Failed to set FEE power to %02i for stand %i, pol. %i', state, stand, pol)
		
		# Cleanup
		self.currentState['activeProcess'].remove('FPW%03d%1d' % (stand, pol))
		
		return True, 0
		
	def setARXPowerState(self, state):
		"""
		Set the ARX power supply power state.
		
		.. note::
			This is currently not implemented.
		"""
		
		# Check the operational status of the system
		if self.currentState['status'] == 'SHUTDWN':
			self.currentState['lastLog'] = 'RXP: %s' % commandExitCodes[0x0A]
			return False, 0x0A
		if 'RXP' in self.currentState['activeProcess']:
			self.currentState['lastLog'] = 'RXP: %s' % commandExitCodes[0x08]
			return False, 0x08
			
		# NOTE: Not implemented
		self.currentState['lastLog'] = 'RXP: %s' % commandExitCodes[0x0B]
		return False, 0x0B
			
		# Validate inputs
		if state not in (0, 11):
			self.currentState['lastLog'] = 'RXP: %s' % commandExitCodes[0x06]
			return False, 0x06
			
		# Block other FIL requests
		self.currentState['activeProcess'].append('RXP')
		
		# Process in the background
		thread = threading.Thread(target=self.__rxpProcess, args=(stand, pol, state))
		thread.setDaemon(1)
		thread.start()
		
		return True, 0
		
	def setFEPPowerState(self, state):
		"""
		Set the FEE power supply power state.
		
		.. note::
			This is currently not implemented.
		"""
		
		# Check the operational status of the system
		if self.currentState['status'] == 'SHUTDWN':
			self.currentState['lastLog'] = 'FEP: %s' % commandExitCodes[0x0A]
			return False, 0x0A
		if 'FEP' in self.currentState['activeProcess']:
			self.currentState['lastLog'] = 'FEP: %s' % commandExitCodes[0x08]
			return False, 0x08
			
		# NOTE: Not implemented
		self.currentState['lastLog'] = 'FEP: %s' % commandExitCodes[0x0B]
		return False, 0x0B
			
		# Validate inputs
		if state not in (0, 11):
			self.currentState['lastLog'] = 'FEP: %s' % commandExitCodes[0x06]
			return False, 0x06
			
		# Block other FIL requests
		self.currentState['activeProcess'].append('FEP')
		
		# Process in the background
		thread = threading.Thread(target=self.__fepProcess, args=(stand, pol, state))
		thread.setDaemon(1)
		thread.start()
		
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
		
		status = 'ON '
		for i in xrange(self.config['powerThread'].getPSUCount()):
			voltage = self.config['powerThread'].getVoltage(i)
			if voltage is None:
				continue
			if voltage > 9:
				continue
			
			s = self.config['powerThread'].getOnOff(i)
			
			if s == 'OFF':
				status = 'OFF'
				break
			elif s == 'UNK':
				status = 'UNK'
				break
			else:
				pass
			
		return True, status
		
	def getARXPowerSupplyInfo(self, psNumb):
		"""
		Return information (name) about the  various ARX power supplies as a two-element 
		tuple (success, values) where success is a boolean related to if the values were 
		found.  See the currentState['lastLog'] entry for the reason for failure if the 
		returned success value is False.
		"""
		
		if psNumb > 0 and psNumb <= self.config['powerThread'].getPSUCount():
			name = self.config['powerThread'].getDescription(psNumbNumb-1)
		
			return True, name
			
		else:
			self.currentState['lastLog'] = 'Invalid ARX power supply (%i)' % psNumb
			return False, None
			
	def getARXCurrentDraw(self):
		"""
		Return the ARX current draw (in mA) as a two-element tuple (success, values) where 
		success is a boolean related to if the current value was found.  See the 
		currentState['lastLog'] entry for the reason for failure if the returned success 
		value is False.
		"""
		
		curr = 0.0
		for i in xrange(self.config['powerThread'].getPSUCount()):
			voltage = self.config['powerThread'].getVoltage(i)
			if voltage is None:
				continue
			if voltage > 9:
				continue
			
			c += self.config['powerThread'].getCurrent(i)
			
		return True, c*1000.0
		
	def getARXVoltage():
		"""
		Return the ARX output voltage (in V) as a two-element tuple (success, value) where
		success is a boolean related to if the current value was found.  See the 
		currentState['lastLog'] entry for the reason for failure if the returned success 
		value is False.
		"""
		
		volt = 0.0
		count = 0
		for v in xrange(self.config['powerThread'].getPSUCount()):
			voltage = self.config['powerThread'].getVoltage(i)
			if voltage is None:
				continue
			if voltage > 9:
				continue
			
			volt += voltage
			count += 1
		
		try:
			volt /= float(count)
		except:
			volt = 0.0
			
		return True, volt
		
	def getFEEPowerSupplyStatus(self):
		"""
		Return the overall FEE power supply status as a two-element tuple (success, values) 
		where success is a boolean related to if the status was found.  See the 
		currentState['lastLog'] entry for the reason for failure if the returned success 
		value is False.
		"""
		
		status = 'ON '
		for i in xrange(self.config['powerThread'].getPSUCount()):
			voltage = self.config['powerThread'].getVoltage(i)
			if voltage is None:
				continue
			if voltage < 9:
				continue
			
			s = self.config['powerThread'].getOnOff(i)
			
			if s == 'OFF':
				status = 'OFF'
				break
			elif s == 'UNK':
				status = 'UNK'
				break
			else:
				pass
			
		return True, status
		
	def getFEEPowerSupplyInfo(self, psNumb):
		"""
		Return information (name and status) about the  various FEE power supplies as a three-
		element tuple (success, name, status string) where success is a boolean related to if 
		the values were found.  See the currentState['lastLog'] entry for the reason for 
		failure if the returned success value is False.
		"""
		
		if psNumb > 0 and psNumb <= self.config['powerThread'].getPSUCount():
			name = self.config['powerThread'].getDescription(psNumbNumb-1)
			status = self.config['powerThread'].getDescription(psNumbNumb-1)
		
			return True, name, status
			
		else:
			self.currentState['lastLog'] = 'Invalid FEE power supply (%i)' % psNumb
			return False, None, None
			
	def getFEECurrentDraw(self):
		"""
		Return the FEE power supply current draw (in mA) as a two-element tuple (success, values) 
		where success is a boolean related to if the current value was found.  See the 
		currentState['lastLog'] entry for the reason for failure if the returned success 
		value is False.
		"""
		
		curr = 0.0
		for i in xrange(self.config['powerThread'].getPSUCount()):
			voltage = self.config['powerThread'].getVoltage(i)
			if voltage is None:
				continue
			if voltage < 9:
				continue
			
			c += self.config['powerThread'].getCurrent(i)
			
		return True, c*1000.0
		
	def getFEEVoltage():
		"""
		Return the ARX output voltage (in V) as a two-element tuple (success, value) where
		success is a boolean related to if the current value was found.  See the 
		currentState['lastLog'] entry for the reason for failure if the returned success 
		value is False.
		"""
		
		volt = 0.0
		count = 0
		for v in xrange(self.config['powerThread'].getPSUCount()):
			voltage = self.config['powerThread'].getVoltage(i)
			if voltage is None:
				continue
			if voltage < 9:
				continue
			
			volt += voltage
			count += 1
		
		try:
			volt /= float(count)
		except:
			volt = 0.0
			
		return True, volt
		
	def getTemperatureStatus(self):
		"""
		Return the summary status (IN_RANGE, OVER_TEMP, UNDER_TEMP) for ASP as a two-element
		tuple (success, summary)  where success is a boolean related to if the temperature 
		values were found.  See the currentState['lastLog'] entry for the reason for failure 
		if the returned success value is False.
		"""
		
		summary = 'IN_RANGE'
		for i in xrange(self.config['tempThread'].getSensorCount()):
			temp = self.config['tempThread'].getTemperature(i)
			if temp is None:
				continue
			
			if temp < self.config['TEMPMIN']:
				summary = 'UNDER_TEMP'
				break
			elif temp > self.config['TEMPMAX']:
				summary = 'OVER_TEMP'
				break
			else:
				pass
			
		return True, summary
	
	def getTempSensorInfo(self, sensorNumb):
		"""
		Return information (name, temp.) about the  various temperature sensors as a two-
		element tuple (success, values) where success is a boolean related to if the values 
		were found.  See the currentState['lastLog'] entry for the reason for failure if 
		the returned success value is False.
		"""
		
		if sensorNumb > 0 and sensorNumb <= self.config['tempThread'].getSensorCount():
			name = self.config['tempThread'].getDescription(sensorNumb-1)
			temp = self.config['tempThread'].getTemperature(sensorNumb-1)
		
			return True, (name, temp)
			
		else:
			self.currentState['lastLog'] = 'Invalid temperature sensor (%i)' % sensorNumb
			return False, ()
