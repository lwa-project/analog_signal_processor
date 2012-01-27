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


from aspCommon import *
from aspFunctions import  AnalogProcessor


__version__ = '0.3'
__revision__ = '$Rev: 1 $'
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
	config['configFilename'] = '/lwa/software/defaults.cfg'
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
	coerceMap = {'MESSAGEHOST'              : str,
			   'MESSAGEOUTPORT'           : int,
			   'MESSAGEINPORT'            : int, 
			   'TEMPMIN'                  : float, 
			   'TEMPMAX'                  : float}
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


class MCSCommunicate(object):
	"""
	Class to deal with the communcating with MCS.
	"""
	
	def __init__(self, config, opts):
		self.config = config
		self.opts = opts
		
		# Update the socket configuration
		self.updateConfig()
		
		# Set the logger
		self.logger = logging.getLogger(__name__)
		
	def updateConfig(self, config=None):
		"""
		Using the configuration file, update the list of boards.
		"""
		
		# Update the current configuration
		if config is not None:
			self.config = config
		
	def start(self):
		"""
		Start the recieve thread - send will run only when needed.
		"""
		
		# Setup the various sockets
		## Receive
		try:
			self.socketIn =  socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
			self.socketIn.bind(("0.0.0.0", self.config['MESSAGEINPORT']))
		except socket.error, err:
			code, e = err
			self.logger.critical('Cannot bind to listening port %i: %s', self.config['MESSAGEINPORT'], str(e))
			self.logger.critical('Exiting on previous error')
			logging.shutdown()
			sys.exit(1)
		
		## Send
		try:
			self.socketOut = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
			self.destAddress = (self.config['MESSAGEHOST'], self.config['MESSAGEOUTPORT'])
		except socket.error, err:
			code, e = err
			self.logger.critical('Cannot bind to sending port %i: %s', self.config['MESSAGEOUTPORT'], str(e))
			self.logger.critical('Exiting on previous error')
			logging.shutdown()
			sys.exit(1)

	def stop(self):
		"""
		Stop the antenna statistics thread, waiting until it's finished.
		"""
		
		# Close the various sockets
		self.socketIn.close()
		self.socketOut.close()
		
	def sendResponse(self, destination, status, systemStatus, command, reference, data):
		"""
		Send a response to MCS via UDP.
		"""
	
		if status:
			response = 'A'
		else:
			response = 'R'
			
		# Set the sender
		sender = 'ASP'

		# Get current time
		(mjd, mpm) = getTime()

		# Build the payload
		payload = destination+sender+command+string.rjust(str(reference),9)
		payload = payload + string.rjust(str(len(data)+8),4)+string.rjust(str(mjd),6)+string.rjust(str(mpm),9)+' '
		payload = payload + response + string.rjust(str(systemStatus),7) + data
	
		bytes_sent = self.socketOut.sendto(payload, self.destAddress)
		self.logger.debug("mcsSend - Sent to MCS '%s'", payload)
		
	def receiveCommand(self, ASPInstance):
		"""
		Recieve and process MCS command over the network.
		"""
		
		data = self.socketIn.recv(MCS_RCV_BYTES)
			
		destination = data[:3]
		sender      = data[3:6]
		command     = data[6:9]
		reference   = int(data[9:18])
		datalen     = int(data[18:22]) 
		mjd         = int(data[22:28]) 
		mpm         = int(data[28:37]) 
		data        = data[38:38+datalen]
	
		# check destination and sender
		if destination in (ASPInstance.subSystem, 'ALL'):
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
					summary = ASPInstance.currentState['status'][:7]
					self.logger.debug('summary = %s', summary)
					packed_data = summary
				elif data == 'INFO':
					### Trim down as needed
					if len(ASPInstance.currentState['info']) > 256:
						infoMessage = "%s..." % ASPInstance.currentState['info'][:253]
					else:
						infoMessage = ASPInstance.currentState['info'][:256]
						
					self.logger.debug('info = %s', infoMessage)
					packed_data = infoMessage
				elif data == 'LASTLOG':
					### Trim down as needed
					if len(ASPInstance.currentState['lastLog']) > 256:
						lastLogEntry = "%s..." % ASPInstance.currentState['lastLog'][:253]
					else:
						lastLogEntry =  ASPInstance.currentState['lastLog'][:256]
					if len(lastLogEntry) == 0:
						lastLogEntry = 'no log entry'
					
					self.logger.debug('lastlog = %s', lastLogEntry)
					packed_data = lastLogEntry
				elif data == 'SUBSYSTEM':
					self.logger.debug('subsystem = %s', ASPInstance.subSystem)
					packed_data = ASPInstance.subSystem
				elif data == 'SERIALNO':
					self.logger.debug('serialno = %s', ASPInstance.serialNumber)
					packed_data = ASPInstance.serialNumber
				elif data == 'VERSION':
					self.logger.debug('version = %s', ASPInstance.version)
					packed_data = ASPInstance.version
					
				## Analog chain state - Filter
				elif data[0:7] == 'FILTER_':
					stand = int(data[7:])
					
					status, filt = ASPInstance.getFilter(stand)
					if status:
						packed_data = str(filt)
					else:
						packed_data = ASPInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
					
				## Analog chain state - Attenuators
				elif data[0:4] == 'AT1_':
					stand = int(data[4:])
					
					status, attens = ASPInstance.getAttenuators(stand)
					if status:
						packed_data = str(attens[0])
					else:
						packed_data = ASPInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
				elif data[0:4] == 'AT2_':
					stand = int(data[4:])
					
					status, attens = ASPInstance.getAttenuators(stand)
					if status:
						packed_data = str(attens[1])
					else:
						packed_data = ASPInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
				elif data[0:8] == 'ATSPLIT_':
					stand = int(data[8:])
					
					status, attens = ASPInstance.getAttenuators(stand)
					if status:
						packed_data = str(attens[2])
					else:
						packed_data = ASPInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
					
				## Analog gain state - FEE power
				elif data[0:11] == 'FEEPOL1PWR_':
					stand = int(data[11:])
					
					status, power = ASPInstance.getFEEPowerState(stand)
					if status:
						if power[0]:
							packed_data = 'ON '
						else:
							packed_data = 'OFF'
					else:
						packed_data = packed_data = ASPInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
				elif data[0:11] == 'FEEPOL2PWR_':
					stand = int(data[11:])
					
					status, power = ASPInstance.getFEEPowerState(stand)
					if status:
						if power[1]:
							packed_data = 'ON '
						else:
							packed_data = 'OFF'
					else:
						packed_data = packed_data = ASPInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
				
				## ARX power supplies
				elif data == 'ARXSUPPLY':
					status, value = ASPInstance.getARXPowerSupplyStatus()
					if status:
						packed_data = value
						
					else:
						packed_data = packed_data = ASPInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
				elif data == 'ARXSUPPLY-NO':
					packed_data = str(ASPInstance.currentState['nAPS'])
					self.logger.debug('%s = %s' % (data, packed_data))
				elif data[0:11] == 'ARXPWRUNIT_':
					psNumb = int(data[11:])
					
					status, value = ASPInstance.getARXPowerSupplyInfo(psNumb)
					if status:
						packed_data = value
						
					else:
						packed_data = packed_data = ASPInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
				elif data == 'ARXCURR':
					status, value = ASPInstance.getARXCurrentDraw()
					if status:
						packed_data = "%-7i" % value
						
					else:
						packed_data = packed_data = ASPInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
				
				## FEE power supplies
				elif data == 'FEESUPPLY':
					status, value = ASPInstance.getFEEPowerSupplyStatus()
					if status:
						packed_data = value
						
					else:
						packed_data = packed_data = ASPInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
				elif data == 'FEESUPPLY_NO':
					packed_data = str(ASPInstance.currentState['nFPS'])
					self.logger.debug('%s = %s' % (data, packed_data))
				elif data[0:11] == 'FEEPWRUNIT_':
					psNumb = int(data[11:])
					
					status, value = ASPInstance.getFEEPowerSupplyInfo(psNumb)
					if status:
						packed_data = value
						
					else:
						packed_data = packed_data = ASPInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
				elif data == 'FEECURR':
					status, value = ASPInstance.getFEECurrentDraw()
					if status:
						packed_data = "%-7i" % value
						
					else:
						packed_data = packed_data = ASPInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
					
				## Temperatue sensors
				elif data == 'TEMP-STATUS':
					status, value = ASPInstance.getTemperatureStatus()
					if status:
						packed_data = value
						
					else:
						packed_data = packed_data = ASPInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
				elif data == 'TEMP-SENSE-NO':
					packed_data = str(ASPInstance.currentState['nTS'])
					self.logger.debug('%s = %s' % (data, packed_data))
				elif data[0:12] == 'SENSOR-NAME-':
					sensorNumb = int(data[12:])
					
					status, values = ASPInstance.getTempSensorInfo(sensorNumb)
					if status:
						packed_data = values[0]
						
					else:
						packed_data = packed_data = ASPInstance.currentState['lastLog']
						
					self.logger.debug('%s = exited with status %s', data, str(status))
				elif data[0:12] == 'SENSOR-DATA-':
					sensorNumb = int(data[12:])
					
					status, values = ASPInstance.getTempSensorInfo(sensorNumb)
					if status:
						packed_data = "%-10f" % values[1]
						
					else:
						packed_data = packed_data = ASPInstance.currentState['lastLog']
						
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
				ASPInstance.updateConfig(config)
				
				# Go
				nBoards = int(data)
				status, exitCode = ASPInstance.ini(nBoards)
				if status:
					packed_data = ''
				else:
					packed_data = "0x%02X! %s" % (exitCode, ASPInstance.currentState['lastLog'])
			
			# SHT
			elif command == 'SHT':
				status, exitCode = ASPInstance.sht(mode=data)
				if status:
					packed_data = ''
				else:
					packed_data = "0x%02X! %s" % (exitCode, ASPInstance.currentState['lastLog'])
					
			# FIL
			elif command == 'FIL':
				stand = int(data[:-2])
				filterCode = int(data[-2:])
				
				status, exitCode = ASPInstance.setFilter(stand, filterCode)
				if status:
					packed_data = ''
				else:
					packed_data = "0x%02X! %s" % (exitCode, ASPInstance.currentState['lastLog'])
					
			# AT1
			elif command == 'AT1':
				mode = 1
				stand = int(data[:-2])
				attenSetting = int(data[-2:])
				
				status, exitCode = ASPInstance.setAttenuator(mode, stand, attenSetting)
				if status:
					packed_data = ''
				else:
					packed_data = "0x%02X! %s" % (exitCode, ASPInstance.currentState['lastLog'])
					
			# AT2
			elif command == 'AT2':
				mode = 2
				stand = int(data[:-2])
				attenSetting = int(data[-2:])
				
				status, exitCode = ASPInstance.setAttenuator(mode, stand, attenSetting)
				if status:
					packed_data = ''
				else:
					packed_data = "0x%02X! %s" % (exitCode, ASPInstance.currentState['lastLog'])
					
			# ATS
			elif command == 'ATS':
				mode = 3
				stand = int(data[:-2])
				attenSetting = int(data[-2:])
				
				status, exitCode = ASPInstance.setAttenuator(mode, stand, attenSetting)
				if status:
					packed_data = ''
				else:
					packed_data = "0x%02X! %s" % (exitCode, ASPInstance.currentState['lastLog'])
					
			# FPW
			elif command == 'FPW':
				stand = int(data[:-3])
				pol = int(data[-3])
				state = int(data[-2:])
				
				status, exitCode = ASPInstance.setFEEPowerState(stand, pol, state)
				if status:
					packed_data = ''
				else:
					packed_data = "0x%02X! %s" % (exitCode, ASPInstance.currentState['lastLog'])
					
			# RXP
			elif command == 'RXP':
				state = int(data)
				
				status, exitCode = ASPInstance.setARXPowerState(state)
				if status:
					packed_data = ''
				else:
					packed_data = "0x%02X! %s" % (exitCode, ASPInstance.currentState['lastLog'])
					
			elif command == 'FEP':
				state = int(data)
				
				status, exitCode = ASPInstance.getFEPPowerState(state)
				if status:
					packed_data = ''
				else:
					packed_data = "0x%02X! %s" % (exitCode, ASPInstance.currentState['lastLog'])
					
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
	
	# Setup the communications channels
	mcsComms = MCSCommunicate(config, opts)
	mcsComms.start()
	
	# Setup ASP control
	lwaASP = AnalogProcessor(config)

	# Setup handler for SIGTERM so that we aren't left in a funny state
	def HandleSignalExit(signum, frame, logger=logger, MCSInstance=mcsComms, ASPInstance=lwaASP):
		logger.info('Exiting on signal %i', signum)

		# Shutdown ASP and close the communications channels
		tStop = time.time()
		logger.info('Shutting down ASP, please wait...')
		ASPInstance.__shtProcess(mode='SCRAM')
		while ASPInstance.currentState['info'] != 'System has been shut down':
			time.sleep(1)
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
			destination, status, command, reference, response = mcsComms.receiveCommand(lwaASP)
			mcsComms.sendResponse(destination, status, lwaASP.currentState['status'], command, reference, response)
			
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
