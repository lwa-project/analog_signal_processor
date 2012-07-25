# -*- coding: utf-8 -*-

"""
Module to store "common" values used by ASP as well as the getTime() function for
finding out the current MJD and MPM values.

$Rev$
$LastChangedBy$
$LastChangedDate$
"""

import math
from datetime import datetime

__version__ = '0.2'
__revision__ = '$Rev$'
__all__ = ['STANDS_PER_BOARD', 'MAX_BOARDS', 'MAX_STANDS', 'MAX_ATTEN', 
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
MAX_STANDS = 260

# Attenuator limits
MAX_ATTEN = 15

# Maximum number of times to retry sending SPI bus commands
MAX_SPI_RETRY = 3

# Base wait period in seconds between SPI command retries
WAIT_SPI_RETRY = 0.2

# Number of ARX power supplies
ARX_PS_ADDRESS = 0x1F

# Number of FEE power supplies
FEE_PS_ADDRESS = 0x2F

