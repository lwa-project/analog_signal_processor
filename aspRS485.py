# -*- coding: utf-8 -*-
"""
Module for storing the various RS485 function calls
"""

import time
import logging
import threading
import subprocess

from lwautils import lwa_arx
_ARX = lwa_arx.ARX()

from aspCommon import RS485_LOCK, RS485_ANTENNA_MAPPING, MAX_RS485_RETRY, WAIT_RS485_RETRY

__version__ = '0.1'
__all__ = ['rs485CountBoards', 'rs485Reset', 'rs485Check', 'rs485Get',
           'rs485Send', 'rs485Power']


aspRS485Logger = logging.getLogger('__main__')


def _stand_to_board_chans(stand):
    board = None
    chan0, chan1 = 0, 1
    for board,stands in RS485_ANTENNA_MAPPING.items():
        if stand >= stands[0] and stand <= stands[1]:
            chan = stand - stands[0]
            chan0 = 2*chan
            chan1 = chan0 + 1
            break
    return board, chan0, chan1


def rs485CountBoards(maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY):
    found = 0
    with RS485_LOCK:
        for board in RS485_ANTENNA_MAPPING.keys():
            for attempt in range(maxRetry+1):
                try:
                    _ARX.get_board_info(board & 0xFF)
                    found += 1
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not query info. for board %s: %s", board, str(e))
                    time.sleep(waitRetry)
    return found


def rs485Reset(maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY):
    success = True
    with RS485_LOCK:
        for board in RS485_ANTENNA_MAPPING.keys():
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    _ARX.reset(board & 0xFF)
                    board_success = True
                except Exception as e:
                    aspRS485Logger.warning("Could not reset board %s: %s", board, str(e))
                    time.sleep(waitRetry)
            success &= board_success
    return success


def rs485Check(maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY):
    data = "check_for_me"
    
    success = True
    failed = []
    with RS485_LOCK:
        for board in RS485_ANTENNA_MAPPING.keys():
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    echo_data = _ARX.echo(board & 0xFF,data)
                except Exception as e:
                    aspRS485Logger.warning("Could not echo '%s' to board %s: %s", data, board, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            if not board_success:
                failed.append(RS485_ANTENNA_MAPPING[board])
    return success, failed


def rs485Get(stand, maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY):
    config = []
    if stand == 0:
        with RS485_LOCK:
            for board in RS485_ANTENNA_MAPPING.keys():
                for attempt in range(maxRetry+1):
                    try:
                        aspRS485Logger.debug('Dummy1')
                        dummy = _ARX._send(board&0xFF, 'geta', '', 15)
                        aspRS485Logger.debug('Dummy2')
                        board_config = _ARX.get_all_chan_cfg(board & 0xFF)
                        aspRS485Logger.debug('Dummy3')
                        config.extend(board_config)
                        aspRS485Logger.debug('Dummy4')
                        break
                    except Exception as e:
                        aspRS485Logger.warning("Could not get channel info. for board %s: %s", board, str(e))
                        time.sleep(waitRetry)
    else:
        board, chan0, chan1 = _stand_to_board_chan(stand)
        if board is None:
            aspRS485Logger.warning("Unable to relate stand %i to a RS485 board", stand)
            return False
            
        with RS485_LOCK:
            for attempt in range(maxRetry+1):
                try:
                    chan_config0 = _ARX.get_chan_cfg(board & 0xFF, chan0)
                    chan_config1 = _ARX.get_chan_cfg(board & 0xFF, chan1)
                    config.append(chan_config0)
                    config.append(chan_config1)
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not get channel config. for board %s: %s", board, str(e))
                    time.sleep(waitRetry)
                    
    return config


def rs485Send(stand, config, maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY):
    success = True
    if stand == 0:
        with RS485_LOCK:
            for board in RS485_ANTENNA_MAPPING.keys():
                board_success = False
                for attempt in range(maxRetry+1):
                    try:
                        _ARX.set_all_different_chan_config(board & 0xFF, config)
                        board_success = True
                        break
                    except Exception as e:
                        aspRS485Logger.warning("Could not set channel config. for board %s: %s", board, str(e))
                        time.sleep(waitRetry)
                success &= board_success
                
    else:
        board, chan0, chan1 = _stand_to_board_chan(stand)
        if board is None:
            aspRS485Logger.warning("Unable to relate stand %i to a RS485 board", stand)
            return False
            
        with RS485_LOCK:
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    _ARX.set_chan_cfg(board & 0xFF, chan0, config[0])
                    _ARX.set_chan_cfg(board & 0xFF, chan1, config[1])
                    board_success = True
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not set channel info. for board %s: %s", board, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    return success


def rs485Power(maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY):
    success = True
    boards = []
    fees = []
    with RS485_LOCK:
        for board in RS485_ANTENNA_MAPPING.keys():
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    new_board = _ARX.get_board_current(board & 0xFF)
                    new_fees = _ARX.get_all_chan_current(board & 0xFF)
                    boards.append(new_board)
                    fees.extend(new_fees)
                    board_success = True
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not get power info. for board %s: %s", board, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    return success, boards, fees
