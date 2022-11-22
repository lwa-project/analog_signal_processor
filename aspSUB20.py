# -*- coding: utf-8 -*-
"""
Module for storing the various SUB-20 function calls
"""

import time
import logging
import threading
import subprocess
from collections import deque

from aspCommon import MAX_SPI_RETRY, WAIT_SPI_RETRY, SUB20_ANTENNA_MAPPING, SUB20_LOCKS

__version__ = '0.3'
__all__ = ['spiCountBoards', 'SPICommandCallback', 'SPIProcessingThread', 'psuSend', 
           'SPI_cfg_normal', 'SPI_cfg_shutdown', 
           'SPI_cfg_output_P12_13_14_15', 'SPI_cfg_output_P16_17_18_19', 'SPI_cfg_output_P20_21_22_23', 'SPI_cfg_output_P24_25_26_27', 'SPI_cfg_output_P28_29_30_31',
           'SPI_P14_on', 'SPI_P14_off', 'SPI_P15_on', 'SPI_P15_off', 'SPI_P16_on', 'SPI_P16_off', 'SPI_P17_on', 'SPI_P17_off', 
           'SPI_P18_on', 'SPI_P18_off', 'SPI_P19_on', 'SPI_P19_off', 'SPI_P20_on', 'SPI_P20_off', 'SPI_P21_on', 'SPI_P21_off', 
           'SPI_P22_on', 'SPI_P22_off', 'SPI_P23_on', 'SPI_P23_off', 'SPI_P24_on', 'SPI_P24_off', 'SPI_P25_on', 'SPI_P25_off', 
           'SPI_P26_on', 'SPI_P26_off', 'SPI_P27_on', 'SPI_P27_off', 'SPI_P28_on', 'SPI_P28_off', 'SPI_P29_on', 'SPI_P29_off', 
           'SPI_P30_on', 'SPI_P30_off', 'SPI_P31_on', 'SPI_P31_off', 'SPI_NoOp']


aspSUB20Logger = logging.getLogger('__main__')


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
    
    def __init__(self, sub20SN, maxRetry=MAX_SPI_RETRY, waitRetry=WAIT_SPI_RETRY):
        super(_spi_thread_count, self).__init__(name="%04X-count" % sub20SN)
        
        self.sub20SN = int(sub20SN)
        
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


def spiCountBoards(maxRetry=MAX_SPI_RETRY, waitRetry=WAIT_SPI_RETRY):
    """
    Count the number of ARX stands on all known SUB-20s.
    """
    
    taskList = []
    for sub20SN in sorted(SUB20_ANTENNA_MAPPING):
        task = _spi_thread_count(sub20SN, maxRetry=maxRetry, waitRetry=waitRetry)
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


class SPICommandCallback(object):
    """
    Class for executing callbacks after a sucessful SPI command.
    """
    
    def __init__(self, func, *args, **kwds):
        self._func = func
        self._args = args
        self._kwds = kwds
        
    def __call__(self):
        return self._func(*self._args, **self._kwds)


class SPIProcessingThread(object):
    """
    Class for batch execution of SPI commands.
    """
    
    _lock = threading.Lock()
    
    def __init__(self, pollInterval=2.0, maxRetry=MAX_SPI_RETRY, waitRetry=WAIT_SPI_RETRY):
        self._pollInterval = pollInterval
        self._maxRetry = maxRetry
        self._waitRetry = waitRetry
        
        self._queue = {}
        for sub20SN in sorted(SUB20_ANTENNA_MAPPING):
            self._queue[sub20SN] = deque()
            
        self.thread = None
        self.alive = threading.Event()
        
    def start(self):
        if self.thread is not None:
            self.stop()
            
        self.thread = threading.Thread(target=self.processingThread)
        self.thread.daemon = 1
        self.alive.set()
        self.thread.start()
        
        time.sleep(1)
        
    def stop(self):
        if self.thread is not None:
            self.alive.clear()
            self.thread.join()
            
    @staticmethod
    def _run_command(sub20SN, device_count, devices, spi_commands, maxRetry=MAX_SPI_RETRY, waitRetry=WAIT_SPI_RETRY):
        with SUB20_LOCKS[sub20SN]:
            command = ["/usr/local/bin/sendARXDevice", "%04X" % sub20SN, str(device_count)]
            for dev,cmd in zip(devices,spi_commands):
                command.append(str(dev))
                command.append("0x%04X" % cmd)
                
            attempt = 0
            status = False
            while ((not status) and (attempt <= maxRetry)):
                if attempt != 0:
                    time.sleep(waitRetry)
                    
                try:
                    subprocess.check_call(command)
                    status = True
                    
                except subprocess.CalledProcessError:
                    pass
                    
        return status
            
    def process_command(self, device, command, callback=None):
        status = True
        
        with self._lock:
            if device == 0:
                for sub20SN in sorted(SUB20_ANTENNA_MAPPING):
                    device_count = SUB20_ANTENNA_MAPPING[sub20SN][1] - SUB20_ANTENNA_MAPPING[sub20SN][0] + 1
                    
                    devices, commands = [], []
                    for dev in range(SUB20_ANTENNA_MAPPING[sub20SN][0], SUB20_ANTENNA_MAPPING[sub20SN][1]+1):
                        devices.append(dev - SUB20_ANTENNA_MAPPING[sub20SN][0] + 1)
                        commands.append(command)
                    status &= self._run_command(sub20SN, device_count, devices, commands, maxRetry=MAX_SPI_RETRY, waitRetry=WAIT_SPI_RETRY)
                        
            else:
                for sub20SN in SUB20_ANTENNA_MAPPING:
                    device_count = SUB20_ANTENNA_MAPPING[sub20SN][1] - SUB20_ANTENNA_MAPPING[sub20SN][0] + 1
                    
                    if device >= SUB20_ANTENNA_MAPPING[sub20SN][0] and device <= SUB20_ANTENNA_MAPPING[sub20SN][1]:
                        devices = [device - SUB20_ANTENNA_MAPPING[sub20SN][0] + 1,]
                        commands = [command,]
                        status &= self._run_command(sub20SN, device_count, devices, commands, maxRetry=MAX_SPI_RETRY, waitRetry=WAIT_SPI_RETRY)
                        
        return status
        
    def queue_command(self, device, command, callback=None):
        with self._lock:
            if device == 0:
                for sub20SN in sorted(SUB20_ANTENNA_MAPPING):
                    for dev in range(SUB20_ANTENNA_MAPPING[sub20SN][0], SUB20_ANTENNA_MAPPING[sub20SN][1]+1):
                        dev = dev - SUB20_ANTENNA_MAPPING[sub20SN][0] + 1
                        self._queue[sub20SN].append((dev,command,callback))
                        callback = None
            else:
                for sub20SN in SUB20_ANTENNA_MAPPING:
                    if device >= SUB20_ANTENNA_MAPPING[sub20SN][0] and device <= SUB20_ANTENNA_MAPPING[sub20SN][1]:
                        dev = device - SUB20_ANTENNA_MAPPING[sub20SN][0] + 1
                        self._queue[sub20SN].append((dev,command,callback))
                        
    def processingThread(self):
        while self.alive.is_set():
            for sub20SN in sorted(SUB20_ANTENNA_MAPPING):
                to_execute = None
                with self._lock:
                    if len(self._queue[sub20SN]) > 0:
                        to_execute = self._queue[sub20SN]
                        self._queue[sub20SN] = deque()
                        
                if to_execute is not None:
                    device_count = SUB20_ANTENNA_MAPPING[sub20SN][1] - SUB20_ANTENNA_MAPPING[sub20SN][0] + 1
                    status = self._run_command(sub20SN, device_count,
                                               [entry[0] for entry in to_execute],
                                               [entry[1] for entry in to_execute],
                                               maxRetry=self._maxRetry, waitRetry=self._waitRetry)
                    
                    if status:
                        for device,command,callback in to_execute:
                            if callback is None:
                                continue
                            try:
                                callback()
                            except Exception as e:
                                aspSUB20Logger.warning("Failed to process callback for device %i, comamnd %04X: %s", device, command, str(e))
                                
            time.sleep(self._pollInterval)


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
