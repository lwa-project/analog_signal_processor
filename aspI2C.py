# -*- coding: utf-8 -*-
"""
Module for storing the various I2C function calls
"""

import re
import time
import serial
import logging
import threading

__version__ = '0.1'
__all__ = ['psuRead', 'psuSend', 'psuTemperatureRead']


aspI2CLogger = logging.getLogger('__main__')


def _read_until_done(fh):
    response = b''
    while response.find(b'Power status:') == -1:
        response += fh.readline()
    try:
        response = response.decode('ascii')
    except AttributeError:
        # Python2 catch
        pass
    return response


def psuRead(serialPort, psuAddress):
    success, voltage, current, onoff, status = True, 0.0, 0.0, "UNK", "UNK"
    # with serial.Serial(serialPort, baudrate=9600, timeout=1) as fh:
    #     fh.open()
    # 
    #     fh.write(b'device %i' % psuAddress)
    #     response = _read_until_done(fh)
    #     fh.write(b'voltage')
    #     response += _read_until_done(fh)
    #     fh.write(b'current')
    #     response += _read_until_done(fh)
    #     fh.write(b'summary')
    #     response += _read_until_done(fh)
    # 
    #     if response.find('ERROR') == -1:
    #         p = re.compile(r'Voltage: (?P<value>\d+\.\d+)')
    #         m = p.search(response)
    #         if m is not None:
    #             voltage = float(m.group('value'))
    #         p = re.compile(r'Current: (?P<value>\d+\.\d+)')
    #         m = p.search(response)
    #         if m is not None:
    #             current = float(m.group('value'))
    #         p = re.compile(r'Power status: (?P<value>[OoNnFf]+)')
    #         m = p.search(response)
    #         if m is not None:
    #             onoff = m.group('value')
    #         p = re.compile(r'Module \d+: .*')
    #         ms = p.findall(response)
    #         new_status = ''
    #         for m in ms:
    #             new_status += ' & '+m
    #         if new_status != '':
    #             status = new_status.replace('\n', '')
    #     else:
    #         success = False
    #         aspI2CLogger.warning("psuRead: Error during polling - %s", response)
            
    return success, voltage, current, onoff, status


def psuSend(serialPort, psuAddress, state):
    """
    Set the state of the power supply unit at the provided I2C address.
    """
    
    status = True
    # with serial.Serial(serialPort, baudrate=9600, timeout=1) as fh:
    #     fh.open()
    # 
    #     fh.write(b'device %i' % psuAddress)
    #     response = _read_until_done(fh)
    #     if response.find('ERROR'):
    #         status = False
    #     fh.write(b'on' if state == '11' else b'off')
    #     response += _read_until_done(fh)
    #     if response.find('ERROR'):
    #         status = False
    # 
    # if not status:
    #     aspI2CLogger.warning("psuSend: Error during polling - %s", response)
    return status


def psuTemperatureRead(serialPort, psuAddress):
    status, temperatures = True, []
    # with serial.Serial(serialPort, baudrate=9600, timeout=1) as fh:
    #     fh.open()
    # 
    #     fh.write(b'device %i' % psuAddress)
    #     response = _read_until_done(fh)
    #     fh.write(b'temperatures')
    #     response += _read_until_done(fh)
    # 
    #     if response.find('ERROR') == -1:
    #         p = re.compile(r'Temperatures: (?P<value0>\d+\.\d+)( +(?P<value1>\d+\.\d+))?')
    #         m = p.search(response)
    #         if m is not None:
    #             temperatures.append(float(m.group('value0')))
    #             try:
    #                 temperatures.append(float(m.group('value1')))
    #             except IndexError:
    #                 pass
    #     else:
    #         status = False
    #         aspI2CLogger.warning("psuTemperatureRead: Error during polling - %s", response)
            
    return status, temperatures
