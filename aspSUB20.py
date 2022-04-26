# -*- coding: utf-8 -*-
"""
Module for storing the various SUB-20 function calls
"""

import time
import logging
import threading
import subprocess

from aspCommon import MAX_SPI_RETRY, WAIT_SPI_RETRY, SUB20_ANTENNA_MAPPING, SUB20_LOCKS

__version__ = '0.3'
__all__ = ['lcdSend', 'psuSend']


aspSUB20Logger = logging.getLogger('__main__')


def lcdSend(sub20SN, message):
    """
    Write the specified string to the LCD screen.
    
    Return the status of the operation as a boolean.
    """
    
    with SUB20_LOCKS[sub20SN]:
        p = subprocess.Popen('/usr/local/bin/writeARXLCD %04X "%s"' % (sub20SN, message), shell=True,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        output, output2 = p.communicate()
        try:
            output = output.decode('ascii')
            output2 = output2.decode('ascii')
        except AttributeError:
            pass
            
    if p.returncode != 0:
        aspSUB20Logger.warning("lcdSend: command returned %i; '%s;%s'", p.returncode, output, output2)
        return False
    
    return True


def psuSend(sub20SN, psuAddress, state):
    """
    Set the state of the power supply unit at the provided I2C address.
    """
    
    with SUB20_LOCKS[sub20SN]:
        p = subprocess.Popen('/usr/local/bin/onoffPSU %04X 0x%02X %s' % (sub20SN, psuAddress, str(state)), shell=True,
                             stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        output, output2 = p.communicate()
        try:
            output = output.decode('ascii')
            output2 = output2.decode('ascii')
        except AttributeError:
            pass
            
    if p.returncode != 0:
        aspSUB20Logger.warning("psuSend: command returned %i; '%s;%s'", p.returncode, output, output2)
        return False
        
    return True
