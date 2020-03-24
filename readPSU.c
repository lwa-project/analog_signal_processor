/*****************************************************
readPSUs - Program to pull information about all power
supplies found on the I2C bus.  The data polled 
includes:
 * on/off status
 * general module health (DC OK, over current, etc.)
 * output voltage
 * output current
 
Usage:
  readPSUs <SUB-20 S/N> <I2C address>

Options:
  None
*****************************************************/


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "libsub.h"
#include "aspCommon.h"

sub_handle* fh = NULL;


void getModuleName(unsigned short, int, char*, int);
void getModulePower(unsigned short, char*, int);
void getModuleStatus(unsigned short, char*, int);


int main(int argc, char* argv[]) {
	char *endptr;
	int i, device, success, nPSU, nMod, done, code, found;
	unsigned short temp, modules, page, status;
	float voltage, current;
	char psuAddresses[128], j, requestedSN[20], sn[20], simpleData[2], bigData[4];
	char moduleName[65], modulePower[4], moduleStatus[129];
	
	// Make sure we have the right number of arguments to continue
	if( argc != 2+1 ) {
		fprintf(stderr, "readPSU - Need %i arguments, %i provided\n", 2, argc-1);
		exit(1);
	}
	
	// Copy the string
	strncpy(requestedSN, argv[1], 17);
	// Convert the strings into integer values
	device = strtod(argv[2], &endptr);
	
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
		fprintf(stderr, "readPSU - Cannot find or open SUB-20 %s\n", requestedSN);
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

		// Enable writing to all of the supported command so we can change 
		// modules/poll module type
		simpleData[0] = (unsigned char) 0;
		success = sub_i2c_write(fh, psuAddresses[i], 0x10, 1, simpleData, 1);
		if( success ) {
			fprintf(stderr, "readPSU - write settings - %s\n", sub_strerror(sub_errno));
			continue;
		}

		// Loop over modules 0 through 15
		simpleData[0] = 0;
		bigData[0] = 0;
		bigData[1] = 0;
		bigData[2] = 0;
		bigData[3] = 0;
		
		nMod = 0;
		moduleName[0] = '\0';
		modulePower[0] = '\0';
		moduleStatus[0] = '\0';
		voltage = 0.0;
		current = 0.0;
		for(j=0; j<16; j++) {
			/****************
			* Module Change *
			****************/
			
			// Skip "dumb" modules
			if( ((modules >> j) & 1) == 0 ) {
				continue;
			}
			nMod += 1;
			
			page = (unsigned short) 17;
			while( page != j ) {
				// Jump to the correct page and give the PSU a second to get ready
				simpleData[0] = j;
				success = sub_i2c_write(fh, psuAddresses[i], 0x00, 1, simpleData, 1);
				if( success ) {
					fprintf(stderr, "readPSU - page change - %s\n", sub_strerror(sub_errno));
					exit(1);
				}
				
				usleep(10000);
				
				// Verify the current page
				success = sub_i2c_read(fh, psuAddresses[i], 0x00, 1, simpleData, 1);
				if( success ) {
					fprintf(stderr, "readPSU - get page - %s\n", sub_strerror(sub_errno));
					continue;
				}
				simpleData[1] = 0;
				page = array_to_ushort(simpleData);
			}

			/***********************
			* Module Name and Type *
			***********************/
			
			#ifdef __DECODE_MODULE_TYPE__
				temp = (unsigned short) bigData[3];
				while( (unsigned short) bigData[3] == temp ) {
					simpleData[0] = 0;
					success = sub_i2c_write(fh, psuAddresses[i], 0xDE, 1, simpleData, 1);
					if( success ) {
						fprintf(stderr, "readPSU - get type - %s\n", sub_strerror(sub_errno));
						exit(1);
					}
					
					usleep(100000);
					
					success = sub_i2c_read(fh, psuAddresses[i], 0xDF, 1, bigData, 4);
					if( success ) {
						fprintf(stderr, "readPSU - get type - %s\n", sub_strerror(sub_errno));
						continue;
					}
					
					printf("%u %u %u %u\n", bigData[0], bigData[1], bigData[2], bigData[3]);
				}
				code = (int) (bigData[3] & 0xFF);
				getModuleName(page, code, (char *) moduleName, 1);
			#else
				code = 0;
				if( moduleName[0] == '\0' ) {
					sprintf(moduleName, "Module%02i", page);
				} else {
					sprintf(moduleName, "%s|Module%02i", moduleName, page);
				}
			#endif

			/*************************
			* Power State and Status *
			*************************/
			
			success = sub_i2c_read(fh, psuAddresses[i], 0xDB, 1, simpleData, 1);
			if( success ) {
				fprintf(stderr, "readPSU - get status - %s\n", sub_strerror(sub_errno));
				continue;
			}
			status = array_to_ushort(simpleData);
			if( modulePower[0] == '\0' || modulePower[1] == 'N' ) {
				getModulePower(status, (char *) modulePower, 0);
			}
			getModuleStatus(status, (char *) moduleStatus, 1);

			/*****************
			* Output Voltage *
			*****************/
			
			success = sub_i2c_read(fh, psuAddresses[i], 0x8B, 1, simpleData, 2);
			if( success ) {
				fprintf(stderr, "readPSU - get output voltage - %s\n", sub_strerror(sub_errno));
				continue;
			}
			temp = array_to_ushort(simpleData);
			voltage += temp/100.0;

			/*****************
			* Output Current *
			*****************/
			
			#ifdef __USE_INPUT_CURRENT__
				success = sub_i2c_read(fh, psuAddresses[i], 0x89, 1, simpleData, 2);
				if( success ) {
					fprintf(stderr, "readPSUs - get input current - %s\n", sub_strerror(sub_errno));
					continue;
				}
				temp = array_to_ushort(simpleData);
				current = temp/100.0 * 0.95;		// Removes the ~5% power conversion loss
			#else
				success = sub_i2c_read(fh, psuAddresses[i], 0x8C, 1, simpleData, 2);
				if( success ) {
					fprintf(stderr, "readPSU - get output current - %s\n", sub_strerror(sub_errno));
					continue;
				}
				temp = array_to_ushort(simpleData);
				current += temp/100.0;
			#endif
		}
		
		// Print mean output voltage and the total current
		if( nMod != 0 ) {
			voltage /= (float) nMod;
		}
		printf("0x%02X %s %s %s %.2f %.2f", psuAddresses[i], moduleName, modulePower, moduleStatus, voltage, current);

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


void getModuleName(unsigned short page, int moduleCode, char* moduleName, int append) {
	int n, m;
	
	if( append && *(moduleName + 0) != '\0' ) {
		n = sprintf(moduleName, "%s|Module%02i", moduleName, page);
	} else {
		n = sprintf(moduleName, "Module%02i", page);
	}
	m = n;
	
	// Decode the power rating of the current module
	if( ((moduleCode >> 4 ) & 0xF) == 0 ) {
		n = sprintf(moduleName, "%s_210W", moduleName);
	}
	if( ((moduleCode >> 4 ) & 0xF) == 1 ) {
		n = sprintf(moduleName, "%s_360W", moduleName);
	}
	if( ((moduleCode >> 4 ) & 0xF) == 2 ) {
		n = sprintf(moduleName, "%s_144W", moduleName);
	}
	if( ((moduleCode >> 4 ) & 0xF) == 3 ) {
		n = sprintf(moduleName, "%s_600W", moduleName);
	}
	if( ((moduleCode >> 4 ) & 0xF) == 4 ) {
		n = sprintf(moduleName, "%s_750W", moduleName);
	}
	if( ((moduleCode >> 4) & 0xF) == 5 ) {
		n = sprintf(moduleName, "%s_1500W", moduleName);
	}
	
	if( n == m ) {
		n = sprintf(moduleName, "%s_UNK", moduleName);
	}
	m = n;
	
	// Decode the voltage range of the current module
	if( (moduleCode & 0xF) == 0 ) {
		n = sprintf(moduleName, "%s_2to5.5V", moduleName);
	}
	if( (moduleCode & 0xF) == 1 ) {
		n = sprintf(moduleName, "%s_6to12V", moduleName);
	}
	if( (moduleCode & 0xF) == 2 ) {
		n = sprintf(moduleName, "%s_14to20V", moduleName);
	}
	if( (moduleCode & 0xF) == 3 ) {
		n = sprintf(moduleName, "%s_24to36V", moduleName);
	}
	if( (moduleCode & 0xF) == 4 ) {
		n = sprintf(moduleName, "%s_42to60V", moduleName);
	}
	if( (moduleCode & 0xF) == 5 ) {
		n = sprintf(moduleName, "%s_fixed5V", moduleName);
	}
	if( (moduleCode & 0xF) == 6 ) {
		n = sprintf(moduleName, "%s_2to6V", moduleName);
	}
	if( (moduleCode & 0xF) == 7 ) {
		n = sprintf(moduleName, "%s_12to15V", moduleName);
	}
	if( (moduleCode & 0xF) == 8 ) {
		n = sprintf(moduleName, "%s_24to28V", moduleName);
	}
	if( (moduleCode & 0xF) == 9 ) {
		sprintf(moduleName, "%s_24to30V", moduleName);
	}
	if( (moduleCode & 0xF) == 10 ) {
		n = sprintf(moduleName, "%s_33to60V", moduleName);
	}
	
	if( n == m ) {
		n = sprintf(moduleName, "%s_UNK", moduleName);
	}
	m = n;
}


void getModulePower(unsigned short statusCode, char *modulePower, int append) {
	if( append && *(modulePower + 0) != '\0' ) {
		sprintf(modulePower, "%s|", modulePower);
	} else {
		*(modulePower + 0) = '\0';
	}
	
	// Decode the output enabled flag
	if( statusCode & 1 ) {
		sprintf(modulePower, "%sON", modulePower);
	} else {
		sprintf(modulePower, "%sOFF", modulePower);
	}
}


void getModuleStatus(unsigned short statusCode, char* moduleStatus, int append) {
	int n, m;
	
	if( append && *(moduleStatus + 0) != '\0' ) {
		n = sprintf(moduleStatus, "%s|", moduleStatus);
	} else {
		*(moduleStatus + 0) = '\0';
		n = 0;
	}
	m = n;
	
	// Decode the various status and fault flags
	if( (statusCode >> 1) & 1 ) {
		if( n == m ) {
			n = sprintf(moduleStatus, "%sUnderVolt", moduleStatus);
		} else {
			n = sprintf(moduleStatus, "%s&UnderVolt", moduleStatus);
		}
	}
	if( (statusCode >> 2) & 1 ) {
		if( n == m ) {
			n = sprintf(moduleStatus, "%sOK", moduleStatus);
		} else {
			n = sprintf(moduleStatus, "%s&OK", moduleStatus);
		}
	}
	if( (statusCode >> 3) & 1 ) {
		if( n == m ) {
			n = sprintf(moduleStatus, "%sOverCurrent", moduleStatus);
		} else {
			n = sprintf(moduleStatus, "%s&OverCurrent", moduleStatus);
		}
	}
	if( (statusCode >> 4) & 1 ) {
		if( n == m ) {
			n = sprintf(moduleStatus, "%sOverTemperature", moduleStatus);
		} else {
			n = sprintf(moduleStatus, "%s&OverTemperature", moduleStatus);
		}
	}
	if( (statusCode >> 5) & 1 ) {
		if( n == m ) {
			n = sprintf(moduleStatus, "%sWarningTemperature", moduleStatus);
		} else {
			n = sprintf(moduleStatus, "%s&WarningTemperature", moduleStatus);
		}
	}
	if( (statusCode >> 6) & 1 ) {
		if( n == m ) {
			n = sprintf(moduleStatus, "%sOverVolt", moduleStatus);
		} else {
			n = sprintf(moduleStatus, "%s&OverVolt", moduleStatus);
		}
	}
	if( (statusCode >> 7) & 1 ) {
		if( n == m ) {
			n = sprintf(moduleStatus, "%sModuleFault", moduleStatus);
		} else {
			n = sprintf(moduleStatus, "%s&ModuleFault", moduleStatus);
		}
	}
	
	// Make sure we return something...
	if( n == m ) {
		n = sprintf(moduleStatus, "%sUNK", moduleStatus);
	}
}
