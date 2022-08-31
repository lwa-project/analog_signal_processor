# -*- coding: utf-8 -*-
"""
Module for storing the various SUB-20 function calls
"""

import time
import logging
import threading
import subprocess

from aspThreads import SUB20_LOCKS

__version__ = '0.2'
__all__ = ['spiCountBoards', 'spiSend', 'lcdSend', 'psuSend', 
           'SPI_cfg_normal', 'SPI_cfg_shutdown', 
           'SPI_cfg_output_P12_13_14_15', 'SPI_cfg_output_P16_17_18_19', 'SPI_cfg_output_P20_21_22_23', 'SPI_cfg_output_P24_25_26_27', 'SPI_cfg_output_P28_29_30_31',
           'SPI_P14_on', 'SPI_P14_off', 'SPI_P15_on', 'SPI_P15_off', 'SPI_P16_on', 'SPI_P16_off', 'SPI_P17_on', 'SPI_P17_off', 
           'SPI_P18_on', 'SPI_P18_off', 'SPI_P19_on', 'SPI_P19_off', 'SPI_P20_on', 'SPI_P20_off', 'SPI_P21_on', 'SPI_P21_off', 
           'SPI_P22_on', 'SPI_P22_off', 'SPI_P23_on', 'SPI_P23_off', 'SPI_P24_on', 'SPI_P24_off', 'SPI_P25_on', 'SPI_P25_off', 
           'SPI_P26_on', 'SPI_P26_off', 'SPI_P27_on', 'SPI_P27_off', 'SPI_P28_on', 'SPI_P28_off', 'SPI_P29_on', 'SPI_P29_off', 
           'SPI_P30_on', 'SPI_P30_off', 'SPI_P31_on', 'SPI_P31_off', 'SPI_NoOp']


aspSUB20Logger = logging.getLogger('__main__')

# SPI control
MAX_SPI_RETRY = 0
WAIT_SPI_RETRY = 0.2


# SPI constants
SPI_cfg_normal = 0x0104
SPI_cfg_shutdown = 0x0004
SPI_cfg_output_P12_13_14_15 = 0x550B
SPI_cfg_output_P16_17_18_19 = 0x550C
SPI_cfg_output_P20_21_22_23 = 0x550D
SPI_cfg_output_P24_25_26_27 = 0x550E
SPI_cfg_output_P28_29_30_31 = 0x550F

SPI_P14_on  = 0x012E
SPI_P14_off = 0x002E
SPI_P15_on  = 0x012F
SPI_P15_off = 0x002F
SPI_P16_on  = 0x0130
SPI_P16_off = 0x0030
SPI_P17_on  = 0x0131
SPI_P17_off = 0x0031
SPI_P18_on  = 0x0132
SPI_P18_off = 0x0032
SPI_P19_on  = 0x0133
SPI_P19_off = 0x0033
SPI_P20_on  = 0x0134
SPI_P20_off = 0x0034
SPI_P21_on  = 0x0135
SPI_P21_off = 0x0035
SPI_P22_on  = 0x0136
SPI_P22_off = 0x0036
SPI_P23_on  = 0x0137
SPI_P23_off = 0x0037
SPI_P24_on  = 0x0138
SPI_P24_off = 0x0038
SPI_P25_on  = 0x0139
SPI_P25_off = 0x0039
SPI_P26_on  = 0x013A
SPI_P26_off = 0x003A
SPI_P27_on  = 0x013B
SPI_P27_off = 0x003B

SPI_P28_on  = 0x013C
SPI_P28_off = 0x003C
SPI_P29_on  = 0x013D
SPI_P29_off = 0x003D
SPI_P30_on  = 0x013E
SPI_P30_off = 0x003E
SPI_P31_on  = 0x013F
SPI_P31_off = 0x003F

SPI_NoOp = 0x0000


_threadWaitInterval = 0.05


class _spi_thread_count(threading.Thread):
    """
    Class to count the boards attached to a single SUB-20.
    """
    
    def __init__(self, sub20SN, sub20Mapper, maxRetry=MAX_SPI_RETRY, waitRetry=WAIT_SPI_RETRY):
        super(_spi_thread_count, self).__init__(name="%04X-count" % int(sub20SN))
        
        self.sub20SN = int(sub20SN)
        self.sub20Mapper = sub20Mapper
        
        self.maxRetry = maxRetry
        self.waitRetry = waitRetry
        
        self.boards = 0
        self.status = False
        
    def run(self):
        if self.sub20SN not in SUB20_LOCKS:
            return False
        
        with SUB20_LOCKS[self.sub20SN]:
            attempt = 0
            status = False
            while ((not status) and (attempt <= self.maxRetry)):
                if attempt != 0:
                    time.sleep(self.waitRetry)
                    
                p = subprocess.Popen('/usr/local/bin/countBoards %04X' % self.sub20SN, shell=True,
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output, output2 = p.communicate()
                try:
                    output = output.decode('ascii')
                    output2 = output2.decode('ascii')
                except AttributeError:
                    pass
                    
                if p.returncode == 0:
                    aspSUB20Logger.warning("%s: SUB-20 S/N %04X command %i of %i returned %i; '%s;%s'", type(self).__name__, self.sub20SN, attempt, self.maxRetry, p.returncode, output, output2)
                    status = False
                else:
                    self.boards = p.returncode
                    status = True
                
                attempt += 1
                
        self.status = status


class _spi_thread_device(threading.Thread):
    """
    Class to start a thread to command a single device attached to a 
    single SUB-20.
    """
    
    def __init__(self, sub20SN, device, data, sub20Mapper, maxRetry=MAX_SPI_RETRY, waitRetry=WAIT_SPI_RETRY):
        super(_spi_thread_device, self).__init__(name="%04X-%i-0x%04x" % (int(sub20SN), device, data))
        
        self.sub20SN = int(sub20SN)
        self.device = device
        self.data = data
        self.sub20Mapper = sub20Mapper
        
        self.maxRetry = maxRetry
        self.waitRetry = waitRetry
        
        self.status = False
        
    def run(self):
        if self.sub20SN not in SUB20_LOCKS:
            return False
        
        with SUB20_LOCKS[self.sub20SN]:
            num = self.sub20Mapper[str(self.sub20SN)][1] - self.sub20Mapper[str(self.sub20SN)][0] + 1
            
            attempt = 0
            status = False
            while ((not status) and (attempt <= self.maxRetry)):
                if attempt != 0:
                    time.sleep(self.waitRetry)
                    
                p = subprocess.Popen('/usr/local/bin/sendARXDevice %04X %i %i 0x%04x' % (self.sub20SN, num, self.device, self.data), shell=True,
                                     stdout=subprocess.PIPE, stderr=subprocess.PIPE)
                output, output2 = p.communicate()
                try:
                    output = output.decode('ascii')
                    output2 = output2.decode('ascii')
                except AttributeError:
                    pass
                    
                if p.returncode != 0:
                    aspSUB20Logger.warning("%s: SUB-20 S/N %04X command %i of %i returned %i; '%s;%s'", type(self).__name__, self.sub20SN, attempt, self.maxRetry, p.returncode, output, output2)
                    status = False
                else:
                    status = True
                
                attempt += 1
                
        self.status = status


class _spi_thread_all(_spi_thread_device):
    """
    Class to start a thread to command all devices attached to a single 
    SUB-20.
    """
    
    def __init__(self, sub20SN, data, sub20Mapper, maxRetry=MAX_SPI_RETRY, waitRetry=WAIT_SPI_RETRY):
        super(_spi_thread_all, self).__init__(sub20SN, 0, data, sub20Mapper, maxRetry=maxRetry, waitRetry=waitRetry)


def spiCountBoards(sub20Mapper, maxRetry=MAX_SPI_RETRY, waitRetry=WAIT_SPI_RETRY):
    """
    Count the number of ARX stands on all known SUB-20s.
    """
    
    taskList = []
    for sub20SN in sorted(sub20Mapper):
        task = _spi_thread_count(sub20SN, sub20Mapper, maxRetry=maxRetry, waitRetry=waitRetry)
        task.start()
        taskList.append(task)
        
    nBoards = 0
    overallStatus = True
    for task in taskList:
        while task.isAlive():
            time.sleep(_threadWaitInterval)
        nBoards += task.boards
        overallStatus &= task.status
        
    if not overallStatus:
        nBoards = 0
        
    return nBoards


def spiSend(device, data, sub20Mapper, maxRetry=MAX_SPI_RETRY, waitRetry=WAIT_SPI_RETRY):
    """
    Send a command via SPI bus to the specified device.
    
    Return the status of the operation as a boolean.
    """
    
    taskList = []
    if device == 0:
        for sub20SN in sorted(sub20Mapper):
            task = _spi_thread_all(sub20SN, data, sub20Mapper, maxRetry=maxRetry, waitRetry=waitRetry)
            task.start()
            taskList.append(task)
        
    else:
        found = False
        for sub20SN in sub20Mapper:
            if device >= sub20Mapper[sub20SN][0] and device <= sub20Mapper[sub20SN][1]:
                found = True
                break
                
        if not found:
            aspSUB20Logger.warning("Unable to relate stand %i to a SUB-20", device)
            return False
            
        redDevice = device - sub20Mapper[sub20SN][0] + 1
        task = _spi_thread_device(sub20SN, redDevice, data, sub20Mapper, maxRetry=maxRetry, waitRetry=waitRetry)
        task.start()
        taskList.append(task)
        
    overallStatus = True
    for task in taskList:
        while task.isAlive():
            time.sleep(_threadWaitInterval)
        overallStatus &= task.status
        
    return overallStatus


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
        p = subprocess.Popen('/usr/local/bin/onoffPSU %04X 0x%02X %s' % (int(sub20SN), psuAddress, str(state)), shell=True,
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
