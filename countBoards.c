/*****************************************************
countBoards - Utility for identifying the number of 
ARX boards connected to the SPI bus.  The exit code 
contains the number of boards found.
 
Usage:
  countBoards

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
	int success, num;
	unsigned short temp;
	char sn[20], simpleNoOp[2], simpleMarker[2];
	char fullData[2*33*8+2], fullResponse[2*33*8+2];

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
	while( temp != marker && num < (STANDS_PER_BOARD*(MAX_BOARDS+1)) ) {
		num += STANDS_PER_BOARD;
		
		// Fill the data array with the commands to send
		j = 0;
		fullData[j++] = simpleMarker[0];
		fullData[j++] = simpleMarker[1];
		for(i=num; i>0; i--) {
			fullData[j++] = simpleNoOp[0];
			fullData[j++] = simpleNoOp[1];
		}
		
		// Read & write (2*num+2) bytes at a time making sure to return chip select to high 
		// when we are done.
		success = sub_spi_transfer(fh, fullData, fullResponse, 2*num+2, SS_CONF(0, SS_LO));
		if( success ) {
			fprintf(stderr, "sendARXDeviceBatch - SPI write %i of %i - %s\n", 0, num, sub_strerror(sub_errno));
		}
		
		// Check the command verification marker
		simpleResponse[0] = fullResponse[2*num];
		simpleResponse[1] = fullResponse[2*num+1];
		temp = array_to_ushort(simpleResponse);
	}
	
	// Convert stands to boards (making sure that we are in range for the board count)
	if( num > (STANDS_PER_BOARD*MAX_BOARDS) ){
		num = 0;
	}
	num /= STANDS_PER_BOARD;
	
	// Report
	printf("Found %i ARX boards (%i stands)\n", num, num*STANDS_PER_BOARD);
	

	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);

	return num;
}
