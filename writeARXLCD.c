#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libsub.h"

sub_handle* fh = NULL;


int main(int argc, char* argv[]) {
	/*************************
	* Command line parsing   *
	*************************/
	int success, found;
	char requestedSN[20], sn[20], message[20];
	
	// Make sure we have the right number of arguments to continue
	if( argc < 2+1 ) {
		fprintf(stderr, "writeARXLCD - Need %i arguments, %i provided\n", 2, argc-1);
		exit(1);
	}
	
	// Copy the string
	strncpy(requestedSN, argv[1], 17);
	strncpy(message, argv[2], 17);
	
	
	/************************************
	* SUB-20 device selection and ready *
	************************************/
	struct usb_device* dev;

	// Find the right SUB-20
	found = 0;
	int openTries = 0;
	while( dev = sub_find_devices(dev) ) {
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
		fprintf(stderr, "writeARXLCD - Cannot find or open SUB-20 %s\n", requestedSN);
		exit(1);
	}


	/***************************
	* Send the text to the LCD *
	***************************/
	char lcd_str[65];
	
	// Clear and update the LCD screen
	sprintf(lcd_str, "\f%s", message);
	success = sub_lcd_write(fh, lcd_str);
	if( success ) {
		fprintf(stderr, "writeARXLCD - lcd write - %s\n", sub_strerror(sub_errno));
		exit(1);
	}


	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);

	return 0;
}
