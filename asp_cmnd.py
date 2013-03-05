#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
asp_cmnd - Software for controlling ASP within the guidelines of the ASP and 
MCS ICDs.

$Rev$
$LastChangedBy$
$LastChangedDate$
"""

import os
import sys
import time
import getopt
import signal
import socket
import string
import struct
import logging
try:
	from logging.handlers import WatchedFileHandler
except ImportError:
	from logging import FileHandler as WatchedFileHandler
import traceback
try:
	import cStringIO as StringIO
except ImportError:
	import StringIO


from MCS import *

from aspCommon import *
from aspFunctions import  AnalogProcessor


__version__ = '0.3'
__revision__ = '$Rev$'
__date__ = '$LastChangedDate$'
__all__ = ['DEFAULTS_FILENAME', 'parseConfigFile', 'MCSCommunicate', '__version__', '__revision__', '__date__', '__all__']


#
# Default Configuration File
#
DEFAULTS_FILENAME = '/lwa/software/defaults.cfg'


def usage(exitCode=None):
	print """asp_cmnd.py - Control the ASP sub-system within the guidelines of the ASP 
and MCS ICDs.

Usage: asp_cmnd.py [OPTIONS]

Options:
-h, --help        Display this help information
-c, --config      Name of the ASP configuration file to use
-l, --log         Name of the logfile to write logging information to
-d, --debug       Print debug messages as well as info and higher
"""

	if exitCode is not None:
		sys.exit(exitCode)
	else:
		return True


def parseOptions(args):
	"""
	Parse the command line options and return a dictionary of the configuration
	"""

	config = {}
	# Default parameters
	config['configFilename'] = DEFAULTS_FILENAME
	config['logFilename'] = None
	config['debugMessages'] = False
	
	# Read in and process the command line flags
	try:
		opts, args = getopt.getopt(args, "hc:l:dk", ["help", "config=", "log=", "debug"])
	except getopt.GetoptError, err:
		# Print help information and exit:
		print str(err) # will print something like "option -a not recognized"
		usage(exitCode=2)
		
	# Work through opts
	for opt, value in opts:
		if opt in ('-h', '--help'):
			usage(exitCode=0)
		elif opt in ('-c', '--config'):
			config['configFilename'] = value
		elif opt in ('-l', '--log'):
			config['logFilename'] = value
		elif opt in ('-d', '--debug'):
			config['debugMessages'] = True
		else:
			assert False
	
	# Add in arguments
	config['args'] = args

	# Return configuration
	return config


def parseConfigFile(filename):
	"""
	Given a filename of a ASP configuation file, read in the various values
	and return the requested configuation as a dictionary.
	"""
	
	# Deal with logging
	logger = logging.getLogger(__name__)
	logger.info("Parsing config file '%s'", filename)

	# List of the required parameters and their coercion functions
	coerceMap = {'MESSAGEHOST'    : str,
			   'MESSAGEOUTPORT' : int,
			   'MESSAGEINPORT'  : int, 
			   'TEMPMIN'        : float, 
			   'TEMPWARN'       : float,
			   'TEMPMAX'        : float, 
			   'TEMPPERIOD'     : float, 
			   'POWERPERIOD'    : float}
	config = {}

	#
	# read defaults config file
	#
	if not os.path.exists(filename):
		logger.critical('Config file does not exist: %s', filename)
		sys.exit(1)

	cfile_error = False
	fh = open(filename, 'r')
	for line in fh:
		line = line.strip()
		if len(line) == 0 or line.startswith('#'):
			continue    # ignore blank or comment line
			
		tokens = line.split()
		if len(tokens) != 2:
			logger.error('Invalid config line, needs parameter-name and value: %s', line)
			cfile_error = True
			continue
		
		paramName = tokens[0].upper()
		if paramName in coerceMap.keys():
			# Try to do the type conversion and, for int's and float's, make sure
			# the values are greater than zero.
			try:
				val = coerceMap[paramName](tokens[1])
				if coerceMap[paramName] == int or coerceMap[paramName] == float:
					if val <= 0:
						logger.error("Integer and float values must be > 0")
						cfile_error = True
					else:
						config[paramName] = val
				else:
					config[paramName] = val
					
			except Exception, e:
				logger.error("Error parsing parameter %s: %s", paramName, str(e))
				cfile_error = True
				
		else:
			logger.warning("Unknown config parameter %s", paramName)
			
	# Verify that all required parameters were found
	for paramName in coerceMap.keys():
		if not paramName in config:
			logger.error("Config parameter '%s' is missing", paramName)
			cfile_error = True
	if cfile_error:
		logger.critical("Error parsing configuation file '%s'", filename)
		sys.exit(1)

	return config


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
		self.logger.debug('Got command %s from %s with ref# %i', sender, command, reference)
	
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
				elif data[0:4] == 'AT1_':
					stand = int(data[4:])
					
					status, attens = self.SubSystemInstance.getAttenuators(stand)
					if status:
						packed_data = str(attens[0])
					else:
						packed_data = self.SubSystemInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
				elif data[0:4] == 'AT2_':
					stand = int(data[4:])
					
					status, attens = self.SubSystemInstance.getAttenuators(stand)
					if status:
						packed_data = str(attens[1])
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
				elif data[0:11] == 'FEEPOL1PWR_':
					stand = int(data[11:])
					
					status, power = self.SubSystemInstance.getFEEPowerState(stand)
					if status:
						if power[0]:
							packed_data = 'ON '
						else:
							packed_data = 'OFF'
					else:
						packed_data = self.SubSystemInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
				elif data[0:11] == 'FEEPOL2PWR_':
					stand = int(data[11:])
					
					status, power = self.SubSystemInstance.getFEEPowerState(stand)
					if status:
						if power[1]:
							packed_data = 'ON '
						else:
							packed_data = 'OFF'
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
					status, value = self.SubSystemInstance.getARXCurrentDraw()
					if status:
						packed_data = "%-7i" % value
						
					else:
						packed_data = self.SubSystemInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
				elif data == 'ARXVOLT':
					status, value = self.SubSystemInstance.getARXVoltage()
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
					status, value = self.SubSystemInstance.getFEECurrentDraw()
					if status:
						packed_data = "%-7i" % value
						
					else:
						packed_data = self.SubSystemInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
				elif data == 'FEEVOLT':
					status, value = self.SubSystemInstance.getFEEVoltage()
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
				config = parseConfigFile(self.opts['configFilename'])
		
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
	
	# Parse command line options
	opts = parseOptions(args)
	
	# Setup logging
	logger = logging.getLogger(__name__)
	logFormat = logging.Formatter('%(asctime)s [%(levelname)-8s] %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
	logFormat.converter = time.gmtime
	if opts['logFilename'] is None:
		logHandler = logging.StreamHandler(sys.stdout)
	else:
		logHandler = WatchedFileHandler(opts['logFilename'])
	logHandler.setFormatter(logFormat)
	logger.addHandler(logHandler)
	if opts['debugMessages']:
		logger.setLevel(logging.DEBUG)
	else:
		logger.setLevel(logging.INFO)
	
	# Get current MJD and MPM
	mjd, mpm = getTime()
	
	# Report on who we are
	shortRevision = __revision__.split()[1]
	shortDate = ' '.join(__date__.split()[1:4])
	
	logger.info('Starting asp_cmnd.py with PID %i', os.getpid())
	logger.info('Version: %s', __version__)
	logger.info('Revision: %s', shortRevision)
	logger.info('Last Changed: %s',shortDate)
	logger.info('Current MJD: %i', mjd)
	logger.info('Current MPM: %i', mpm)
	logger.info('All dates and times are in UTC except where noted')
	
	# Read in the configuration file
	config = parseConfigFile(opts['configFilename'])
	
	# Setup ASP control
	lwaASP = AnalogProcessor(config)

	# Setup the communications channels
	mcsComms = MCSCommunicate(lwaASP, config, opts)
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
			
		except Exception, e:
			exc_type, exc_value, exc_traceback = sys.exc_info()
			logger.error("asp_cmnd.py failed with: %s at line %i", str(e), traceback.tb_lineno(exc_traceback))
				
			## Grab the full traceback and save it to a string via StringIO
			fileObject = StringIO.StringIO()
			traceback.print_tb(exc_traceback, file=fileObject)
			tbString = fileObject.getvalue()
			fileObject.close()
			## Print the traceback to the logger as a series of DEBUG messages
			for line in tbString.split('\n'):
				logger.debug("%s", line)
	
	# If we've made it this far, we have finished so shutdown ASP and close the 
	# communications channels
	tStop = time.time()
	print '\nShutting down ASP, please wait...'
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
	main(sys.argv[1:])
