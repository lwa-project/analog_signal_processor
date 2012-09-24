/*****************************************************
sendARXDeviceBatch - Send a set SPI commands written
in a file to the specified devices.  An exit code of 
zero indicates that no errors
were encountered.
 
Usage:
  sendARXDeviceBatch <filename>

  * The file format is:
    1) Total stand count, 
    2) Device number, and
    3) Command, where command is a four digit 
       hexadecimal values (i.e., 0x1234)
  
Options:
  None

$Rev$
$LastChangedBy$
$LastChangedDate$
*****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>


#include "libsub.h"
#include "aspCommon.h"

sub_handle* fh = NULL;


int main(int argc, char* argv[]) {
	/*************************
	* Command line parsing   *
	*************************/
	int success, num, device;
	unsigned short temp;
	char sn[20], simpleData[2], simpleNoOp[2], simpleMarker[2];
	char command[8];
	FILE *spifile;
	size_t len = 0;
	ssize_t read;
	char *line = NULL;
	int args;
	
	// Make sure we have the right number of arguments to continue
	if( argc != 1+1 ) {
		fprintf(stderr, "sendARXDeviceBatch - Need %i arguments, %i provided\n", 1, argc-1);
		exit(1);
	}
	
	// Open the file
	spifile = fopen(argv[1], "r");
	if( spifile == NULL ) {
		printf("sendARXDeviceBatch - Cannot open SPI file: %s\n", argv[1]);
		exit(errno);
	}
	
	/************************************
	* SUB-20 device selection and ready *
	************************************/
	struct usb_device* dev;
	
	// Open the USB device (or die trying)
	dev = NULL;
	fh = sub_open(dev);
	while( !fh ) {
		fprintf(stderr, "sendARXDeviceBatch - open - %s\n", sub_strerror(sub_errno));
		usleep(10000);
		fh = sub_open(dev);
	}
	
	success = sub_get_serial_number(fh, sn, sizeof(sn));
	if( !success ) {
		fprintf(stderr, "sendARXDeviceBatch - get sn - %s\n", sub_strerror(sub_errno));
		exit(1);
	}
	fprintf(stderr, "Found SUB-20 device S/N: %s\n", sn);
	
	
	/******************************************
	* Send the commands and get the responses *
	******************************************/
	
	int i, j;
	char simpleResponse[2];
	hex_to_array("0x0000", simpleNoOp);
	ushort_to_array(marker, simpleMarker);
	
	// Enable the SPI bus operations on the SUB-20 board
	j = 0;
	success = sub_spi_config(fh, 0, &j);
	if( success ) {
		fprintf(stderr, "sendARXDeviceBatch - get config - %s\n", sub_strerror(sub_errno));
	}
	
	success = 1;
	while( success ) {
		success = sub_spi_config(fh, ARX_SPI_CONFIG, NULL);
		if( success ) {
			fprintf(stderr, "sendARXDeviceBatch - set config - %s\n", sub_strerror(sub_errno));
			exit(1);
		}
	}
	
	// Read in the SPI file lines
	while( (read = getline(&line, &len, spifile) != EOF) ) {
		args = sscanf(line, "%d %d %6s", &num, &device, command);
		
		if (args > 2 ) {
			// Convert the command to an array
			hex_to_array(command, simpleData);
			
			// Make sure we have a device number that makes sense
			if( device < 0 || device > num ) {
				fprintf(stderr, "sendARXDeviceBatch - Device #%i is out-of-range\n", device);
				continue;
			}
			
			// Report on where we are at
			temp = array_to_ushort(simpleData);
			if( device != 0 ) {
				fprintf(stderr, "Sending data 0x%04X (%u) to device %i of %i\n", temp, temp, device, num);
			} else {
				fprintf(stderr, "Sending data 0x%04X (%u) to all %i devices\n", temp, temp, num);
			}
			
			// Read & write 2 bytes at a time making sure to return chip select to high 
			// when we are done.
			success = sub_spi_transfer(fh, simpleMarker, simpleResponse, 2, TRANS_SPI_INTERMEDIATE);
			if( success ) {
				fprintf(stderr, "sendARXDeviceBatch - SPI write %i of %i - %s\n", 0, num, sub_strerror(sub_errno));
			}
			
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
					fprintf(stderr, "sendARXDeviceBatch - SPI write %i of %i - %s\n", i+1, num, sub_strerror(sub_errno));
					i += 1;
				}
				
				j += 1;
			}
			
			temp = array_to_ushort(simpleResponse);
			if( temp != marker ) {
				fprintf(stderr, "sendARXDeviceBatch - SPI write returned a marker of 0x%04X instead of 0x%04X\n", temp, marker);
				exit(1);
			}
		}
	}

	/*******************
	* Cleanup and exit *
	*******************/
	fclose(spifile);
	printf("sendARXDeviceBatch - OK\n");
	
	sub_close(fh);

	return 0;
}
