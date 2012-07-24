/*****************************************************
onoffPSU - Change the overall power state for the 
specified device.

Usage:
  onoffPSU <device address> <new power state>
  
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

#include "libsub.h"
#include "spiCommon.h"

sub_handle* fh = NULL;


int main(int argc, char* argv[]) {
	char *endptr;
	int i, device, newState, success, nPSU, done;
	unsigned short status;
	char psuAddresses[128], sn[20], simpleData[2];
	
	// Make sure we have the right number of arguments to continue
	if( argc != 2+1 ) {
		fprintf(stderr, "onoffPSU - Need %i arguments, %i provided\n", 2, argc-1);
		exit(1);
	}
	
	// Convert the strings into integer values
	device   = strtod(argv[1], &endptr);
	newState = strtod(argv[2], &endptr);
	if( newState != 0 && newState != 11 ) {
		fprintf(stderr, "onoffPSU - Unknown state %i (valid values are %02i and %02i)\n", newState, 0, 11);
		exit(1);
	}
	
	/************************************
	* SUB-20 device selection and ready *
	************************************/
	struct usb_device* dev;
	
	// Open the USB device (or die trying)
	dev = NULL;
	fh = sub_open(dev);
	while( !fh ) {
		fprintf(stderr, "onoffPSU - open - %s\n", sub_strerror(sub_errno));
		usleep(50000);
		fh = sub_open(dev);
	}
	
	success = sub_get_serial_number(fh, (char *) sn, sizeof(sn));
	if( !success ) {
		fprintf(stderr, "onoffPSU - get sn - %s\n", sub_strerror(sub_errno));
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
		
		// Enable writing to the OPERATION address (0x00) so we can change modules
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
