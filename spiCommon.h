#ifndef __SPICOMMON_H
#define __SPICOMMON_H

/*
  spiCommon.h - Header library to carry out a few simple hexidecimal to array 
  operations
  
  Functions defined:
    * hex_to_array - convert a hexadecimal string to an 2-byte array
    * array_to_int - convert a 2-byte array to an integer
    
$Rev$
$LastChangedBy$
$LastChangedDate$
*/

#include <stdlib.h>


void hex_to_array(char*, char*);
int array_to_int(char*);


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
  array_to_int - Convert a 2-byte arry to an integer
*/
int array_to_int(char* in) {
	int value;

	value  =  (int) (in[1] << 8);
	value |=  (int) in[0];
	value &= 0xFFFF;

	return value;
}

#endif