import os
import sys
import glob
import ctypes
import serial
import struct
import subprocess
from enum import IntEnum

from typing import List


class Command(IntEnum):
    """
    Valid ATmega commands.
    """
    
    COMMAND_SUCCESS = 0x00
    COMMAND_READ_SN = 0x01
    COMMAND_READ_VER = 0x02
    COMMAND_READ_MLEN = 0x03
    COMMAND_ECHO = 0x04
    COMMAND_TRANSFER_SPI = 0x11
    COMMAND_READ_ADCS = 0x21
    COMMAND_SCAN_I2C = 0x31
    COMMAND_READ_I2C = 0x32
    COMMAND_WRITE_I2C = 0x33
    COMMAND_LOCK = 0xA1
    COMMAND_UNLOCK = 0xA2
    COMMAND_WRITE_SN = 0xA3
    COMMAND_FAILURE = 0xF0
    COMMAND_FAILURE_ARG = 0xFA
    COMMAND_FAILURE_STA = 0xFB
    COMMAND_FAILURE_BUS = 0xFC
    COMMAND_FAILURE_CMD = 0xFF


class Buffer(ctypes.LittleEndianStructure):
    """
    ATmega command/response data structure.
    """
    
    _pack_ = 1  # Pack the structure tightly
    _fields_ = [("command", ctypes.c_uint8),
                ("size",    ctypes.c_uint16),
                ("buffer",  ctypes.c_char * 530)]


def find_devices() -> List[str]:
    """
    Return a list of Atmega devices under /dev/ttyUSB* and /dev/ttyACM*.
    """
    
    possible_devices = glob.glob('/dev/ttyUSB*')
    possible_devices.extend(glob.glob('/dev/ttyACM*'))
    
    devices = []
    for dev in possible_devices:
        udev_info = subprocess.check_output(['udevadm', 'info', f"--name={dev}"])
        udev_info = udev_info.decode()
        
        match = 0
        for line in udev_info.split('\n'):
            if line.find('ID_VENDOR_ID=0403') != -1 or line.find('ID_VENDOR_ID=2341') != -1:
                match |= 1
            elif line.find('ID_MODEL_ID=6001') != -1 or line.find('ID_MODEL_ID=0001') != -1:
                match |= 2
                
        if match == 3:
            devices.append(dev)
            
    return devices


def open(device: str) -> serial.Serial:
    """
    Open an ATmega device and get it ready for command processing.
    """
    
    return serial.Serial(device, baudrate=115200, bytesize=8, parity='N', stopbits=1, timeout=0.1)


def send_command(handle: serial.Serial, command: Buffer, max_retry: int=0, retry_wait_ms: int=50) -> Buffer:
    """
    Send a command buffer to an ATmega device and return the response.
    """
    
    # Send the command with the <<< and >>> command markers
    nsend = handle.write(bytes('<<<'))
    nsend += handle.write(bytes(command)[:3+command.size])
    nsend += handle.write(bytes('>>>'))
    
    # Read in the response
    handle.read(3)
    resp = handle.read_all()
    
    return Buffer.from_buffer_copy(resp[:-3])


def close(handle: serial.Serial) -> None:
    """
    Close an open ATmega device.
    """
    
    handle.close()
