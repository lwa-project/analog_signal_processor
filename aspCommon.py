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

__version__ = '0.1'
__revision__ = '$Rev$'
__all__ = ['STANDS_PER_BOARD', 'MAX_BOARDS', 'MAX_STANDS', 'MAX_ATTEN', 'MAX_SPI_RETRY', 'WAIT_SPI_RETRY', 
		 'NUM_ARX_PS', 'NUM_FEE_PS', 'NUM_TEMP_SENSORS', 
		 'MCS_RCV_BYTES', 'getTime', '__version__', '__revision__', '__all__']


# ASP-ARX number of stands per board
STANDS_PER_BOARD = 8

# ASP-ARX board and stand limits
MAX_BOARDS = 33
MAX_STANDS = 260

# Attenuator limits
MAX_ATTEN = 30

# Maximum number of times to retry sending SPI bus commands
MAX_SPI_RETRY = 3

# Base wait period in seconds between SPI command retries
WAIT_SPI_RETRY = 0.2

# Number of ARX power supplies
NUM_ARX_PS = 1

# Number of FEE power supplies
NUM_FEE_PS = 1

# Number of temperature sensors
NUM_TEMP_SENSORS = 1

# Maximum number of bytes to receive from MCS
MCS_RCV_BYTES = 16*1024


def getTime():
	"""
	Return a two-element tuple of the current MJD and MPM.
	"""
	
	# determine current time
	dt = datetime.utcnow()
	year        = dt.year             
	month       = dt.month      
	day         = dt.day    
	hour        = dt.hour
	minute      = dt.minute
	second      = dt.second     
	millisecond = dt.microsecond / 1000

	# compute MJD         
	# adapted from http://paste.lisp.org/display/73536
	# can check result using http://www.csgnetwork.com/julianmodifdateconv.html
	a = (14 - month) // 12
	y = year + 4800 - a          
	m = month + (12 * a) - 3                    
	p = day + (((153 * m) + 2) // 5) + (365 * y)   
	q = (y // 4) - (y // 100) + (y // 400) - 32045
	mjd = int(math.floor( (p+q) - 2400000.5))  

	# compute MPM
	mpm = int(math.floor( (hour*3600 + minute*60 + second)*1000 + millisecond ))

	return (mjd, mpm)
