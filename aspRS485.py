# -*- coding: utf-8 -*-
"""
Module for storing the various RS485 function calls
"""

import time
import serial
import logging
import threading
import subprocess
import contextlib


__version__ = '0.5'
__all__ = ['ChannelConfig', 'rs485CountBoards', 'rs485Reset', 'rs485Check',
           'rs485SetTime', 'rs485GetTime', 'rs485Get', 'rs485Send',
           'rs485Power', 'rs485RFPower', 'rs485Temperature']


aspRS485Logger = logging.getLogger('__main__')


RS485_LOCK = threading.Semaphore(1)


class ChannelConfig():
    """
    Class to help go to/from the 16-bit channel configuration value used
    by the Rev H boards.
    """
    
    def __init__(self, narrow_hpf=False, narrow_lpf=False, at1=31.5, at2=31.5, dc_on=False, sig_on=False):
        self.narrow_hpf = narrow_hpf
        self.narrow_lpf = narrow_lpf
        self.at1 = round(at1*2)/2
        self.at2 = round(at2*2)/2
        self.dc_on = dc_on
        self.sig_on = sig_on
        
    @classmethod
    def from_raw(kls, raw):
        """
        Construct a new ChannelConfig instance from a 16-bit configuration
        value.
        """
        
        k = kls()
        
        if isinstance(raw, (str, bytes)):
            raw = int(raw, 16)
            
        k.narrow_hpf = (raw & 1) == 1
        k.sig_on = ((raw >> 1) & 1) == 1
        k.narrow_lpf = ((raw >> 2) & 1) == 1
        k.at1 = (((raw ^ 0xFFFF) >> 3) & 0x3F) * 0.5
        k.at2 = (((raw ^ 0xFFFF) >> 9) & 0x3F) * 0.5
        k.dc_on = ((raw >> 15) & 1) == 1
        return k
        
    @property
    def raw(self):
        """
        Render the channel configuration as a four character hexadecimal
        string.
        """
        
        value = 0
        value |= int(self.narrow_hpf)
        value |= (int(self.sig_on) << 1)
        value |= (int(self.narrow_lpf) << 2)
        value |= (((int(round(self.at1*2)) ^ 0xFFFF) & 0x3F) << 3)
        value |= (((int(round(self.at2*2)) ^ 0xFFFF) & 0x3F) << 9)
        value |= (int(self.dc_on) << 15)
        return "%04X" % value


def _send_command(portName, addr, cmd, data=None, timeout=1.0):
    addr = (0x80 + addr) & 0xFF
    if not isinstance(cmd, bytes):
        cmd = cmd.encode('ascii')
    if cmd == b'WAKE':
        cmd = b'W'
    if data is None:
        data = ''
    if not isinstance(data, bytes):
        data = data.encode('ascii')
        
    with serial.Serial(port=portName, baudrate=19200, parity=serial.PARITY_NONE,
                         stopbits=serial.STOPBITS_ONE, bytesize=serial.EIGHTBITS,
                         timeout=timeout, writeTimeout=0) as port:
        port.write(bytes(addr) + cmd + data + b'\r')
        resp = port.read_until(b'\r', 80)
        if cmd in (b'RSET', b'SLEP', b'W'):
            return b''
            
        else:
            if len(resp) > 0:
                status = resp[0] == bytes(0x06)
                resp = resp[1:]
                if not status:
                    errcode = {b'\x01': 'command code was not recognized ',
                               b'\x02': 'command was too long',
                               b'\x03': 'command failed'}[resp[1]]
                    
            else:
                errcode = 'timeout'
                
            if not status:
                raise RuntimeError("Command '%s' failed: %s" % (cmd, errcode))
                
        return resp


def _stand_to_board_chans(stand, antennaMapping):
    board = None
    chan0, chan1 = 0, 1
    for board,stands in antennaMapping.items():
        board = int(board)
        if stand >= stands[0] and stand <= stands[1]:
            chan = stand - stands[0]
            chan0 = 2*chan
            chan1 = chan0 + 1
            break
    return board, chan0, chan1


def rs485CountBoards(portName, antennaMapping, maxRetry=0, waitRetry=0.2):
    """
    Find and return the number of Rev H ARX boards connected to the RS485 bus.
    """
    
    found = 0
    with RS485_LOCK:
        for board_key in antennaMapping.keys():
            board = int(board_key)
            for attempt in range(maxRetry+1):
                try:
                    _send_command(portName, board, 'ARXN')
                    found += 1
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not query info. for board %s: %s", board_key, str(e))
                    time.sleep(waitRetry)
    return found


def rs485Reset(portName, antennaMapping, maxRetry=0, waitRetry=0.2):
    """
    Set a reset command to all of the Rev H ARX boards connected to the RS485
    bus.  Returns True if all of the boards have been reset, False otherwise.
    """
    
    success = True
    with RS485_LOCK:
        for board_key in antennaMapping.keys():
            board = int(board_key)
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    _send_command(portName, board, 'RSET')
                    board_success = True
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not reset board %s: %s", board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success

    # Check for completion of reset
    time.sleep(10) # Wait a little bit
    reset_check, failed = rs485Check(antennaMapping, verbose=False)
    success &= reset_check

    return success


def rs485Check(portName, antennaMapping, maxRetry=0, waitRetry=0.2, verbose=False):
    """
    Ping each of the Rev H ARX boards connected to the RS485 bus.  Returns
    a two-element tuple of:
     * True if all boards were pinged, false otherwise
     * a list of any boards that failed to respond
    """
    
    data = "check_for_me"
    
    success = True
    failed = []
    with RS485_LOCK:
        for board_key in antennaMapping.keys():
            board = int(board_key)
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    echo_data = _send_command(portName, 'ECHO', data=data)
                    board_success = True
                    break
                except Exception as e:
                    if verbose:
                        aspRS485Logger.warning("Could not echo '%s' to board %s: %s", data, board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            if not board_success:
                failed.append(antennaMapping[board_key])
                
    return success, failed


def rs485SetTime(portName, antennaMapping, maxRetry=0, waitRetry=0.2):
    """
    Get the board time on all Rev H ARX boards connected to the RS485 bus.
    Returns a three-element tuple of:
     * True if all boards were pinged, false otherwise
     * a list of any boards that failed to respond
     * the time set
    """
    
    data = "08X" % int(time.time())
    success = True
    failed = []
    with RS485_LOCK:
        for board_key in antennaMapping.keys():
            board = int(board_key)
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    _send_command(portName, 'STIM', data=data)
                    board_success = True
                    break
                except Exception as e:
                    if verbose:
                        aspRS485Logger.warning("Could not set time to '%s' on board %s: %s", data, board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            if not board_success:
                failed.append(antennaMapping[board_key])
                
    return success, failed, int(data, 16)


def rs485GetTime(portName, antennaMapping, maxRetry=0, waitRetry=0.2):
    """
    Poll all of the Rev H ARX boards on the RS485 bus and return a list of
    board times.  Any board that failed to respond will have its time reported
    as zero.
    """
    
    data = []
    with RS485_LOCK:
        for board_key in antennaMapping.keys():
            board = int(board_key)
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    gtim_data = _send_command(portName, 'GTIM')
                    data.append(int(gtim_data, 16))
                    board_success = True
                    break
                except Exception as e:
                    if verbose:
                        aspRS485Logger.warning("Could not get time from board %s: %s", board_key, str(e))
                    time.sleep(waitRetry)
            if not board_success:
                data.append(0)
                
    return data


def rs485Get(stand, portName, antennaMapping, maxRetry=0, waitRetry=0.2):
    """
    Return a list of ChannelConfig instances for the specified stands.
    """
    
    config = []
    if stand == 0:
        with RS485_LOCK:
            for board_key in antennaMapping.keys():
                board = int(board_key)
                for attempt in range(maxRetry+1):
                    try:
                        raw_board_config = _send_command(portName, board, 'GETA')
                        for i in range(16):
                            config.append(ChannelConfig.from_raw(raw_board_config[2*i:2*i+2]))
                        break
                    except Exception as e:
                        aspRS485Logger.warning("Could not get channel info. for board %s: %s", board_key, str(e))
                        time.sleep(waitRetry)
    else:
        board, chan0, chan1 = _stand_to_board_chans(stand, antennaMapping)
        if board is None:
            aspRS485Logger.warning("Unable to relate stand %i to a RS485 board", stand)
            return False
            
        with RS485_LOCK:
            for attempt in range(maxRetry+1):
                try:
                    raw_chan_config0 = _send_command(portName, board, 'GETC', data="%X" % chan0)
                    raw_chan_config1 = _send_command(portName, board, 'GETC', data="%X" % chan1)
                    config.append(ChannelConfig.from_raw(raw_chan_config0))
                    config.append(ChannelConfig.from_raw(raw_chan_config1))
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not get channel config. for board %s: %s", board, str(e))
                    time.sleep(waitRetry)
                    
    return config


def rs485Send(stand, config, portName, antennaMapping, maxRetry=0, waitRetry=0.2):
    """
    Give a stand and a list of ChannelConfig instances, set the stand
    configuration.
    """
    
    success = True
    if stand == 0:
        with RS485_LOCK:
            for board_key in antennaMapping.keys():
                board = int(board_key)
                board_success = False
                for attempt in range(maxRetry+1):
                    try:
                        config_start = 2*(antennaMapping[board_key][0]-1)
                        config_end = 2*(antennaMapping[board_key][1])
                        subconfig = config[config_start:config_end]
                        
                        raw_config = b''
                        for sc in sub_config:
                            raw_config += sc.raw
                        _send_command(portName, board, 'SETA', data=raw_config)
                        board_success = True
                        break
                    except Exception as e:
                        aspRS485Logger.warning("Could not set channel config. for board %s: %s", board_key, str(e))
                        time.sleep(waitRetry)
                success &= board_success
                
    else:
        board, chan0, chan1 = _stand_to_board_chans(stand, antennaMapping)
        if board is None:
            aspRS485Logger.warning("Unable to relate stand %i to a RS485 board", stand)
            return False
            
        with RS485_LOCK:
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    _send_command(portName, board, "SETC%X" % chan0, config[0].raw)
                    _send_command(portName, board, "SETC%X" % chan1, config[1].raw) _config_to_raw(config[1]))
                    board_success = True
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not set channel info. for board %s: %s", board, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    return success


def rs485Power(portName, antennaMapping, maxRetry=0, waitRetry=0.2):
    """
    Poll all of the Rev H ARX boards connected to the RS485 bus and return a
    three-element tuple of:
     * True if all board were successfully polled, False otherwise
     * a list of board currents (1/board)
     * a list of FEE currents (16/board)
    """
    
    success = True
    boards = []
    fees = []
    with RS485_LOCK:
        for board_key in antennaMapping.keys():
            board = int(board_key)
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    raw_board = _send_command(portName, board, 'CURB')
                    raw_board = (ord(raw_board[0]) << 8) | ord(raw_board[1])
                    boards.append(int(raw_board, 16) * 0.008)
                    raw_fees = _send_command(portName, board, 'CURA')
                    for i in range(16):
                        fees.append(int(raw_fees[4*i:4*(i+1)], 16) * 0.004 * 100)
                    board_success = True
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not get power info. for board %s: %s", board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    return success, boards, fees


def rs485RFPower(portName, antennaMapping, maxRetry=0, waitRetry=0.2):
    """
    Poll all of the Rev H ARX boards connected to the RS485 bus and return a
    two-element tuple of:
     * True if all board were successfully polled, False otherwise
     * a list of per-channel RF powers (16/board)
    """
    
    success = True
    rf_powers = []
    with RS485_LOCK:
        for board_key in antennaMapping.keys():
            board = int(board_key)
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    raw_rf_power = _send_command(portName, board, 'POWA')
                    for i in range(16):
                        v = int(raw_rf_power[4*i:4*(i+1)], 16) * 0.004
                        rf_powers.append((v/2.296)**2/50)
                    board_success = True
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not get RF power info. for board %s: %s", board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    return success, rf_powers


def rs485Temperature(portName, antennaMapping, maxRetry=0, waitRetry=0.2):
    """
    Poll all of the Rev H ARX boards connected to the RS485 bus and return a
    two-element tuple of:
     * True if all board were successfully polled, False otherwise
     * a list of temperatures (typically 3/board)
    """
    
    success = True
    temps = []
    with RS485_LOCK:
        for board_key in antennaMapping.keys():
            board = int(board_key)
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    ntemp = _send_command(portName, board, 'OWDC')
                    ntemp = int(ntemp, 16)
                    raw_temps = _send_command(portName, board, 'OWTE')
                    for i in range(ntemp):
                        temps.append(int(raw_temps[4*i:4*(i+1)], 16)/16)
                    board_success = True
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not get temperature info. for board %s: %s", board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    return success, temps
