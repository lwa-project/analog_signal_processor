/*****************************************************
configPSU - Change configuration of the specified PSU

Usage:
  configPSU <SUB-20 S/N> <device address> <command>
  
  * Device addresses are two-digit hexadecimal numbers 
    (i.e. 0x1F)
  * Valid commands are:
     query - get the current configuration of the PSU
     autoOn - Automatically turn on the DC output on 
              power up
     autoOff - Automatically turn off the DC output 
               on power up
     tempWarm ##.# - Set the temperaure warning limit
                     to the specified value in degrees
                     C
     tempFault ##.# - Set the temperature fault limit
                      to the specified value in 
                      degrees C

Options:
  None

$Rev$
$LastChangedBy$
$LastChangedDate$
*****************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "libsub.h"
#include "aspCommon.h"

sub_handle* fh = NULL;


#define MODE_UNKOWN      0
#define MODE_QUERY     101
#define MODE_AUTOON    102
#define MODE_AUTOOFF   103
#define MODE_TEMPWARN  104
#define MODE_TEMPFAULT 105


int main(int argc, char* argv[]) {
	char *endptr;
	int i, device, success, nPSU, done, mode, found;
	float argument = 0.0;
	unsigned short temp;
	char psuAddresses[128], requestedSN[20], sn[20], command[33], simpleData[2], bigData[5];
	
	// Make sure we have the right number of arguments to continue
	if( argc < 3+1 ) {
		fprintf(stderr, "configPSU - Need %i arguments, %i provided\n", 3, argc-1);
		exit(1);
	}
	
	// Copy the string
	strncpy(requestedSN, argv[1], 17);
	// Convert the strings into integer values
	device   = strtod(argv[2], &endptr);
	// Copy the string and validate the command
	strncpy(command, argv[3], 32);
	mode = MODE_UNKOWN;
	if( !strncmp(command, "query", 5) ) {
		mode = MODE_QUERY;
	} else if( !strncmp(command, "autoOn", 6) ) {
		mode = MODE_AUTOON;
	} else if( !strncmp(command, "autoOff", 7) ) {
		mode = MODE_AUTOOFF;
	} else if( !strncmp(command, "tempWarn", 8) ) {
		mode = MODE_TEMPWARN;
		if( argc < 4+1 ) {
			fprintf(stderr, "configPSU - Need %i arguments for 'tempWarn', %i provided\n", 4, argc-1);
			exit(1);
		}
		argument = strtod(argv[4], &endptr);
	} else if( !strncmp(command, "tempFault", 9) ) {
		mode = MODE_TEMPFAULT;
		if( argc < 4+1 ) {
			fprintf(stderr, "configPSU - Need %i arguments for 'tempFault', %i provided\n", 4, argc-1);
			exit(1);
		}
		argument = strtod(argv[4], &endptr);
	}
		
	if( mode == MODE_UNKOWN ) {
		fprintf(stderr, "configPSU - Invalid command '%s'\n", command);
		exit(1);
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
		fprintf(stderr, "configPSU - Cannot find or open SUB-20 %s\n", requestedSN);
		exit(1);
	}
	
	/********************
	* Read from the I2C *
	********************/
	success = sub_i2c_scan(fh, &nPSU, psuAddresses);
	if( success ) {
		fprintf(stderr, "configPSU - get PSUs - %s\n", sub_strerror(sub_errno));
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
		
		if( mode != MODE_QUERY ) {
			// Enable writing to the OPERATION address (0x01) so we can change modules
			simpleData[0] = (unsigned char) 0;
			success = sub_i2c_write(fh, psuAddresses[i], 0x10, 1, simpleData, 1);
			if( success ) {
				fprintf(stderr, "configPSU - write settings - %s\n", sub_strerror(sub_errno));
				continue;
			}
		}
		
		// Go!
		switch(mode) {
			case MODE_QUERY:
				// Querty the PSU setup
				simpleData[0] = (unsigned char) 0;
				success = sub_i2c_read(fh, psuAddresses[i], 0xD6, 1, simpleData, 1);
				if( success ) {
					fprintf(stderr, "configPSU - get setup - %s\n", sub_strerror(sub_errno));
					continue;
				}
				switch ((simpleData[0] >> 0) & 3) {
					case 3:
						printf("Config. Data Source:       User\n");
						break;
					case 2:
						printf("Config. Data Source:       Default\n");
						break;
					case 1:
						printf("Config. Data Source:       Firmware\n");
						break;
					default:
						printf("Config. Data Source:       Memory\n");
						break;
				}
				printf("DC Used for Input:         %i\n", (simpleData[0] >> 3) & 1);
				
				// Query the PSU configuation
				simpleData[0] = (unsigned char) 0;
				success = sub_i2c_read(fh, psuAddresses[i], 0xD5, 1, simpleData, 1);
				if( success ) {
					fprintf(stderr, "configPSU - get configuration - %s\n", sub_strerror(sub_errno));
					continue;
				}
				printf("Fan Alarm Disabled:        %i\n", (simpleData[0] >> 0) & 1);
				printf("Fan Off at Standby:        %i\n", (simpleData[0] >> 1) & 1);
				printf("Fan Direction Reversed:    %i\n", (simpleData[0] >> 2) & 1);
				printf("DC Output ON with Power:   %i\n", (simpleData[0] >> 7) & 1);
				
				// Query temperature limits
				success = sub_i2c_read(fh, psuAddresses[i], 0x51, 1, simpleData, 2);
				if( success ) {
					fprintf(stderr, "configPSU - get temperature warning - %s\n", sub_strerror(sub_errno));
					continue;
				}
				temp = array_to_ushort(simpleData);
				printf("Temperature Warning Limit: %5.2f C\n", temp/4.0);
				
				success = sub_i2c_read(fh, psuAddresses[i], 0x4F, 1, simpleData, 2);
				if( success ) {
					fprintf(stderr, "configPSU - get temperature fault - %s\n", sub_strerror(sub_errno));
					continue;
				}
				temp = array_to_ushort(simpleData);
				printf("Temperature Fault Limit:   %5.2f C\n", temp/4.0);
				
				// Query power limits
				success = sub_i2c_read(fh, psuAddresses[i], 0xEB, 1, bigData, 5);
				if( success ) {
					fprintf(stderr, "configPSU - get power limits - %s\n", sub_strerror(sub_errno));
					continue;
				}
				simpleData[0] = bigData[1];
				simpleData[1] = bigData[2];
				temp = array_to_ushort(simpleData);
				printf("Low Power Limit:           %i W\n", temp);
				simpleData[0] = bigData[3];
				simpleData[1] = bigData[4];
				temp = array_to_ushort(simpleData);
				printf("High Power Limit:          %i W\n", temp);
				break;
				
			case MODE_AUTOOFF:
				// Query the PSU configuation
				simpleData[0] = (unsigned char) 0;
				success = sub_i2c_read(fh, psuAddresses[i], 0xD5, 1, simpleData, 1);
				if( success ) {
					fprintf(stderr, "configPSU - get configuration - %s\n", sub_strerror(sub_errno));
					continue;
				}
				
				// Update the default operation flag
				simpleData[0] = (simpleData[0] & 63) | (0 << 7);
				
				// Write the PSU configuation
				success = sub_i2c_write(fh, psuAddresses[i], 0xD5, 1, simpleData, 1);
				if( success ) {
					fprintf(stderr, "configPSU - set configuration - %s\n", sub_strerror(sub_errno));
					continue;
				}
				
				// Save the configutation as default
				simpleData[0] = 0x21;
				success = sub_i2c_write(fh, psuAddresses[i], 0x15, 1, simpleData, 1);
				if( success ) {
					fprintf(stderr, "configPSU - save configuration - %s\n", sub_strerror(sub_errno));
					continue;
				}
				break;
				
			case MODE_AUTOON:
				// Query the PSU configuation
				simpleData[0] = (unsigned char) 0;
				success = sub_i2c_read(fh, psuAddresses[i], 0xD5, 1, simpleData, 1);
				if( success ) {
					fprintf(stderr, "configPSU - get configuration - %s\n", sub_strerror(sub_errno));
					continue;
				}
				
				// Update the default operation flag
				simpleData[0] = (simpleData[0] & 63) | (1 << 7);
				
				// Write the PSU configuation
				success = sub_i2c_write(fh, psuAddresses[i], 0xD5, 1, simpleData, 1);
				if( success ) {
					fprintf(stderr, "configPSU - set configuration - %s\n", sub_strerror(sub_errno));
					continue;
				}
				
				// Save the configutation as default
				simpleData[0] = 0x21;
				success = sub_i2c_write(fh, psuAddresses[i], 0x15, 1, simpleData, 1);
				if( success ) {
					fprintf(stderr, "configPSU - save configuration - %s\n", sub_strerror(sub_errno));
					continue;
				}
				break;
				
			case MODE_TEMPWARN:
				// Convert to the right format
				temp = (unsigned short) (argument*4);
				
				// Write to memory
				ushort_to_array(temp, simpleData);
				success = sub_i2c_write(fh, psuAddresses[i], 0x51, 1, simpleData, 2);
				if( success ) {
					fprintf(stderr, "configPSU - set temperature warning - %s\n", sub_strerror(sub_errno));
					continue;
				}
				
				// Save the configutation as default
				simpleData[0] = 0x21;
				success = sub_i2c_write(fh, psuAddresses[i], 0x15, 1, simpleData, 1);
				if( success ) {
					fprintf(stderr, "configPSU - save configuration - %s\n", sub_strerror(sub_errno));
					continue;
				}
				break;
				
			case MODE_TEMPFAULT:
				// Convert to the right format
				temp = (unsigned short) (argument*4);
				
				// Write to memory
				ushort_to_array(temp, simpleData);
				success = sub_i2c_write(fh, psuAddresses[i], 0x4F, 1, simpleData, 2);
				if( success ) {
					fprintf(stderr, "configPSU - set temperature warning - %s\n", sub_strerror(sub_errno));
					continue;
				}
				
				// Save the configutation as default
				simpleData[0] = 0x21;
				success = sub_i2c_write(fh, psuAddresses[i], 0x15, 1, simpleData, 1);
				if( success ) {
					fprintf(stderr, "configPSU - save configuration - %s\n", sub_strerror(sub_errno));
					continue;
				}
				break;
				
			default:
				break;
		}
		
		if( mode != MODE_QUERY ) {// Write-protect all entries but WRITE_PROTECT (0x10)
			simpleData[0] = (unsigned char) ((1 << 7) & 1);
			success = sub_i2c_write(fh, psuAddresses[i], 0x10, 1, simpleData, 1);
			if( success ) {
				fprintf(stderr, "configPSU - write settings - %s\n", sub_strerror(sub_errno));
				continue;
			}
		}
		
		// Mark that we have sone something
		done = 1;
	}
	
	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);
	
	if( !done ) {
		fprintf(stderr, "configPSU - Cannot find device at address 0x%02X\n", device);
		exit(1);
	}
	return 0;
}
