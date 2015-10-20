#ifndef __ASPCOMMON_H
#define __ASPCOMMON_H

/*
  aspCommon.h - Header library to carry out a few simple hexadecimal to array 
  operations needed for using the SUB-20 device with the SPI and I2C buses.
  
  Functions defined:
    * hex_to_array - convert a hexadecimal string to an 2-byte array
    * ushort_to_array - convert an unsigned short to a 2-byte array
    * array_to_ushort - convert a 2-byte array to an unsigned short
    
$Rev$
$LastChangedBy$
$LastChangedDate$
*/

#include <stdlib.h>

// SUB-20 device opening control
#define SUB20_OPEN_MAX_ATTEMPTS 20
#define SUB20_OPEN_WAIT_US  5000


// SPI bus configuration settings
#define ARX_SPI_CONFIG SPI_ENABLE|SPI_CPOL_FALL|SPI_SETUP_SMPL|SPI_MSB_FIRST|SPI_CLK_500KHZ
#define TRANS_SPI_INTERMEDIATE SS_CONF(0, SS_L)
#define TRANS_SPI_FINAL SS_CONF(0, SS_LO)


// ARX board configuration
#define STANDS_PER_BOARD 8
#define MAX_BOARDS 33


// Command verification marker
unsigned short marker = 0x0120;


// Pack and Unpack functions
void hex_to_array(char*, char*);
void ushort_to_array(unsigned short, char*);
int array_to_ushort(char*);


// Uncomment the next line to include polling of module temperatures
//#define __INCLUDE_MODULE_TEMPS__


// Uncommend the next line to decode the module type (causes readPSU to run slower)
//#define __DECODE_MODULE_TYPE__

// Uncomment the next line to use input rather than the module outuput current
//#define __USE_INPUT_CURRENT__


/*
  hex_to_array - Convert a hex. string to a 2-byte array
*/
void hex_to_array(char* in, char* out) {
	int value;
	char* endptr;

	value = strtod(in, &endptr);

	out[1] = (unsigned int) ((value >>  8) & 0xFF);
	out[0] = (unsigned int) (value & 0xFF);    
}


/*
  ushort_to_array - Convert an unsigned short to a 2-byte array
*/
void ushort_to_array(unsigned short value, char* out) {
	out[1] = (unsigned int) ((value >>  8) & 0xFF);
	out[0] = (unsigned int) (value & 0xFF);    
}


/*
  array_to_ushort - Convert a 2-byte array to an integer
*/
int array_to_ushort(char* in) {
	unsigned short value;

	value  =  (unsigned short) ((in[1] & 0xFF)<< 8);
	value |=  (unsigned short) (in[0] & 0xFF);

	return value;
}

#endif
