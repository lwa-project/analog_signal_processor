/*****************************************************
countBoards - Utility for identifying the number of 
ARX boards connected to the SPI bus.  The exit code 
contains the number of boards found.
 
Usage:
  countBoards <SUB-20 S/N>

Options:
  None
*****************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "libsub.h"
#include "aspCommon.h"

sub_handle* fh = NULL;


int main(int argc, char* argv[]) {
	int success, num, found;
	unsigned short temp;
	char requestedSN[20], sn[20], lcd_str[20], simpleNoOp[2], simpleMarker[2];
	char fullData[2*MAX_BOARDS*STANDS_PER_BOARD+2], fullResponse[2*MAX_BOARDS*STANDS_PER_BOARD+2];
	
	// Make sure we have the right number of arguments to continue
	if( argc != 1+1 ) {
		fprintf(stderr, "countBoards - Need %i arguments, %i provided\n", 1, argc-1);
		exit(0);
	}
	
	// Copy the string
	strncpy(requestedSN, argv[1], 17);
	
	// Convert the strings into integer values
	hex_to_array("0x0000", simpleNoOp);
	ushort_to_array(marker, simpleMarker);
	
	
	/************************************
	* SUB-20 device selection and ready *
	************************************/
	sub_device* dev = NULL;
	
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
		fprintf(stderr, "countBoards - Cannot find or open SUB-20 %s\n", requestedSN);
		exit(0);
	}
		
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
		success = sub_spi_config(fh, ARX_SPI_CONFIG, NULL);
		if( success ) {
			fprintf(stderr, "countBoards - set config - %s\n", sub_strerror(sub_errno));
			exit(0);
		}
	}

	num = 0;
	temp = array_to_ushort(simpleResponse);
	while( temp != marker && num < (STANDS_PER_BOARD*(17+1)) ) {
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
	if( num > (STANDS_PER_BOARD*17) ){
		num = 0;
	}
	num /= STANDS_PER_BOARD;
	
	sprintf(lcd_str, "\fSN: %4s\nBds: %3i\n", sn, num);
	success = sub_lcd_write(fh, lcd_str);
	
	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);
	
	// Report
	printf("Found %i ARX boards (%i stands)\n", num, num*STANDS_PER_BOARD);

	return num;
}
