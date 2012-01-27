# -*- coding: utf-8 -*-
"""
Module for storing the various SPI bus bits to twiddle.

$Rev$
$LastChangedBy$
$LastChangedDate$
"""

import logging
import subprocess

__version__ = '0.3'
__revision__ = '$Rev$'
__all__ = ['SPI_init_devices', 'SPI_config_devices', 'SPI_Send', 'LCD_Write', 
	  'SPI_cfg_normal', 'SPI_cfg_shutdown', 
	  'SPI_cfg_output_P16_17_18_19', 'SPI_cfg_output_P20_21_22_23', 'SPI_cfg_output_P24_25_26_27', 'SPI_cfg_output_P28_29_30_31',
	  'SPI_P16_on', 'SPI_P16_off', 'SPI_P17_on', 'SPI_P17_off', 'SPI_P18_on', 'SPI_P18_off', 'SPI_P19_on', 'SPI_P19_off', 
	  'SPI_P20_on', 'SPI_P20_off', 'SPI_P21_on', 'SPI_P21_off', 'SPI_P22_on', 'SPI_P22_off', 'SPI_P23_on', 'SPI_P23_off', 
	  'SPI_P24_on', 'SPI_P24_off', 'SPI_P25_on', 'SPI_P25_off', 'SPI_P26_on', 'SPI_P26_off', 'SPI_P27_on', 'SPI_P27_off', 
	  'SPI_P28_on', 'SPI_P28_off', 'SPI_P29_on', 'SPI_P29_off', 'SPI_P30_on', 'SPI_P30_off', 'SPI_P31_on', 'SPI_P31_off', 
	  'SPI_NoOp', '__version__', '__revision__', '__all__']


aspSPILogger = logging.getLogger('__main__')


# SPI constants
SPI_cfg_normal = 0x0104
SPI_cfg_shutdown = 0x0004
SPI_cfg_output_P16_17_18_19 = 0x550C
SPI_cfg_output_P20_21_22_23 = 0x550D
SPI_cfg_output_P24_25_26_27 = 0x550E
SPI_cfg_output_P28_29_30_31 = 0x550F

SPI_P16_on = 0x0130
SPI_P16_off = 0x0030
SPI_P17_on = 0x0131
SPI_P17_off = 0x0031
SPI_P18_on = 0x0132
SPI_P18_off = 0x0032
SPI_P19_on = 0x0133
SPI_P19_off = 0x0033
SPI_P20_on = 0x0134
SPI_P20_off = 0x0034
SPI_P21_on = 0x0135
SPI_P21_off = 0x0035
SPI_P22_on = 0x0136
SPI_P22_off = 0x0036
SPI_P23_on = 0x0137
SPI_P23_off = 0x0037
SPI_P24_on = 0x0138
SPI_P24_off = 0x0038
SPI_P25_on = 0x0139
SPI_P25_off = 0x0039
SPI_P26_on = 0x013A
SPI_P26_off = 0x003A
SPI_P27_on = 0x013B
SPI_P27_off = 0x003B

SPI_P28_on = 0x013C
SPI_P28_off = 0x003C
SPI_P29_on = 0x013D
SPI_P29_off = 0x003D
SPI_P30_on = 0x013E
SPI_P30_off = 0x003E
SPI_P31_on = 0x013F
SPI_P31_off = 0x003F

SPI_NoOp = 0x0000


def SPI_init_devices(num, Config):
	"""
	Initialize a given number of SPI bus devices (ARX boards) with the specified state.

	Return the status of the operation as a boolean.
	"""

	p = subprocess.Popen(['initARXDevices', '%i' % num, '0x%04x' % Config], shell=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	p.wait()
          
	output, output2 = p.communicate()
	
	if p.returncode != 0:
		aspSPILogger.warning("SPI_init_devices: command returned %i; '%s;%s'", p.returncode, output, output2)
		return False
	
	return True


def SPI_config_devices(num, Config):
	"""
	Configure a given number fo SPI bus devices (ARX boards) with the specified state.

	Return the status of the operation as a boolean.
	"""

	p = subprocess.Popen(['sendARXDevice', '%i' % num, '0', '0x%04x' % Config], shell=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	p.wait()
          
	output, output2 = p.communicate()
	
	if p.returncode != 0:
		aspSPILogger.warning("SPI_config_devices: command returned %i; '%s;%s'", p.returncode, output, output2)
		return False
	
	return True


def SPI_Send(num, device, data):
	"""
	Send a command via SPI bus to the specified device.

	Return the status of the operation as a boolean.
	"""

	p = subprocess.Popen(['sendARXDevice', '%i' % num, '%i' % device, '0x%04x' % data], shell=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	p.wait()
          
	output, output2 = p.communicate()
	
	if p.returncode != 0:
		aspSPILogger.warning("SPI_Send: command returned %i; '%s;%s'", p.returncode, output, output2)
		return False
	
	return True


def LCD_Write(message):
	"""
	Write the specified string to the LCD screen.
	
	Return the status of the operation as a boolean.
	"""
	
	p = subprocess.Popen(['writeARXLCD', '%s' % message], shell=False, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
	p.wait()
          
	output, output2 = p.communicate()
	
	if p.returncode != 0:
		aspSPILogger.warning("LCD_Write: command returned %i; '%s;%s'", p.returncode, output, output2)
		return False
	
	return True