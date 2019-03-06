/*****************************************************
onoffPSU - Change the overall power state for the 
specified device.

Usage:
  onoffPSU <SUB-20 S/N> <device address> <new power state>
  
  * Device addresses are two-digit hexadecimal numbers 
    (i.e. 0x1F)
  * Valid power states are 00 (off) and 11 (on)

Options:
  None

$Rev$
$LastChangedBy$
$LastChangedDate$
*****************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "libsub.h"
#include "aspCommon.h"

sub_handle* fh = NULL;


int main(int argc, char* argv[]) {
	char *endptr;
	int i, device, newState, success, nPSU, done, found;
	unsigned short status;
	char psuAddresses[128], requestedSN[20], sn[20], simpleData[2];
	
	// Make sure we have the right number of arguments to continue
	if( argc != 3+1 ) {
		fprintf(stderr, "onoffPSU - Need %i arguments, %i provided\n", 3, argc-1);
		exit(1);
	}
	
	// Copy the string
	strncpy(requestedSN, argv[1], 17);
	// Convert the strings into integer values
	device   = strtod(argv[2], &endptr);
	newState = strtod(argv[3], &endptr);
	if( newState != 0 && newState != 11 ) {
		fprintf(stderr, "onoffPSU - Unknown state %i (valid values are %02i and %02i)\n", newState, 0, 11);
		exit(1);
	}
	
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
		fprintf(stderr, "onoffPSU - Cannot find or open SUB-20 %s\n", requestedSN);
		exit(1);
	}
	
	/********************
	* Read from the I2C *
	********************/
	success = sub_i2c_scan(fh, &nPSU, psuAddresses);
	if( success ) {
		fprintf(stderr, "onoffPSU - get PSUs - %s\n", sub_strerror(sub_errno));
		exit(1);
	}

	simpleData[0] = 0;
	simpleData[1] = 0;
	done = 0;
	for(i=0; i<nPSU; i++) {
		// See if we have the right device
		if( psuAddresses[i] != device ) {
			continue;
		}
		
		// Get the current power supply state
		success = sub_i2c_read(fh, psuAddresses[i], 0x01, 1, simpleData, 1);
		if( success ) {
			fprintf(stderr, "onoffPSU - page change - %s\n", sub_strerror(sub_errno));
			continue;
		}
		simpleData[1] = 0;
		status = (array_to_ushort(simpleData) >> 7) & 1;
		printf("0x%02X is in state %u\n", psuAddresses[i], status);
		
		// Enable writing to the OPERATION address (0x01) so we can change modules
		simpleData[0] = (unsigned char) 0;
		success = sub_i2c_write(fh, psuAddresses[i], 0x10, 1, simpleData, 1);
		if( success ) {
			fprintf(stderr, "onoffPSU - write settings - %s\n", sub_strerror(sub_errno));
			continue;
		}

		// Find out the new state to put the power supply in
		if( newState == 0 ) {
			// Turn off the power supply
			simpleData[0] = (unsigned char) 0;
		} else {
			// Turn on the power supply
			simpleData[0] = (unsigned char) (1 << 7);
		}
		simpleData[1] = 0;

		// Toggle the power status and wait a bit for the changes to take affect
		success = sub_i2c_write(fh, psuAddresses[i], 0x01, 1, simpleData, 1);
		if( success ) {
			fprintf(stderr, "onoffPSU - on/off toggle - %s\n", sub_strerror(sub_errno));
			continue;
		}
		usleep(20000);
		
		// Check the power supply status
		simpleData[0] = (unsigned char) 0;
		success = sub_i2c_read(fh, psuAddresses[i], 0x01, 1, simpleData, 1);
		if( success ) {
			fprintf(stderr, "onoffPSU - page change - %s\n", sub_strerror(sub_errno));
			continue;
		}
		simpleData[1] = 0;
		status = (array_to_ushort(simpleData) >> 7) & 1;
		printf("0x%02X is now in state %u\n", psuAddresses[i], status);
		
		// Write-protect all entries but WRITE_PROTECT (0x10)
		simpleData[0] = (unsigned char) ((1 << 7) & 1);
		success = sub_i2c_write(fh, psuAddresses[i], 0x10, 1, simpleData, 1);
		if( success ) {
			fprintf(stderr, "onoffPSU - write settings - %s\n", sub_strerror(sub_errno));
			continue;
		}
		
		// Mark that we have sone something
		done = 1;
	}
	
	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);
	
	if( !done ) {
		fprintf(stderr, "onoffPSU - Cannot find device at address 0x%02X\n", device);
		exit(1);
	}
	return 0;
}
