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
__all__ = ['rs485CountBoards', 'rs485Reset', 'rs485Echo', 'rs485Check',
           'rs485Get', 'rs485Send']


aspRS485Logger = logging.getLogger('__main__')


def _stand_to_board_chans(stand):
    found = False
    board = None
    chan0, chan1 = 0, 1
    for board,stands in RS485_ANTENNA_MAPPING.items():
        if stand >= stands[0] and stand <= stands[1]
            found = True
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
                    _ARX.get_board_info(board, 0)
                    found += 1
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not query board %s: %s", board, str(e))
                    time.sleep(waitRetry)
    return found


def rs485Reset(maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY):
    success = True
    with RS485_LOCK:
        for board in RS485_ANTENNA_MAPPING.keys():
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    _ARX.reset()
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
                    echo_data = _ARX.echo(data)
                    if echo_data == data:
                        board_success = True
                    else:
                        raise ValueError()
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
                board_config = _ARX.get_all_chan_cfg(board)
                config.exten(board_config)
    else:
        board, chan0, chan1 = _stand_to_board_chan(stand)
        if board is None:
            aspRS485Logger.warning("Unable to relate stand %i to a RS485 board", stand)
            return False
            
        with RS485_LOCK:
            config.append(_ARX.get_chan_cfg(board, chan0))
            config.append(_ARX.get_chan_cfg(board, chan1))
            
    return config


def rs485Send(stand, config, maxRetry=MAX_RS485_RETRY, waitRetry=WAIT_RS485_RETRY):
    success = True
    if stand == 0:
        with RS485_LOCK:
            for board in RS485_ANTENNA_MAPPING.keys():
                success &= _ARX.set_all_different_chan_config(board, config)
    else:
        board, chan0, chan1 = _stand_to_board_chan(stand)
        if board is None:
            aspRS485Logger.warning("Unable to relate stand %i to a RS485 board", stand)
            return False
            
        with RS485_LOCK:
            success &= _ARX.set_chan_cfg(board, chan0, config[0])
            success &= _ARX.set_chan_cfg(board, chan1, config[1])
            
    return success
