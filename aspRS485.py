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


__version__ = '0.4'
__all__ = ['rs485CountBoards', 'rs485Reset', 'rs485Check', 'rs485Get',
           'rs485Send', 'rs485Power', 'rs485RFPower', 'rs485Temperature']


aspRS485Logger = logging.getLogger('__main__')


RS485_LOCK = threading.Semaphore(1)


 @contextlib.contextmanager
def _open_port(portName, timeout=1.0):
    return serial.Serial(port=portName, baudrate=19200, parity=serial.PARITY_NONE,
                         stopbits=serial.STOPBITS_ONE, bytesize=serial.EIGHTBITS,
                         timeout=timeout, writeTimeout=0)


def _send_command(portName, addr, cmd, data=None):
    addr = 0x80 + (addr & 0xFF)
    if not isinstance(cmd, bytes):
        cmd = cmd.encode('utf-8')
    if data is None:
        data = ''
    if not isinstance(data, bytes):
        data = data.encode('utf-8')
        
    msg = bytes(addr) + cmd + data + b'\r'
    
    with _open_port(portName) as port:
        port.write(msg)
        resp = port.read_until(b'\r', 80)
        if cmd == 'RSET':
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


def _raw_to_config(raw):
    config = {'narrow_lpf': False,
              'narrow_hpf': False,
              'first_atten': 0.0,
              'second_atten': 0.0,
              'sig_on': False,
              'dc_on': False}
    
    raw = int(raw, 16)
    config['narrow_hpf'] = (raw & 1) == 1
    config['sig_on'] = ((raw >> 1) & 1) == 1
    config['narrow_lpf'] = ((raw >> 2) & 1) == 1
    config['first_atten'] = (((raw ^ 0xFFFF) >> 3) & 0x3F) * 0.5
    config['second_atten'] = (((raw ^ 0xFFFF) >> 9) & 0x3F) * 0.5
    config['dc_on'] = ((raw >> 15) & 1) == 1
    return config


def _config_to_raw(config):
    raw = 0
    raw |= int(config['narrow_hpf'])
    raw |= (int(config['sig_on']) << 1)
    raw |= (int(config['narrow_lpf']) << 2)
    raw |= (((int(round(config['first_atten']*2)) ^ 0xFFFF) & 0x3F) << 3)
    raw |= (((int(round(config['second_atten']*2)) ^ 0xFFFF) & 0x3F) << 9)
    raw |= (int(config['dc_on']) << 15)
    
    return "%04X" % raw


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


def rs485Get(stand, portName, antennaMapping, maxRetry=0, waitRetry=0.2):
    config = []
    if stand == 0:
        with RS485_LOCK:
            for board_key in antennaMapping.keys():
                board = int(board_key)
                for attempt in range(maxRetry+1):
                    try:
                        raw_board_config = _send_command(portName, board, 'GETA')
                        for i in range(16):
                            config.append(_raw_to_config(raw_board_config[2*i:2*i+2]))
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
                    config.append(_raw_to_config(raw_chan_config0))
                    config.append(_raw_to_config(raw_chan_config1))
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not get channel config. for board %s: %s", board, str(e))
                    time.sleep(waitRetry)
                    
    return config


def rs485Send(stand, config, portName, antennaMapping, maxRetry=0, waitRetry=0.2):
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
                            raw_config += _config_to_raw(sc)
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
                    _send_command(portName, board, "SETC%X" % chan0", _config_to_raw(config[0]))
                    _send_command(portName, board, "SETC%X" % chan1", _config_to_raw(config[1]))
                    board_success = True
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not set channel info. for board %s: %s", board, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    return success


def rs485Power(portName, antennaMapping, maxRetry=0, waitRetry=0.2):
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
