/*****************************************************
countPSUs - Utility for identifying the power supply
modules connected to the I2C bus. The exit code 
contains the number of modules found.

Note:  There can be more than one module per power
       supply chassis.
 
Usage:
  countPSUs

Options:
  None

$Rev$
$LastChangedBy$
$LastChangedDate$
*****************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libsub.h"
#include "aspCommon.h"

sub_handle* fh = NULL;


int main(int argc, char* argv[]) {
	int i, j, success, num, total, nPSU, modules;
	char sn[20], i2cSN[20], psuAddresses[128];
	unsigned char simpleData[2];

	strcpy(i2cSN, "UNK");
	
	/************************************
	* SUB-20 device selection and ready *
	************************************/
	struct usb_device* dev;

	// Loop through all SUB-20s
	total = 0;
	while( dev = sub_find_devices(dev) ) {
		// Open the USB device (or die trying)
		fh = sub_open(dev);
		if( !fh ) {
			continue;
		}
		
		success = sub_get_serial_number(fh, sn, sizeof(sn));
		if( !success ) {
			continue;
		}
		
		fprintf(stderr, "Found SUB-20 device S/N: %s\n", sn);
	
		/********************
		* Read from the I2C *
		********************/
		success = sub_i2c_scan(fh, &nPSU, psuAddresses);
		if( success ) {
			fprintf(stderr, "countPSUs - get PSUs - %s\n", sub_strerror(sub_errno));
			exit(1);
		}
		printf("-> found %i I2C devices:\n", nPSU);
		for(i=0; i<nPSU; i++) {
			printf(" -> 0x%02X\n", psuAddresses[i]);
		}

		simpleData[0] = 0;
		simpleData[1] = 0;

		num = 0;
		for(i=0; i<nPSU; i++) {
			if( psuAddresses[i] > 0x1F ) {
				continue;
			}
		
			// Get a list of smart modules for polling
			success = sub_i2c_read(fh, psuAddresses[i], 0xD3, 1, (char *) simpleData, 2);
			if( success ) {
				fprintf(stderr, "countPSUs - module status - %s\n", sub_strerror(sub_errno));
				continue;
			}
			simpleData[0] &= 0x00FF;
			simpleData[1] &= 0x00FF;	

			modules = ((int) simpleData[1] << 8) | ((int) simpleData[0]);
			for(j=0; j<16; j++) {
				num += ((modules >> j) & 1);
			}
		}
	
		printf("-> %i PSU modules\n", num);
		if( num > 0 ) {
			strcpy(i2cSN, sn);
		}

		total += num;
	
		/*******************
		* Cleanup and exit *
		*******************/
		sub_close(fh);
	}

	printf("I2C devices appear to be on %s\n", i2cSN);
	return total;
}
