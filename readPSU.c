/*****************************************************
readPSUs - Program to pull information about all power
supplies found on the I2C bus.  The data polled 
includes:
 * on/off status
 * general module health (DC OK, over current, etc.)
 * output voltage
 * output current
 
Usage:
  readPSUs

Options:
  None

$Rev$
$LastChangedBy$
$LastChangedDate$
*****************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include "libsub.h"
#include "aspCommon.h"

sub_handle* fh = NULL;


int main(int argc, char* argv[]) {
	char *endptr;
	int i, device, success, nPSU, done;
	unsigned short temp, modules, page, status;
	float voltage, current;
	char psuAddresses[128], j, sn[20], simpleData[2];
	
	// Make sure we have the right number of arguments to continue
	if( argc != 1+1 ) {
		fprintf(stderr, "readPSU - Need %i arguments, %i provided\n", 1, argc-1);
		exit(1);
	}
	
	// Convert the strings into integer values
	device   = strtod(argv[1], &endptr);
	
	/************************************
	* SUB-20 device selection and ready *
	************************************/
	struct usb_device* dev;
	
	// Open the USB device (or die trying)
	dev = NULL;
	fh = sub_open(dev);
	while( !fh ) {
		fprintf(stderr, "readPSU - open - %s\n", sub_strerror(sub_errno));
		usleep(50000);
		fh = sub_open(dev);
	}
	
	success = sub_get_serial_number(fh, (char *) sn, sizeof(sn));
	if( !success ) {
		fprintf(stderr, "readPSU - get sn - %s\n", sub_strerror(sub_errno));
		exit(1);
	}
	
	/********************
	* Read from the I2C *
	********************/
	success = sub_i2c_scan(fh, &nPSU, psuAddresses);
	if( success ) {
		fprintf(stderr, "readPSU - get PSUs - %s\n", sub_strerror(sub_errno));
		exit(1);
	}

	simpleData[0] = 0;
	simpleData[1] = 0;
	done = 0;
	for(i=0; i<nPSU; i++) {
		// See if we have the right device
		if( psuAddresses[i] != device ) {
			continue;
		}
		
		// Get a list of smart modules for polling
		success = sub_i2c_read(fh, psuAddresses[i], 0xD3, 1, simpleData, 2);
		if( success ) {
			fprintf(stderr, "readPSU - module status - %s\n", sub_strerror(sub_errno));
			continue;
		}
		modules = array_to_ushort(simpleData);

		// Enable writing to the PAGE address (0x00) so we can change modules
		simpleData[0] = (unsigned char) ((1 << 6) & 1);
		success = sub_i2c_write(fh, psuAddresses[i], 0x10, 1, simpleData, 1);
		if( success ) {
			fprintf(stderr, "readPSU - write settings - %s\n", sub_strerror(sub_errno));
			continue;
		}

		// Loop over modules 0 through 7
		simpleData[0] = 0;
		for(j=0; j<16; j++) {
			// Skip "dumb" modules
			if( ((modules >> j) & 1) == 0 ) {
				continue;
			}

			// Jump to the correct page and give the PSU a second to get ready
			simpleData[0] = j;
			success = sub_i2c_write(fh, psuAddresses[i], 0x00, 1, simpleData, 1);
			if( success ) {
				fprintf(stderr, "readPSU - page change - %s\n", sub_strerror(sub_errno));
				continue;
			}
			usleep(20000);

			// Verify the current page
			success = sub_i2c_read(fh, psuAddresses[i], 0x00, 1, simpleData, 1);
			if( success ) {
				fprintf(stderr, "readPSU - get page - %s\n", sub_strerror(sub_errno));
				continue;
			}
			simpleData[1] = 0;
			page = array_to_ushort(simpleData);

			/*********
			* Status *
			*********/

			success = sub_i2c_read(fh, psuAddresses[i], 0xDB, 1, simpleData, 1);
			if( success ) {
				fprintf(stderr, "readPSU - get status - %s\n", sub_strerror(sub_errno));
				continue;
			}
			status = array_to_ushort(simpleData);

			/*******************************
			* Get Input and Output Voltage *
			*******************************/

// 			success = sub_i2c_read(fh, psuAddresses[i], 0x88, 1, simpleData, 2);
// 			if( success ) {
// 				fprintf(stderr, "readPSUs - get input voltage - %s\n", sub_strerror(sub_errno));
// 				continue;
// 			}
// 			temp = array_to_ushort(simpleData);

			success = sub_i2c_read(fh, psuAddresses[i], 0x8B, 1, simpleData, 2);
			if( success ) {
				fprintf(stderr, "readPSU - get output voltage - %s\n", sub_strerror(sub_errno));
				continue;
			}
			temp = array_to_ushort(simpleData);
			voltage = temp/100.0;

			/*********************
			* Get Output Current *
			*********************/

			success = sub_i2c_read(fh, psuAddresses[i], 0x89, 1, simpleData, 2);
			if( success ) {
				fprintf(stderr, "readPSUs - get input current - %s\n", sub_strerror(sub_errno));
				continue;
			}
			temp = array_to_ushort(simpleData);
			current = temp/100.0;

// 			success = sub_i2c_read(fh, psuAddresses[i], 0x8C, 1, simpleData, 2);
// 			if( success ) {
// 				fprintf(stderr, "readPSU - get output current - %s\n", sub_strerror(sub_errno));
// 				continue;
// 			}
// 			temp = array_to_ushort(simpleData);
// 			current = temp/100.0;
			
			if( status & 1 ) { 
				printf("0x%02X Module%02i ON ", psuAddresses[i], page);
			} else {
				printf("0x%02X Module%02i OFF ", psuAddresses[i], page);
			}
			if( (status >> 1) & 1 ) {
				printf("UnderVolt ");
			} else {
				if( (status >> 2) & 1 ) {
					printf("OK ");
				} else {
					if( (status >> 3) & 1 ) {
						printf("OverCurrent ");
					} else {
						if( (status >> 4) & 1 ) {
							printf("OverTemperature ");
						} else {
							if( (status >> 5) & 1 ) {
								printf("WarningTemperature ");
							} else {
								if( (status >> 6) & 1 ) {
									printf("OverVolt ");
								} else {
									if( (status >> 7) & 1 ) {
										printf("ModuleFault ");
									} else {
										printf("UNK ");
									}
								}
							}
						}
					}
				}
			}
			printf("%.2f %.2f\n", voltage, current);
			
			// Only work on one module (the first one)
			break;
		}

		// Set the module number back to 0
		simpleData[0] = (unsigned char) 0;
		success = sub_i2c_write(fh, psuAddresses[i], 0x00, 1, simpleData, 1);
		if( success ) {
			fprintf(stderr, "readPSU - page change - %s\n", sub_strerror(sub_errno));
			continue;
		}

		// Write-protect all entries but WRITE_PROTECT (0x10)
		simpleData[0] = (unsigned char) ((1 << 7) & 1);
		success = sub_i2c_write(fh, psuAddresses[i], 0x10, 1, simpleData, 1);
		if( success ) {
			fprintf(stderr, "readPSU - write settings - %s\n", sub_strerror(sub_errno));
			continue;
		}
		
		done = 1;

	}
	
	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);
	
	if( !done ) {
		fprintf(stderr, "readPSU - Cannot find device at address 0x%02X\n", device);
		exit(1);
	}
	return 0;
}

