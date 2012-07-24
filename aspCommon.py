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
__all__ = ['STANDS_PER_BOARD', 'MAX_BOARDS', 'MAX_STANDS', 'MAX_ATTEN', 
		 'MAX_SPI_RETRY', 'WAIT_SPI_RETRY', 
		 'ARX_PS_ADDRESS', 'FEE_PS_ADDRESS',
		 '__version__', '__revision__', '__all__']


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
ARX_PS_ADDRESS = 0x1F

# Number of FEE power supplies
FEE_PS_ADDRESS = 0x2F

