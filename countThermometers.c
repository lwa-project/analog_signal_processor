/*****************************************************
countThermometers - Utility for identifying the 
temperature sensors associated with each power supply.
The exit code contains the number of sensors found.

Note:  There can be more than one temperature sensor
       per power supply.
 
Usage:
  countThermometers

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
	int i, success, num, nPSU, found;
	char requestedSN[20], sn[20], psuAddresses[128];
    
#ifdef __INCLUDE_MODULE_TEMPS__
    int j = 0, modules = 0;
	unsigned char simpleData[2] = { 0 };
#endif

	// Make sure we have the right number of arguments to continue
	if( argc != 1+1 ) {
		fprintf(stderr, "countThermometers - Need %i arguments, %i provided\n", 1, argc-1);
		exit(1);
	}

	// Copy the string
	strncpy(requestedSN, argv[1], 17);
	
	/************************************
	* SUB-20 device selection and ready *
	************************************/
	struct usb_device* dev = NULL;
	
	// Find the right SUB-20
	found = 0;
	while( (dev = sub_find_devices(dev)) ) {
		// Open the USB device (or die trying)
		fh = sub_open(dev);
		if( !fh ) {
			continue;
		}
		
		success = sub_get_serial_number(fh, sn, sizeof(sn));
		if( !success ) {
			continue;
		}
		
		if( !strcmp(sn, requestedSN) ) {
			fprintf(stderr, "Found SUB-20 device S/N: %s\n", sn);
			found = 1;
			break;
		}
	}
	
	// Make sure we actually have a SUB-20 device
	if( !found ) {
		fprintf(stderr, "countThermometers - Cannot find or open SUB-20 %s\n", requestedSN);
		exit(1);
	}
	
	/********************
	* Read from the I2C *
	********************/
	success = sub_i2c_scan(fh, &nPSU, psuAddresses);
	if( success ) {
		fprintf(stderr, "countThermometers - get PSUs - %s\n", sub_strerror(sub_errno));
		exit(1);
	}

	num = 0;
	for(i=0; i<nPSU; i++) {
		if( psuAddresses[i] > 0x1F ) {
			continue;
		}

#ifdef __INCLUDE_MODULE_TEMPS__
			// Get a list of smart modules for polling
			success = sub_i2c_read(fh, psuAddresses[i], 0xD3, 1, (char *) simpleData, 2);
			if( success ) {
				fprintf(stderr, "countThermometers - module status - %s\n", sub_strerror(sub_errno));
				continue;
			}
			simpleData[0] &= 0x00FF;
			simpleData[1] &= 0x00FF;

			// Each module has a temperature sensor
			modules = ((int) simpleData[1] << 8) | ((int) simpleData[0]);
			for(j=0; j<16; j++) {
				num += ((modules >> j) & 1);
			}
#endif

		// And there are two overall sensors per PSU
		num += 2;
	}
	
	printf("Found %i PSU thermometers\n", num);
	
	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);

	return num;
}
