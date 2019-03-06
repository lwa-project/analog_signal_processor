/*****************************************************
readThermometers - Program to pull information about 
temperature sensors found on the I2C bus.  The data 
polled includes temperatures from:
  * case
  * primary side input
  * modules
 
Usage:
  readThermometers <SUB-20 S/N>

Options:
  None

$Rev$
$LastChangedBy$
$LastChangedDate$
*****************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "libsub.h"
#include "aspCommon.h"

sub_handle* fh = NULL;


int main(int argc, char* argv[]) {
	int i, success, nPSU, found;
	unsigned short temp, modules, page;
	char psuAddresses[128], j, requestedSN[20], sn[20], simpleData[2];
	
	// Make sure we have the right number of arguments to continue
	if( argc != 1+1 ) {
		fprintf(stderr, "readThermometers - Need %i arguments, %i provided\n", 1, argc-1);
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
	int openTries = 0;
	while( (dev = sub_find_devices(dev)) ) {
		// Open the USB device (or die trying)
		fh = sub_open(dev);
		while( (fh == NULL) && (openTries < SUB20_OPEN_MAX_ATTEMPTS) ) {
			openTries++;
			usleep(SUB20_OPEN_WAIT_US);
			
			fh = sub_open(dev);
		}
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
		} else {
			sub_close(fh);
		}
	}
	
	// Make sure we actually have a SUB-20 device
	if( !found ) {
		fprintf(stderr, "readThermometers - Cannot find or open SUB-20 %s\n", requestedSN);
		exit(1);
	}
	
	/********************
	* Read from the I2C *
	********************/
	success = sub_i2c_scan(fh, &nPSU, psuAddresses);
	if( success ) {
		fprintf(stderr, "readThermometers - get PSUs - %s\n", sub_strerror(sub_errno));
		exit(1);
	}
	
	simpleData[0] = 0;
	simpleData[1] = 0;
	
	page = 0;
	modules = 0;
	for(i=0; i<nPSU; i++) {
		if( psuAddresses[i] > 0x1F ) {
			continue;
		}

		#ifdef __INCLUDE_MODULE_TEMPS__
			// Get a list of smart modules for polling
			success = sub_i2c_read(fh, psuAddresses[i], 0xD3, 1, simpleData, 2);
			if( success ) {
				fprintf(stderr, "readThermometers - module status - %s\n", sub_strerror(sub_errno));
				continue;
			}
			modules = array_to_ushort(simpleData);
			
			// Enable writing to the PAGE address (0x00) so we can change modules
			simpleData[0] = (unsigned char) ((1 << 6) & 1);
			success = sub_i2c_write(fh, psuAddresses[i], 0x10, 1, simpleData, 1);
			if( success ) {
				fprintf(stderr, "readThermometers - write settings - %s\n", sub_strerror(sub_errno));
				continue;
			}
			
			// Loop over modules 0 through 15
			simpleData[0] = 0;
			for(j=0; j<16; j++) {
				// Skip "dumb" modules
				if( ((modules >> j) & 1) == 0 ) {
					continue;
				}
				
				// Jump to the correct page and give the PSU a second to get ready
				simpleData[0] = j;
				success = sub_i2c_write(fh, psuAddresses[i], 0x00, 1, simpleData, 1);
				if( success ) {
					fprintf(stderr, "readThermometers - page change - %s\n", sub_strerror(sub_errno));
					continue;
				}
				usleep(20000);
				
				// Verify the current page
				success = sub_i2c_read(fh, psuAddresses[i], 0x00, 1, simpleData, 1);
				if( success ) {
					fprintf(stderr, "readThermometers - get page - %s\n", sub_strerror(sub_errno));
					continue;
				}
				simpleData[1] = 0;
				page = array_to_ushort(simpleData);
				
				/******************
				* Get Temperature *
				******************/
				
				success = sub_i2c_read(fh, psuAddresses[i], 0x8F, 1, simpleData, 2);
				if( success ) {
					fprintf(stderr, "readThermometers - get temperature #3 - %s\n", sub_strerror(sub_errno));
					continue;
				}
				temp = array_to_ushort(simpleData);
				printf("0x%02X Module%02i %.2f\n", psuAddresses[i], page, 1.0*temp);
			}
			
			// Set the module number back to 0
			simpleData[0] = (unsigned char) 0;
			success = sub_i2c_write(fh, psuAddresses[i], 0x00, 1, simpleData, 1);
			if( success ) {
				fprintf(stderr, "readThermometers - page change - %s\n", sub_strerror(sub_errno));
				continue;
			}
			
			// Write-protect all entries but WRITE_PROTECT (0x10)
			simpleData[0] = (unsigned char) ((1 << 7) & 1);
			success = sub_i2c_write(fh, psuAddresses[i], 0x10, 1, simpleData, 1);
			if( success ) {
				fprintf(stderr, "readThermometers - write settings - %s\n", sub_strerror(sub_errno));
				continue;
			}
		#else
			j = 0;
		#endif
		
		/**************************
		* Get System Temperatures *
		**************************/
		simpleData[0] = 0;
		simpleData[1] = 0;

		success = sub_i2c_read(fh, psuAddresses[i], 0x8D, 1, (char *) simpleData, 2);
		if( success ) {
			fprintf(stderr, "readThermometers - get temperature #1 - %s\n", sub_strerror(sub_errno));
			continue;
		}
		temp = array_to_ushort(simpleData);
		printf("0x%02X Case %.2f\n", psuAddresses[i], temp/4.0);
		
		success = sub_i2c_read(fh, psuAddresses[i], 0x8E, 1, (char *) simpleData, 2);
		if( success ) {
			fprintf(stderr, "readThermometers - get temperature #2 - %s\n", sub_strerror(sub_errno));
			continue;
		}
		temp = array_to_ushort(simpleData);
		printf("0x%02X PrimarySide %.2f\n", psuAddresses[i], temp/4.0);
	}
	
	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);

	return 0;
}
