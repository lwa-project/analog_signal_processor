#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libsub.h"

sub_handle* fh = NULL;


int main(int argc, char* argv[]) {
	/*************************
	* Command line parsing   *
	*************************/
	int success;
	char sn[20], message[20];
	
	// Make sure we have the right number of arguments to continue
	if( argc < 1+1 ) {
		fprintf(stderr, "writeARXLCD - Need %i arguments, %i provided\n", 1, argc-1);
		exit(1);
	}
	
	// Copy the string
	strncpy(message, argv[1], 17);


	/************************************
	* SUB-20 device selection and ready *
	************************************/
	struct usb_device* dev;

	// Open the USB device (or die trying)
	dev = NULL;
	fh = sub_open(dev);
	while( !fh ) {
		fprintf(stderr, "writeARXLCD - open - %s\n", sub_strerror(sub_errno));
		usleep(50000);
		fh = sub_open(dev);
	}
	
	success = sub_get_serial_number(fh, sn, sizeof(sn));
	if( !success ) {
		fprintf(stderr, "writeARXLCD - get sn - %s\n", sub_strerror(sub_errno));
		exit(1);
	}
	fprintf(stderr, "Found SUB-20 device S/N: %s\n", sn);


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
