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
	int success, num, temp;
	char sn[20], simpleData[2];

	// Make sure we have the right number of arguments to continue
	if( argc != 2+1 ) {
		fprintf(stderr, "initARXDevices - Need %i arguments, %i provided\n", 2, argc-1);
		exit(1);
	}
	
	// Convert the strings into integer values
	num    = strtod(argv[1], &endptr);
	hex_to_array(argv[2], simpleData);

	// Report on where we are at
	temp = array_to_int(simpleData);
	fprintf(stderr, "Sending data 0x%04x (%u) to all %i devices\n", temp, temp, num);


	/************************************
	* SUB-20 device selection and ready *
	************************************/
	struct usb_device* dev;

	// Open the USB device (or die trying)
	dev = NULL;
	fh = sub_open(dev);
	if( !fh ) {
		fprintf(stderr, "initARXDevices - open - %s\n", sub_strerror(sub_errno));
		exit(1);
	}
	
	success = sub_get_serial_number(fh, sn, sizeof(sn));
	if( !success ) {
		fprintf(stderr, "initARXDevices - get sn - %s\n", sub_strerror(sub_errno));
		exit(1);
	}
	fprintf(stderr, "Found SUB-20 device S/N: %s\n", sn);


	/****************************************
	* Send the command and get the response *
	****************************************/
	int i, j;
	char simpleResponse[2];
	
	// Enable SPI bus commands
	j = 0;
	success = sub_spi_config(fh, 0, &j);
	if( success ) {
		fprintf(stderr, "initARXDevices - get config - %s\n", sub_strerror(sub_errno));
	}
	
	success = 1;
	while( success ) {
		success = sub_spi_config(fh, SPI_ENABLE|SPI_CPOL_FALL|SPI_SETUP_SMPL|SPI_MSB_FIRST|SPI_CLK_250KHZ, NULL);
		if( success ) {
			fprintf(stderr, "initARXDevices - set config - %s\n", sub_strerror(sub_errno));
			// exit(1);
		}
	}

	for(i=0; i<2*num; i++) {
		// Read & write
		success = sub_spi_transfer(fh, simpleData, simpleResponse, 2, SS_CONF(0, SS_LO));
		
		if( success ) {
			fprintf(stderr, "initARXDevices - SPI write %i of %i - %s\n", i+1, 2*num, sub_strerror(sub_errno));
			i -= 1;
		}
	}


	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);

	return 0;
}