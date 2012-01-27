#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libsub.h"
#include "spiCommon.h"

sub_handle* fh = NULL;


int main(int argc, char* argv[]) {
	/*************************
	* Command line parsing   *
	*************************/
	char *endptr;
	int success, num, device, temp;
	char sn[20], simpleData[2], simpleNoOp[2];

	// Make sure we have the right number of arguments to continue
	if( argc != 3+1 ) {
		fprintf(stderr, "sendARXDevice - Need %i arguments, %i provided\n", 3, argc-1);
		exit(1);
	}
	
	// Convert the strings into integer values
	num    = strtod(argv[1], &endptr);
	device = strtod(argv[2], &endptr);
	hex_to_array(argv[3], simpleData);
	hex_to_array("0x0000", simpleNoOp);

	// Make sure we have a device number that makes sense
	if( device < 0 || device > num ) {
		fprintf(stderr, "sendARXDevice - Device #%i is out-of-range\n", device);
		exit(1);
	}

	// Report on where we are at
	temp = array_to_int(simpleData);
	if( device != 0 ) {
		fprintf(stderr, "Sending data 0x%04x (%u) to device %i of %i\n", temp, temp, device, num);
	} else {
		fprintf(stderr, "Sending data 0x%04x (%u) to all %i devices\n", temp, temp, num);
	}


	/************************************
	* SUB-20 device selection and ready *
	************************************/
	struct usb_device* dev;

	// Open the USB device (or die trying)
	dev = NULL;
	fh = sub_open(dev);
	if( !fh ) {
		fprintf(stderr, "sendARXDevice - open - %s\n", sub_strerror(sub_errno));
		exit(1);
	}
	
	success = sub_get_serial_number(fh, sn, sizeof(sn));
	if( !success ) {
		fprintf(stderr, "sendARXDevice - get sn - %s\n", sub_strerror(sub_errno));
		exit(1);
	}
	fprintf(stderr, "Found SUB-20 device S/N: %s\n", sn);


	/****************************************
	* Send the command and get the response *
	****************************************/
	int i, j;
	char simpleResponse[2];
	
	// Enable the SPI bus operations on the SUB-20 board
	j = 0;
	success = sub_spi_config(fh, 0, &j);
	if( success ) {
		fprintf(stderr, "sendARXDevice - get config - %s\n", sub_strerror(sub_errno));
	}
	
	success = 1;
	while( success ) {
		success = sub_spi_config(fh, SPI_ENABLE|SPI_CPOL_FALL|SPI_SETUP_SMPL|SPI_MSB_FIRST|SPI_CLK_250KHZ, NULL);
		if( success ) {
			fprintf(stderr, "sendARXDevice - set config - %s\n", sub_strerror(sub_errno));
			// exit(1);
		}
	}

	// Read & write 2 bytes at a time making sure to return chip select to high 
	// when we are done.
	j = 1;
	for(i=num; i>0; i--) {
		if( i == device || device == 0 ) {
			if( j == num ) {
				// Final set of 2 bytes - chip select to high after transmitting
				success = sub_spi_transfer(fh, simpleData, simpleResponse, 2, SS_CONF(0, SS_LO));
			} else {
				success = sub_spi_transfer(fh, simpleData, simpleResponse, 2, SS_CONF(0, SS_L));
			}
			
		} else {
			if( j == num ) {
				// Final set of 2 bytes - chip select to high after transmitting
				success = sub_spi_transfer(fh, simpleNoOp, simpleResponse, 2, SS_CONF(0, SS_LO));
			} else {
				success = sub_spi_transfer(fh, simpleNoOp, simpleResponse, 2, SS_CONF(0, SS_L));
			}
		}
		
		if( success ) {
			fprintf(stderr, "sendARXDevice - SPI write %i of %i - %s\n", i+1, num, sub_strerror(sub_errno));
			i += 1;
		}
		
		j += 1;
	}


	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);

	return 0;
}
