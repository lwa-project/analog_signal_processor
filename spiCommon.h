#ifndef __SPICOMMON_H
#define __SPICOMMON_H

/*
  spiCommon.h - Header library to carry out a few simple hexadecimal to array 
  operations
  
  Functions defined:
    * hex_to_array - convert a hexadecimal string to an 2-byte array
    * ushort_to_array - convert an unsigned short to a 2-byte array
    * array_to_ushort - convert a 2-byte array to an unsigned short
    
$Rev$
$LastChangedBy$
$LastChangedDate$
*/

#include <stdlib.h>

// SPI bus configuration settings
#define ARX_SPI_CONFIG SPI_ENABLE|SPI_CPOL_FALL|SPI_SETUP_SMPL|SPI_MSB_FIRST|SPI_CLK_250KHZ
#define TRANS_SPI_INTERMEDIATE SS_CONF(0, SS_L)
#define TRANS_SPI_FINAL SS_CONF(0, SS_LO)


// ARX board configuration
#define STANDS_PER_BOARD 8


// Command verification marker
unsigned short marker = 0x0120;


// Pack and Unpack functions
void hex_to_array(char*, char*);
void ushort_to_array(unsigned short, char*);
int array_to_ushort(char*);


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