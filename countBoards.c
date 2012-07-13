#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libsub.h"
#include "spiCommon.h"

sub_handle* fh = NULL;


int main(int argc, char* argv[]) {
	int success, num;
	unsigned short temp;
	char sn[20], simpleNoOp[2], simpleMarker[2];

	// Convert the strings into integer values
	hex_to_array("0x0000", simpleNoOp);
	ushort_to_array(marker, simpleMarker);


	/************************************
	* SUB-20 device selection and ready *
	************************************/
	struct usb_device* dev;

	// Open the USB device (or die trying)
	dev = NULL;
	fh = sub_open(dev);
	if( !fh ) {
		fprintf(stderr, "countBoards - open - %s\n", sub_strerror(sub_errno));
		exit(0);
	}
	
	success = sub_get_serial_number(fh, sn, sizeof(sn));
	if( !success ) {
		fprintf(stderr, "countBoards - get sn - %s\n", sub_strerror(sub_errno));
		exit(0);
	}
	fprintf(stderr, "Found SUB-20 device S/N: %s\n", sn);


	/****************************************
	* Send the command and get the response *
	****************************************/
	int i, j;
	char simpleResponse[2];
	hex_to_array("0x0000", simpleResponse);
	
	// Enable the SPI bus operations on the SUB-20 board
	j = 0;
	success = sub_spi_config(fh, 0, &j);
	if( success ) {
		fprintf(stderr, "countBoards - get config - %s\n", sub_strerror(sub_errno));
	}
	
	success = 1;
	while( success ) {
		success = sub_spi_config(fh, SPI_ENABLE|SPI_CPOL_FALL|SPI_SETUP_SMPL|SPI_MSB_FIRST|SPI_CLK_125KHZ, NULL);
		if( success ) {
			fprintf(stderr, "countBoards - set config - %s\n", sub_strerror(sub_errno));
			exit(0);
		}
	}

	num = 0;
	temp = array_to_ushort(simpleResponse);
	while( temp != marker ) {
		num += STANDS_PER_BOARD;

		// Read & write 2 bytes at a time making sure to return chip select to high 
		// when we are done.
		j = 0;
		success = sub_spi_transfer(fh, simpleMarker, simpleResponse, 2, SS_CONF(0, SS_L));
		temp = array_to_ushort(simpleResponse);
		j += 1;

		for(i=num; i>0; i--) {
			if( j == num ) {
				// Final set of 2 bytes - chip select to high after transmitting
				success = sub_spi_transfer(fh, simpleNoOp, simpleResponse, 2, SS_CONF(0, SS_LO));
			} else {
				success = sub_spi_transfer(fh, simpleNoOp, simpleResponse, 2, SS_CONF(0, SS_L));
			}
		
			if( success ) {
				fprintf(stderr, "countBoards - SPI write %i of %i - %s\n", i+1, num, sub_strerror(sub_errno));
				i += 1;
			}

			temp = array_to_ushort(simpleResponse);
			j += 1;
		}
	}
	
	// Convert stands to boards
	num /= STANDS_PER_BOARD;
	
	// Report
	printf("Found %i ARX boards (%i stands)\n", num, num*STANDS_PER_BOARD);
	

	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);

	return num;
}
