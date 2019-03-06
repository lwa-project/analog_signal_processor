/*****************************************************
readARXDevice - Read a SPI register from the specified 
device.  An exit code of zero indicates that no errors
were encountered.
 
Usage:
  readARXDevice <SUB-20 S/N> <total stand count> <device> <command>

  * Command is a four digit hexadecimal values (i.e., 
  0x1234)
  
Options:
  None

$Rev$
$LastChangedBy$
$LastChangedDate$
*****************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libsub.h"
#include "aspCommon.h"

sub_handle* fh = NULL;


int main(int argc, char* argv[]) {
	/*************************
	* Command line parsing   *
	*************************/
	char *endptr;
	int success, num, device, found;
	unsigned short temp;
	char requestedSN[20], sn[20], simpleData[2], simpleNoOp[2], simpleMarker[2];

	// Make sure we have the right number of arguments to continue
	if( argc != 4+1 ) {
		fprintf(stderr, "readARXDevice - Need %i arguments, %i provided\n", 4, argc-1);
		exit(1);
	}
	
	// Copy the string
	strncpy(requestedSN, argv[1], 17);
	// Convert the strings into integer values
	num    = strtod(argv[2], &endptr);
	device = strtod(argv[3], &endptr);
	hex_to_array(argv[4], simpleData);
	hex_to_array("0x0000", simpleNoOp);
	ushort_to_array(marker, simpleMarker);
	
	// Make this a "read" operation to setting the high bit on the command 
	// address to 1.  Also, zero out the data value.
	simpleData[0] |= 0x80;
	simpleData[1] &= 0x00;

	// Make sure we have a device number that makes sense
	if( device < 0 || device > num ) {
		fprintf(stderr, "readARXDevice - Device #%i is out-of-range\n", device);
		exit(1);
	}
	
	// Report on where we are at
	temp = array_to_ushort(simpleData);
	if( device != 0 ) {
		fprintf(stderr, "Reading data 0x%04X (%u) to device %i of %i\n", temp, temp, device, num);
	} else {
		fprintf(stderr, "Reading data 0x%04X (%u) to all %i devices\n", temp, temp, num);
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
		fprintf(stderr, "readARXDevice - Cannot find or open SUB-20 %s\n", requestedSN);
		exit(1);
	}
	
	
	/****************************************
	* Send the command and get the response *
	****************************************/
	int i, j;
	char simpleResponse[2];
	
	// Enable the SPI bus operations on the SUB-20 board
	j = 0;
	success = sub_spi_config(fh, 0, &j);
	if( success ) {
		fprintf(stderr, "readARXDevice - get config - %s\n", sub_strerror(sub_errno));
	}
	
	success = 1;
	while( success ) {
		success = sub_spi_config(fh, ARX_SPI_CONFIG, NULL);
		if( success ) {
			fprintf(stderr, "readARXDevice - set config - %s\n", sub_strerror(sub_errno));
			exit(1);
		}
	}
	
	
	/*****************************/
	/* Ready the register value */
	/*****************************/

	// Read & write 2 bytes at a time making sure to return chip select to high 
	// when we are done.
	success = sub_spi_transfer(fh, simpleMarker, simpleResponse, 2, TRANS_SPI_INTERMEDIATE);
	if( success ) {
		fprintf(stderr, "readARXDevice - SPI write %i of %i - %s\n", 0, num, sub_strerror(sub_errno));
	}
	temp = array_to_ushort(simpleResponse);
	
	j = 1;
	for(i=num; i>0; i--) {
		if( i == device || device == 0 ) {
			if( j == num ) {
				// Final set of 2 bytes - chip select to high after transmitting
				success = sub_spi_transfer(fh, simpleData, simpleResponse, 2, TRANS_SPI_FINAL);
			} else {
				success = sub_spi_transfer(fh, simpleData, simpleResponse, 2, TRANS_SPI_INTERMEDIATE);
			}
			
		} else {
			if( j == num ) {
				// Final set of 2 bytes - chip select to high after transmitting
				success = sub_spi_transfer(fh, simpleNoOp, simpleResponse, 2, TRANS_SPI_FINAL);
			} else {
				success = sub_spi_transfer(fh, simpleNoOp, simpleResponse, 2, TRANS_SPI_INTERMEDIATE);
			}
		}
		
		if( success ) {
			fprintf(stderr, "readARXDevice - SPI write %i of %i - %s\n", i+1, num, sub_strerror(sub_errno));
			i += 1;
		}
		
		j += 1;
	}
	
	temp = array_to_ushort(simpleResponse);
	if( temp != marker ) {
		fprintf(stderr, "readARXDevice - SPI write returned a marker of 0x%04X instead of 0x%04X\n", temp, marker);
		exit(1);
	}
	
	
	/****************************/
	/* Read the register value */
	/****************************/
	
	// Read & write 2 bytes at a time making sure to return chip select to high 
	// when we are done.
	success = sub_spi_transfer(fh, simpleMarker, simpleResponse, 2, TRANS_SPI_INTERMEDIATE);
	if( success ) {
		fprintf(stderr, "readARXDevice - SPI write %i of %i - %s\n", 0, num, sub_strerror(sub_errno));
	}
	simpleResponse[0] ^= 0x80;
	temp = array_to_ushort(simpleResponse);
	if( num == device || device == 0 ) {
		printf("%i: 0x%04X\n", num, temp);
	}
	
	j = 1;
	for(i=num; i>0; i--) {
		if( i == device || device == 0 ) {
			if( j == num ) {
				// Final set of 2 bytes - chip select to high after transmitting
				success = sub_spi_transfer(fh, simpleNoOp, simpleResponse, 2, TRANS_SPI_FINAL);
			} else {
				success = sub_spi_transfer(fh, simpleNoOp, simpleResponse, 2, TRANS_SPI_INTERMEDIATE);
			}
			
		} else {
			if( j == num ) {
				// Final set of 2 bytes - chip select to high after transmitting
				success = sub_spi_transfer(fh, simpleNoOp, simpleResponse, 2, TRANS_SPI_FINAL);
			} else {
				success = sub_spi_transfer(fh, simpleNoOp, simpleResponse, 2, TRANS_SPI_INTERMEDIATE);
			}
		}
		
		if( success ) {
			fprintf(stderr, "readARXDevice - SPI write %i of %i - %s\n", i+1, num, sub_strerror(sub_errno));
			i += 1;
		}
		
		// Trim off the write bit from the command address
		simpleResponse[0] ^= 0x80;
		
		temp = array_to_ushort(simpleResponse);
		if( (num-j == device || device == 0) && num != j ) {
			printf("%i: 0x%04X\n", num-j, temp);
		}
		
		// Make sure we don't accidently kill the marker
		if( j == num ) {
			simpleResponse[0] ^= 0x80;
		}
		
		j += 1;
	}
	
	temp = array_to_ushort(simpleResponse);
	if( temp != marker ) {
		fprintf(stderr, "readARXDevice - SPI write returned a marker of 0x%04X instead of 0x%04X\n", temp, marker);
		exit(1);
	}
	
	
	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);
	
	return 0;
}
