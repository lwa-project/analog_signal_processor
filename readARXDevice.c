/*****************************************************
readARXDevice - Read a SPI register from the specified 
device.  An exit code of zero indicates that no errors
were encountered.
 
Usage:
  readARXDevice <total stand count> <device> <command>

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
	int success, num, device;
	unsigned short temp;
	char sn[20], simpleData[2], simpleNoOp[2], simpleMarker[2];

	// Make sure we have the right number of arguments to continue
	if( argc != 3+1 ) {
		fprintf(stderr, "readARXDevice - Need %i arguments, %i provided\n", 3, argc-1);
		exit(1);
	}
	
	// Convert the strings into integer values
	num    = strtod(argv[1], &endptr);
	device = strtod(argv[2], &endptr);
	hex_to_array(argv[3], simpleData);
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
		fprintf(stderr, "Sending data 0x%04X (%u) to device %i of %i\n", temp, temp, device, num);
	} else {
		fprintf(stderr, "Sending data 0x%04X (%u) to all %i devices\n", temp, temp, num);
	}


	/************************************
	* SUB-20 device selection and ready *
	************************************/
	struct usb_device* dev;

	// Open the USB device (or die trying)
	dev = NULL;
	fh = sub_open(dev);
	while( !fh ) {
		fprintf(stderr, "readARXDevice - open - %s\n", sub_strerror(sub_errno));
		usleep(10000);
		fh = sub_open(dev);
	}
	
	success = sub_get_serial_number(fh, sn, sizeof(sn));
	if( !success ) {
		fprintf(stderr, "readARXDevice - get sn - %s\n", sub_strerror(sub_errno));
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
	printf("%i: 0x%04X\n", num, temp);
	
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
		printf("%i: 0x%04X\n", num-j, temp);
		
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
