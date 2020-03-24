/*****************************************************
sendARXDevice - Send a SPI command to the specified 
device.  An exit code of zero indicates that no errors
were encountered.
 
Usage:
  sendARXDevice <SUB-20 S/N> <total stand count> <device> <command>

  * Command is a four digit hexadecimal values (i.e., 
  0x1234)
  
Options:
  None
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
	int p, success, num, device, found;
	unsigned short temp;
	char requestedSN[20], sn[20], simpleMarker[2];
    int devices[MAX_BOARDS*STANDS_PER_BOARD] = { 0 };
    char simpleDatas[MAX_BOARDS*STANDS_PER_BOARD][2] = { { 0 } };
	char fullData[2*MAX_BOARDS*STANDS_PER_BOARD+2], fullResponse[2*MAX_BOARDS*STANDS_PER_BOARD+2];

	// Make sure we have the right number of arguments to continue
	if( argc < 4+1 ) {
		fprintf(stderr, "sendARXDevice - Need %i arguments, %i provided\n", 4, argc-1);
		exit(1);
	}
	
	// Copy the string
	strncpy(requestedSN, argv[1], 17);
	// Convert the strings into integer values
	num    = strtod(argv[2], &endptr);
    for(p=3; p<argc; p+=2) {
        devices[(p-3)/2] = strtod(argv[p+0], &endptr);
        hex_to_array(argv[p+1], simpleDatas[(p-3)/2]);
    }
    devices[(p-3)/2] = -1;
	ushort_to_array(marker, simpleMarker);
	
	// Make sure we have a device numbers that makes sense
    for(p=0; p<num; p++) {
        if( devices[p] < 0 ) {
            break;
        }
        if( devices[p] < 0 || devices[p] > num ) {
            fprintf(stderr, "sendARXDevice - Device #%i is out-of-range\n", devices[p]);
            exit(1);
        }
	}
	
	// Report on where we are at
	for(p=0; p<num; p++) {
        if( devices[p] < 0 ) {
            break;
        }
        temp = array_to_ushort(simpleDatas[p]);
        if( devices[p] != 0 ) {
            fprintf(stderr, "Sending data 0x%04X (%u) to device %i of %i\n", temp, temp, devices[p], num);
        } else {
            fprintf(stderr, "Sending data 0x%04X (%u) to all %i devices\n", temp, temp, num);
        }
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
		fprintf(stderr, "sendARXDevice - Cannot find or open SUB-20 %s\n", requestedSN);
		exit(1);
	}
	
	
	/****************************************
	* Send the command and get the response *
	****************************************/
	int i, j, k, l;
	char simpleResponse[2];
	
	// Enable the SPI bus operations on the SUB-20 board
	j = 0;
	success = sub_spi_config(fh, 0, &j);
	if( success ) {
		fprintf(stderr, "sendARXDevice - get config - %s\n", sub_strerror(sub_errno));
	}
	
	success = 1;
	while( success ) {
		success = sub_spi_config(fh, ARX_SPI_CONFIG, NULL);
		if( success ) {
			fprintf(stderr, "sendARXDevice - set config - %s\n", sub_strerror(sub_errno));
			exit(1);
		}
	}
	
	// Fill the data array with the commands to send
	j = 0;
	fullData[j++] = simpleMarker[0];
	fullData[j++] = simpleMarker[1];
	for(i=num; i>0; i--) {
        l = 0;
        for(k=0; k<num; k++) {
            if( devices[k] == -1 ) {
                break;
            }
            if( i == devices[k] || devices[k] == 0 ) {
                fullData[j++] = simpleDatas[k][0];
                fullData[j++] = simpleDatas[k][1];
                l = 1;
                break;
            }
        }
        if( l == 0 ) {
            fullData[j++] = 0;
            fullData[j++] = 0;
        }
	}
	
	// Read & write (2*num+2) bytes at a time making sure to return chip select to high 
	// when we are done.
	success = sub_spi_transfer(fh, fullData, fullResponse, 2*num+2, SS_CONF(0, SS_LO));
	if( success ) {
		fprintf(stderr, "sendARXDeviceBatch - SPI write %i of %i - %s\n", device, num, sub_strerror(sub_errno));
		exit(2);
	}
	
	// Check the command verification marker
	simpleResponse[0] = fullResponse[2*num];
	simpleResponse[1] = fullResponse[2*num+1];
	temp = array_to_ushort(simpleResponse);
	if( temp != marker ) {
		fprintf(stderr, "sendARXDevice - SPI write returned a marker of 0x%04X instead of 0x%04X\n", temp, marker);
		exit(3);
	}
	
	
	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);
	
	return 0;
}
