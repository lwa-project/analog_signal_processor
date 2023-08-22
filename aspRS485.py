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

__version__ = '0.3'
__all__ = ['rs485CountBoards', 'rs485Reset', 'rs485Check', 'rs485Get',
           'rs485Send', 'rs485Power', 'rs485RFPower', 'rs485Temperature']


aspRS485Logger = logging.getLogger('__main__')


RS485_LOCK = threading.Semaphore(1)


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


def rs485CountBoards(antennaMapping, maxRetry=0, waitRetry=0.2):
    found = 0
    with RS485_LOCK:
        for board_key in antennaMapping.keys():
            board = int(board_key)
            for attempt in range(maxRetry+1):
                try:
                    _ARX.get_board_info(board & 0xFF)
                    found += 1
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not query info. for board %s: %s", board_key, str(e))
                    time.sleep(waitRetry)
    return found


def rs485Reset(antennaMapping, maxRetry=0, waitRetry=0.2):
    success = True
    with RS485_LOCK:
        for board_key in antennaMapping.keys():
            board = int(board_key)
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    _ARX.reset(board & 0xFF)
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


def rs485Check(antennaMapping, maxRetry=0, waitRetry=0.2, verbose=False):
    data = "check_for_me"
    
    success = True
    failed = []
    with RS485_LOCK:
        for board_key in antennaMapping.keys():
            board = int(board_key)
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    echo_data = _ARX.echo(board & 0xFF, data)
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


def rs485Get(stand, antennaMapping, maxRetry=0, waitRetry=0.2):
    config = []
    if stand == 0:
        with RS485_LOCK:
            for board_key in antennaMapping.keys():
                board = int(board_key)
                for attempt in range(maxRetry+1):
                    try:
                        board_config = _ARX.get_all_chan_cfg(board & 0xFF)
                        config.extend(board_config)
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
                    chan_config0 = _ARX.get_chan_cfg(board & 0xFF, chan0)
                    chan_config1 = _ARX.get_chan_cfg(board & 0xFF, chan1)
                    config.append(chan_config0)
                    config.append(chan_config1)
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not get channel config. for board %s: %s", board, str(e))
                    time.sleep(waitRetry)
                    
    return config


def rs485Send(stand, config, antennaMapping, maxRetry=0, waitRetry=0.2):
    success = True
    if stand == 0:
        with RS485_LOCK:
            for board_key in antennaMapping.keys():
                board = int(board_key)
                board_success = False
                for attempt in range(maxRetry+1):
                    try:
                        # THIS IS ALL DEBUG GARBAGE
                        config_start = 2*(antennaMapping[board_key][0]-1)
                        config_end = 2*(antennaMapping[board_key][1])
                        subconfig = config[config_start:config_end]
                        for i,c in enumerate(subconfig):
                             _ARX.set_chan_cfg(board & 0xFF, i, c)
                        #_ARX.set_all_different_chan_cfg(board & 0xFF, subconfig)
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
                    _ARX.set_chan_cfg(board & 0xFF, chan0, config[0])
                    _ARX.set_chan_cfg(board & 0xFF, chan1, config[1])
                    board_success = True
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not set channel info. for board %s: %s", board, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    return success


def rs485Power(antennaMapping, maxRetry=0, waitRetry=0.2):
    success = True
    boards = []
    fees = []
    with RS485_LOCK:
        for board_key in antennaMapping.keys():
            board = int(board_key)
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
                    aspRS485Logger.warning("Could not get power info. for board %s: %s", board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    return success, boards, fees


def rs485RFPower(antennaMapping, maxRetry=0, waitRetry=0.2):
    success = True
    rf_powers = []
    with RS485_LOCK:
        for board_key in antennaMapping.keys():
            board = int(board_key)
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    new_rf_powers = _ARX.get_all_chan_power(board & 0xFF)
                    rf_powers.extend(new_rf_powers)
                    board_success = True
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not get power info. for board %s: %s", board_key, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    return success, rf_powers


def rs485Temperature(antennaMapping, maxRetry=0, waitRetry=0.2):
    success = True
    temps = []
    with RS485_LOCK:
        for board_key in antennaMapping.keys():
            board = int(board_key)
            board_success = False
            for attempt in range(maxRetry+1):
                try:
                    new_temps = _ARX.get_1wire_temp(board & 0xFF)
                    temps.extend(new_temps)
                    board_success = True
                    break
                except Exception as e:
                    aspRS485Logger.warning("Could not get temperature info. for board %s: %s", board_kety, str(e))
                    time.sleep(waitRetry)
            success &= board_success
            
    return success, temps
