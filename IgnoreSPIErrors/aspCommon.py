# -*- coding: utf-8 -*-

"""
Module to store "common" values used by ASP as well as the getTime() function for
finding out the current MJD and MPM values.

$Rev$
$LastChangedBy$
$LastChangedDate$
"""

import math
import threading
from datetime import datetime

__version__ = '0.4'
__revision__ = '$Rev$'
__all__ = ['STANDS_PER_BOARD', 'MAX_BOARDS', 'MAX_STANDS', 'MAX_ATTEN', 
		 'SUB20_I2C_MAPPING', 'SUB20_ANTENNA_MAPPING', 'SUB20_LOCKS', 
		 'MAX_SPI_RETRY', 'WAIT_SPI_RETRY', 
		 'ARX_PS_ADDRESS', 'FEE_PS_ADDRESS',
		 '__version__', '__revision__', '__all__']

# Read in the aspCommon.h file to make sure that the C and Python codes are on 
# the same page.  The filling of this file should probably be part of the 
# Makefile but it isn't clear to me how to do that.
try:
	cDefines = {}
	fh = open('aspCommon.h', 'r')
	for d in filter(lambda x: x.find('#define') >= 0, fh):
		try:
			junk, name, value = d.split(None, 2)
			cDefines[name] = value.replace('\n', '')
		except ValueError:
			pass
	fh.close()
	
except IOError:
	cDefines = {'STANDS_PER_BOARD': 8, 
			  'MAX_BOARDS': 33,}

# ASP-ARX number of stands per board
STANDS_PER_BOARD = int(cDefines['STANDS_PER_BOARD'])

# ASP-ARX board and stand limits
MAX_BOARDS = int(cDefines['MAX_BOARDS'])
MAX_STANDS = 264

# Attenuator limits
MAX_ATTEN = 15

# SUB-20 I2C device interface
SUB20_I2C_MAPPING = 0x0FD5

# SUB-20 Antennas Mapping
SUB20_ANTENNA_MAPPING = {}
SUB20_ANTENNA_MAPPING[0x0FD5] = (  1,  64)
SUB20_ANTENNA_MAPPING[0x18A0] = ( 65, 128)
SUB20_ANTENNA_MAPPING[0x140C] = (129, 192)
SUB20_ANTENNA_MAPPING[0x1419] = (193, 264)

# SUB-20 semaphores
SUB20_LOCKS = {}
s = threading.Semaphore(1)
for sn in SUB20_ANTENNA_MAPPING.keys():
	SUB20_LOCKS[int(sn)] = threading.Semaphore(1)
SUB20_LOCKS[int(SUB20_I2C_MAPPING)] = threading.Semaphore(1)

# Maximum number of times to retry sending SPI bus commands
MAX_SPI_RETRY = 0

# Base wait period in seconds between SPI command retries
WAIT_SPI_RETRY = 0.2

# ARX power supply I2C address
ARX_PS_ADDRESS = 0x1F		# I2C connector with pins 6, 7, and 8 high

# FEE power supply I2C address
FEE_PS_ADDRESS = 0x1E		# I2C connector with pins 7 and 8 high, 6 low (shorted to ground)

