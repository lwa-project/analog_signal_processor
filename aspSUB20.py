
"""
Module for storing the various SUB-20 function calls
"""

import re
import time
import inspect
import logging
import threading
import subprocess
from collections import deque

__version__ = '0.6'
__all__ = ['spiCountBoards', 'SPICommandCallback', 'SPIProcessingThread',
           'psuSend', 'psuRead', 'psuCountTemperature', 'psuTemperature',
           'rs485CountBoards', 'rs485Reset', 'rs485Sleep', 'rs485Wake', 'rs485Check',
           'rs485SetTime', 'rs485GetTime', 'rs485Power', 'rs485RFPower', 'rs485Temperature',
           'SPI_cfg_normal', 'SPI_cfg_shutdown', 
           'SPI_cfg_output_P12_13_14_15', 'SPI_cfg_output_P16_17_18_19', 'SPI_cfg_output_P20_21_22_23', 'SPI_cfg_output_P24_25_26_27', 'SPI_cfg_output_P28_29_30_31',
           'SPI_P12_on', 'SPI_P12_off', 'SPI_P13_on', 'SPI_P13_off', 'SPI_P14_on', 'SPI_P14_off', 'SPI_P15_on', 'SPI_P15_off',
           'SPI_P16_on', 'SPI_P16_off', 'SPI_P17_on', 'SPI_P17_off', 'SPI_P18_on', 'SPI_P18_off', 'SPI_P19_on', 'SPI_P19_off',
           'SPI_P20_on', 'SPI_P20_off', 'SPI_P21_on', 'SPI_P21_off', 'SPI_P22_on', 'SPI_P22_off', 'SPI_P23_on', 'SPI_P23_off',
           'SPI_P24_on', 'SPI_P24_off', 'SPI_P25_on', 'SPI_P25_off', 'SPI_P26_on', 'SPI_P26_off', 'SPI_P27_on', 'SPI_P27_off',
           'SPI_P28_on', 'SPI_P28_off', 'SPI_P29_on', 'SPI_P29_off', 'SPI_P30_on', 'SPI_P30_off', 'SPI_P31_on', 'SPI_P31_off',
           'SPI_NoOp',
           'MAX_SPI_RETRY', 'MAX_I2C_RETRY', 'MAX_RS485_RETRY']


aspSUB20Logger = logging.getLogger('__main__')


# SPI control
MAX_SPI_RETRY = 2
WAIT_SPI_RETRY = 0.2

# SPI constants
SPI_cfg_normal = 0x0104
SPI_cfg_shutdown = 0x0004
SPI_cfg_output_P12_13_14_15 = 0x550B
SPI_cfg_output_P16_17_18_19 = 0x550C
SPI_cfg_output_P20_21_22_23 = 0x550D
SPI_cfg_output_P24_25_26_27 = 0x550E
SPI_cfg_output_P28_29_30_31 = 0x550F

SPI_P12_on  = 0x012C
SPI_P12_off = 0x002C
SPI_P13_on  = 0x012D
SPI_P13_off = 0x002D
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

# I2C control
MAX_I2C_RETRY = 2
WAIT_I2C_RETRY = 0.2

# RS485 control
MAX_RS485_RETRY = 2
WAIT_RS485_RETRY = 0.2


def spiCountBoards(sub20Mapper, maxRetry=MAX_SPI_RETRY, waitRetry=WAIT_SPI_RETRY):
    """
    Count the number of ARX stands on all known SUB-20s.
    """
    
    nBoards = 0
    overallStatus = True
    for sub20SN in sorted(sub20Mapper):
        attempt = 0
        status = False
        while ((not status) and (attempt <= maxRetry)):
            if attempt != 0:
                time.sleep(waitRetry)
                
            p = subprocess.Popen(['/usr/local/bin/countBoards', str(sub20SN)],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 text=True)
            output, output2 = p.communicate()
            
            if p.returncode == 0:
                aspSUB20Logger.warning("%s: SUB-20 S/N %s command %i of %i returned %i; '%s;%s'", inspect.stack()[0][3], sub20SN, attempt, maxRetry, p.returncode, output, output2)
                status = False
            else:
                nBoards += p.returncode
                status = True
            attempt += 1
                
        overallStatus &= status
        
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
    
    def __init__(self, sub20Mapper, pollInterval=2.0, maxRetry=MAX_SPI_RETRY, waitRetry=WAIT_SPI_RETRY):
        self._sub20Mapper = sub20Mapper
        self._pollInterval = pollInterval
        self._maxRetry = maxRetry
        self._waitRetry = waitRetry
        
        self._queue = {}
        for sub20SN in sorted(self._sub20Mapper):
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
        command = ["/usr/local/bin/sendARXDevice", str(sub20SN), str(device_count)]
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
            attempt += 1
            
        return status
            
    def process_command(self, device, command, callback=None):
        status = True
        
        with self._lock:
            if device == 0:
                for sub20SN in sorted(self._sub20Mapper):
                    device_count = self._sub20Mapper[sub20SN][1] - self._sub20Mapper[sub20SN][0] + 1
                    
                    devices, commands = [], []
                    for dev in range(self._sub20Mapper[sub20SN][0], self._sub20Mapper[sub20SN][1]+1):
                        devices.append(dev - self._sub20Mapper[sub20SN][0] + 1)
                        commands.append(command)
                    status &= self._run_command(sub20SN, device_count, devices, commands, maxRetry=self._maxRetry, waitRetry=self._waitRetry)
                        
            else:
                for sub20SN in self._sub20Mapper:
                    device_count = self._sub20Mapper[sub20SN][1] - self._sub20Mapper[sub20SN][0] + 1
                    
                    if device >= self._sub20Mapper[sub20SN][0] and device <= self._sub20Mapper[sub20SN][1]:
                        devices = [device - self._sub20Mapper[sub20SN][0] + 1,]
                        commands = [command,]
                        status &= self._run_command(sub20SN, device_count, devices, commands, maxRetry=self._maxRetry, waitRetry=self._waitRetry)
                        
        return status
        
    def queue_command(self, device, command, callback=None):
        with self._lock:
            if device == 0:
                for sub20SN in sorted(self._sub20Mapper):
                    for dev in range(self._sub20Mapper[sub20SN][0], self._sub20Mapper[sub20SN][1]+1):
                        dev = dev - self._sub20Mapper[sub20SN][0] + 1
                        self._queue[sub20SN].append((dev,command,callback))
                        callback = None
            else:
                for sub20SN in self._sub20Mapper:
                    if device >= self._sub20Mapper[sub20SN][0] and device <= self._sub20Mapper[sub20SN][1]:
                        dev = device - self._sub20Mapper[sub20SN][0] + 1
                        self._queue[sub20SN].append((dev,command,callback))
                        
    def processingThread(self):
        while self.alive.is_set():
            for sub20SN in sorted(self._sub20Mapper):
                to_execute = None
                with self._lock:
                    if len(self._queue[sub20SN]) > 0:
                        to_execute = self._queue[sub20SN]
                        self._queue[sub20SN] = deque()
                        
                if to_execute is not None:
                    device_count = self._sub20Mapper[sub20SN][1] - self._sub20Mapper[sub20SN][0] + 1
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


def psuSend(sub20SN, psuAddress, state, maxRetry=MAX_I2C_RETRY, waitRetry=WAIT_I2C_RETRY):
    """
    Set the state of the power supply unit at the provided I2C address.
    """
    
    status = False
    for attempt in range(maxRetry+1):
        if attempt != 0:
            time.sleep(waitRetry)
            
        try:
            p = subprocess.Popen(['/usr/local/bin/onoffPSU', str(sub20SN), '0x%02X' % psuAddress, str(state)],
                                    stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    text=True)
            output, output2 = p.communicate()
            
            if p.returncode == 0:
                status = True
                break
            else:
                aspSUB20Logger.warning("psuSend: command returned %i; '%s;%s'", p.returncode, output, output2)
                
        except Exception as e:
            aspSUB20Logger.warning("Could not send command to PSU %s: %s", psuAddress, str(e))
            
    return status


def psuRead(sub20SN, psuAddress, maxRetry=MAX_I2C_RETRY, waitRetry=WAIT_I2C_RETRY):
    """
    Read the status, voltage, and current of the power supply unit at the
    provided I2C address.
    """
    
    data = {}
    for attempt in range(maxRetry+1):
        if attempt != 0:
            time.sleep(waitRetry)
            
        try:
            p = subprocess.Popen(['/usr/local/bin/readPSU', str(sub20SN), '0x%02X' % psuAddress],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 text=True)
            output, output2 = p.communicate()
            
            if p.returncode == 0:
                psu, desc, onoffHuh, statusHuh, voltageV, currentA, = output.replace('\n', '').split(None, 5)
                data = {'address': psu,
                        'description': desc,
                        'voltage': float(voltageV),
                        'current': float(currentA),
                        'onoff': '%-3s' % onoffHuh,
                        'status': statusHuh
                       }
                break
            else:
                aspSUB20Logger.warning("%s: SUB-20 S/N %s command %i of %i returned %i; '%s;%s'", inspect.stack()[0][3], sub20SN, attempt, maxRetry, p.returncode, output, output2)
                
        except Exception as e:
            aspSUB20Logger.warning("Could not read PSU status: %s", str(e))
            
    return data
                

def psuCountTemperature(sub20SN, maxRetry=MAX_I2C_RETRY, waitRetry=WAIT_I2C_RETRY):
    """
    Return the number of temperature sensors associated with the power supply
    units.
    """
    
    ntemp = 0
    for attempt in range(maxRetry+1):
        if attempt != 0:
            time.sleep(waitRetry)
            
        try:
            p = subprocess.Popen(['/usr/local/bin/countThermometers', str(sub20SN)],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                text=True)
            output, output2 = p.communicate()
            
            if p.returncode == 0:
                aspSUB20Logger.warning("%s: SUB-20 S/N %s command %i of %i returned %i; '%s;%s'", inspect.stack()[0][3], sub20SN, attempt, maxRetry, p.returncode, output, output2)
            else:
                ntemp = p.returncode // 256
                break;
                
        except Exception as e:
            aspSUB20Logger.warning("Could not count PSU temperature sensors: %s", str(e))
            
    return ntemp


def psuTemperature(sub20SN, maxRetry=MAX_I2C_RETRY, waitRetry=WAIT_I2C_RETRY):
    """
    Return a list of dictionaries containing power supply unit info and temperatures.
    """
    
    temps = []
    for attempt in range(maxRetry+1):
        if attempt == 0:
            time.sleep(waitRetry)
            
        try:
            p = subprocess.Popen(['/usr/local/bin/readThermometers', str(sub20SN)],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                text=True)
            output, output2 = p.communicate()
            
            if p.returncode != 0:
                aspSUB20Logger.warning("%s: SUB-20 S/N %s command %i of %i returned %i; '%s;%s'", inspect.stack()[0][3], sub20SN, attempt, maxRetry, p.returncode, output, output2)
            else:
                for i,line in enumerate(output.split('\n')):
                    if len(line) < 4:
                        continue
                    psu, desc, tempC = line.split(None, 2)
                    temps.append({'address': psu,
                                'description': desc,
                                'temp_C': float(tempC)
                                })
                break
                
        except Exception as e:
            aspSUB20Logger.warning("Could not poll PSU temperature sensors: %s", str(e))
            
    return temps


def rs485CountBoards(sub20Mapper, maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY):
    """
    Count the number of PIC devices on all known SUB-20s.
    """
    
    nBoards = 0
    overallStatus = True
    for sub20SN in sorted(sub20Mapper):
        attempt = 0
        status = False
        while ((not status) and (attempt <= maxRetry)):
            if attempt != 0:
                time.sleep(waitRetry)
                
            p = subprocess.Popen(['/usr/local/bin/countPICs', str(sub20SN)],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                 text=True)
            output, output2 = p.communicate()
            
            if p.returncode == 0:
                aspSUB20Logger.warning("%s: SUB-20 S/N %s command %i of %i returned %i; '%s;%s'", inspect.stack()[0][3], sub20SN, attempt, maxRetry, p.returncode, output, output2)
                status = False
            else:
                nBoards += p.returncode
                status = True
            attempt += 1
            
        overallStatus &= status
        
    if not overallStatus:
        nBoards = 0
        
    return nBoards


def rs485Reset(sub20Mapper2, maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY):
    """
    Set a reset command to all of the ARX boards connected to the RS485 bus.
    Returns True if all of the boards have been reset, False otherwise.
    """
    
    success = True
    for sub20SN in sorted(sub20Mapper2.keys()):
        for board_key in sub20Mapper2[sub20SN]:
            board = (int(board_key) % 126) or 126
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    p = subprocess.Popen(['/usr/local/bin/sendPICDevice', str(sub20SN), str(board), ' RSET'],
                                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                         text=True)
                    output, output2 = p.communicate()
                    
                    if p.returncode == 0:
                        board_success = True
                        break
                    else:
                        raise RuntimeError("Non-zero return code: %s" % output2.strip().replace('\n', ' - '))
                        
                except Exception as e:
                    aspSUB20Logger.warning("Could not reset board %s: %s", board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    # Check for completion of reset
    time.sleep(10) # Wait a little bit
    reset_check, failed = rs485Check(portName, antennaMapping, verbose=False)
    success &= reset_check
    
    return success


def rs485Sleep(sub20Mapper2, maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY):
    """
    Set a sleep command to all of the ARX boards connected to the RS485 bus.
    Returns True if all of the boards have been put to bed, False otherwise.
    """
    
    success = True
    for sub20SN in sorted(sub20Mapper2.keys()):
        for board_key in sub20Mapper2[sub20SN]:
            board = (int(board_key) % 126) or 126
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    p = subprocess.Popen(['/usr/local/bin/sendPICDevice', str(sub20SN), str(board), ' SLEP'],
                                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                         text=True)
                    output, output2 = p.communicate()
                    
                    if p.returncode == 0:
                        board_success = True
                        break
                    else:
                        raise RuntimeError("Non-zero return code: %s" % output2.strip().replace('\n', ' - '))
                        
                except Exception as e:
                    aspSUB20Logger.warning("Could not sleep board %s: %s", board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    return success


def rs485Wake(sub20Mapper2, maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY):
    """
    Set a wake command to all ARX boards connected to the RS485 bus.  Returns
    True if all of the boards have woken up, False otherwise.
    """
    
    success = True
    for sub20SN in sorted(sub20Mapper2.keys()):
        for board_key in sub20Mapper2[sub20SN]:
            board = (int(board_key) % 126) or 126
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    p = subprocess.Popen(['/usr/local/bin/sendPICDevice', str(sub20SN), str(board), 'WAKE'],
                                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                         text=True)
                    output, output2 = p.communicate()
                    
                    if p.returncode == 0:
                        board_success = True
                        break
                    else:
                        raise RuntimeError("Non-zero return code: %s" % output2.strip().replace('\n', ' - '))
                        
                except Exception as e:
                    aspSUB20Logger.warning("Could not wake board %s: %s", board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    # Check for completion of wake
    time.sleep(10) # Wait a little bit
    wake_check, failed = rs485Check(portName, antennaMapping, verbose=False)
    success &= wake_check
    
    return success


def rs485Check(sub20Mapper2, maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY, verbose=False):
    """
    Ping each of the ARX boards connected to the RS485 bus.  Returns a two-
    element tuple of:
     * True if all boards were pinged, false otherwise
     * a list of any boards that failed to respond
    """
    
    data = "check_for_me"
    
    success = True
    failed = []
    for sub20SN in sorted(sub20Mapper2.keys()):
        for board_key in sub20Mapper2[sub20SN]:
            board = (int(board_key) % 126) or 126
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    p = subprocess.Popen(['/usr/local/bin/sendPICDevice', '-v', '-d', str(sub20SN), str(board), 'ECHO%s' % data],
                                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                         text=True)
                    output, output2 = p.communicate()
                    
                    if p.returncode == 0 and output.find(data) != -1:
                        board_success = True
                        break
                    else:
                        raise RuntimeError("Non-zero return code: %s" % output2.strip().replace('\n', ' - '))
                        
                except Exception as e:
                    if verbose:
                        aspSUB20Logger.warning("Could not echo '%s' to board %s: %s", data, board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            if not board_success:
                failed.append(antennaMapping[board_key])
                
    return success, failed


def rs485SetTime(sub20Mapper2, maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY, verbose=False):
    """
    Get the board time on all ARX boards connected to the RS485 bus. Returns a
    three-element tuple of:
     * True if all boards were pinged, false otherwise
     * a list of any boards that failed to respond
     * the time set
    """
    
    data = "%08X" % int(time.time())
    success = True
    failed = []
    for sub20SN in sorted(sub20Mapper2.keys()):
        for board_key in sub20Mapper2[sub20SN]:
            board = (int(board_key) % 126) or 126
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    p = subprocess.Popen(['/usr/local/bin/sendPICDevice', str(sub20SN), str(board), ' STIM%s' % data],
                                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                         text=True)
                    output, output2 = p.communicate()
                    
                    if p.returncode == 0:
                        board_success = True
                        break
                    else:
                        raise RuntimeError("Non-zero return code: %s" % output2.strip().replace('\n', ' - '))
                        
                except Exception as e:
                    if verbose:
                        aspSUB20Logger.warning("Could not set time to '%s' on board %s: %s", data, board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            if not board_success:
                failed.append(antennaMapping[board_key])
                
    return success, failed, int(data, 16)


def rs485GetTime(sub20Mapper2, maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY, verbose=False):
    """
    Poll all of the Rev H ARX boards on the RS485 bus and return a two-element
    tuple of:
     * True if all boards were pinged, false otherwise
     * a list of board times.
     
    Any board that failed to respond will have its time reported as zero.
    """
    
    gtimRE = re.compile(r'Board Time: (?<gtim>\d*) s')
    
    success = True
    data = []
    for sub20SN in sorted(sub20Mapper2.keys()):
        for board_key in sub20Mapper2[sub20SN]:
            board = (int(board_key) % 126) or 126
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    p = subprocess.Popen(['/usr/local/bin/sendPICDevice', '-v', '-d', str(sub20SN), str(board), 'GTIM'],
                                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                         text=True)
                    output, output2 = p.communicate()
                    
                    mtch = gtimRE.search(output)
                    if mtch is not None:
                        gtim_data = mtch.group('gtim')
                        data.append(int(gtim_data, 10))
                        board_success = True
                        break
                    else:
                        raise RuntimeError("Non-zero return code: %s" % output2.strip().replace('\n', ' - '))
                        
                except Exception as e:
                    if verbose:
                        aspSUB20Logger.warning("Could not get time from board %s: %s", board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            if not board_success:
                data.append(0)
                
    return success, data


def rs485Power(sub20Mapper2, maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY):
    """
    Poll all of the ARX boards connected to the RS485 bus and return a two-
    element tuple of:
     * True if all board were successfully polled, False otherwise
     * a list of FEE currents (16/board)
    """
    
    curaRE = re.compile(r'(?P<chan>\d*): (?P<curr>\d*\.\d*) mA')
    
    success = True
    fees = []
    for sub20SN in sorted(sub20Mapper2.keys()):
        for board_key in sub20Mapper2[sub20SN]:
            board = (int(board_key) % 126) or 126
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    p = subprocess.Popen(['/usr/local/bin/sendPICDevice', '-v', '-d', str(sub20SN), str(board), 'CURA'],
                                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                         text=True)
                    output, output2 = p.communicate()
                    
                    if p.returncode == 0:
                        for line in filter(lambda x: x.find(' mA') != -1, output.split('\n')):
                            mtch = curaRE.search(line)
                            if mtch is not None:
                                fees.append(float(mtch.group('curr')))
                            else:
                                fees.append(-1.0)
                        board_success = True
                        break
                    else:
                        raise RuntimeError("Non-zero return code: %s" % output2.strip().replace('\n', ' - '))
                        
                except Exception as e:
                    aspSUB20Logger.warning("Could not get power info. for board %s: %s", board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    return success, fees


def rs485RFPower(sub20Mapper2, maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY):
    """
    Poll all of the ARX boards connected to the RS485 bus and return a two-
    element tuple of:
     * True if all board were successfully polled, False otherwise
     * a list of per-channel RF powers (16/board)
    """
    
    powaRE = re.compile(r'(?P<chan>\d*): (?P<pow>\d*\.\d*) uW')
    
    success = True
    rf_powers = []
    for sub20SN in sorted(sub20Mapper2.keys()):
        for board_key in sub20Mapper2[sub20SN]:
            board = (int(board_key) % 126) or 126
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    p = subprocess.Popen(['/usr/local/bin/sendPICDevice', '-v', '-d', str(sub20SN), str(board), 'POWA'],
                                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                         text=True)
                    output, output2 = p.communicate()
                    
                    if p.returncode == 0:
                        for line in filter(lambda x: x.find(' uW') != -1, output.split('\n')):
                            mtch = powaRE.search(line)
                            if mtch is not None:
                                rf_powers.append(float(mtch.group('pow')))
                            else:
                                rf_powers.append(-1.0)
                        board_success = True
                        break
                    else:
                        raise RuntimeError("Non-zero return code: %s" % output2.strip().replace('\n', ' - '))
                        
                except Exception as e:
                    aspSUB20Logger.warning("Could not get RF power info. for board %s: %s", board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    return success, rf_powers


def rs485Temperature(sub20Mapper2, maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY):
    """
    Poll all of the Rev H ARX boards connected to the RS485 bus and return a
    two-element tuple of:
     * True if all board were successfully polled, False otherwise
     * a list of temperatures (typically 3/board)
    """
    
    owteRE = re.compile(r'(?P<chan>\d*): (?P<temp>\d*\.\d*) C')
    
    success = True
    temps = []
    for sub20SN in sorted(sub20Mapper2.keys()):
        for board_key in sub20Mapper2[sub20SN]:
            board = (int(board_key) % 126) or 126
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    p = subprocess.Popen(['/usr/local/bin/sendPICDevice', '-v', '-d', str(sub20SN), str(board), 'OWTE'],
                                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                         text=True)
                    output, output2 = p.communicate()
                    
                    if p.returncode == 0:
                        for line in filter(lambda x: x.find(' C') != -1, output.split('\n')):
                            mtch = owteRE.search(line)
                            if mtch is not None:
                                temps.append(float(mtch.group('temp')))
                            else:
                                temps.append(-99.0)
                        board_success = True
                        break
                    else:
                        raise RuntimeError("Non-zero return code: %s" % output2.strip().replace('\n', ' - '))
                        
                except Exception as e:
                    aspSUB20Logger.warning("Could not get temperature info. for board %s: %s", board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    return success, temps
