/*****************************************************
readThermometers - Program to pull information about 
temperature sensors found on the I2C bus.  The data 
polled includes temperatures from:
  * case
  * primary side input
  * modules
 
Usage:
  readThermometers <SUB-20 S/N>

Options:
  None
*****************************************************/

#include <iostream>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>

#include "libsub.h"
#include "aspCommon.h"

int main(int argc, char** argv) {
  /*************************
  * Command line parsing   *
  *************************/
  // Make sure we have the right number of arguments to continue
  if( argc < 1+1 ) {
    std::cout << "readThermometers - Need 1 argument, " << argc-1 << " provided" << std::endl;
    exit(1);
  }
  
  std::string requestedSN = std::string(argv[1]);
  
  /************************************
  * SUB-20 device selection and ready *
  ************************************/
  sub_device dev = NULL;
  sub_handle fh = NULL;
        
  // Find the right SUB-20
  bool found = false;
  char foundSN[20];
  int success, openTries = 0;
  while( (!found) && (dev = sub_find_devices(dev)) ) {
    // Open the USB device (or die trying)
		fh = sub_open(dev);
    while( (fh == NULL) && (openTries < SUB20_OPEN_MAX_ATTEMPTS) ) {
			openTries++;
			std::this_thread::sleep_for(std::chrono::milliseconds(SUB20_OPEN_WAIT_US/1000));
			
			fh = sub_open(dev);
		}
		if( fh == NULL ) {
			continue;
		}
		
		success = sub_get_serial_number(fh, foundSN, sizeof(foundSN));
		if( !success ) {
			continue;
		}
    
    if( !strcmp(foundSN, requestedSN.c_str()) ) {
			std::cout << "Found SUB-20 device S/N: " << foundSN << std::endl;
			found = true;
		} else {
			sub_close(fh);
		}
	}
	
	// Make sure we actually have a SUB-20 device
	if( !found ) {
		std::cout << "readThermometers - Cannot find or open SUB-20 " << requestedSN << std::endl;
		exit(1);
	}
  
  /********************
	* Read from the I2C *
	********************/
  int num, nPSU;
  char psuAddresses[128];
  success = sub_i2c_scan(fh, &nPSU, psuAddresses);
	if( success ) {
		std::cout << "readThermometers - get PSUs - " << sub_strerror(sub_errno) << std::endl;
		exit(1);
	}
  
  uint16_t data, modules, page;
	for(int i=0; i<nPSU; i++) {
		if( psuAddresses[i] > 0x1F ) {
			continue;
		}

		#ifdef __INCLUDE_MODULE_TEMPS__
			// Get a list of smart modules for polling
			success = sub_i2c_read(fh, psuAddresses[i], 0xD3, 1, (char *) &data, 2);
			if( success ) {
				std::cout << "readThermometers - module status - " << sub_strerror(sub_errno) << std::endl;
				continue;
			}
			modules = data;
			
			// Enable writing to the PAGE address (0x00) so we can change modules
      data = ((1 << 6) & 1) << 8;
			success = sub_i2c_write(fh, psuAddresses[i], 0x10, 1, (char *) &data, 1);
			if( success ) {
				std::cout << "readThermometers - write settings - " << sub_strerror(sub_errno) << std::endl;
				continue;
			}
			
			// Loop over modules 0 through 15
			for(int j=0; j<16; j++) {
				// Skip "dumb" modules
				if( ((modules >> j) & 1) == 0 ) {
					continue;
				}
				
				// Jump to the correct page and give the PSU a second to get ready
				data = j << 8;
				success = sub_i2c_write(fh, psuAddresses[i], 0x00, 1, (char *) &data, 1);
				if( success ) {
					std::cout << "readThermometers - page change - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				
				// Verify the current page
				success = sub_i2c_read(fh, psuAddresses[i], 0x00, 1, (char *) &data, 1);
				if( success ) {
					std::cout << "readThermometers - get page - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				page = (data >> 8) & 0xFFFF;
				
				/******************
				* Get Temperature *
				******************/
				
				success = sub_i2c_read(fh, psuAddresses[i], 0x8F, 1, (char *) &data, 2);
				if( success ) {
					std::cout << "readThermometers - get temperature #3 - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				printf("0x%02X Module%02i %.2f\n", psuAddresses[i], page, 1.0*data);
			}
			
			// Set the module number back to 0
			data = 0;
			success = sub_i2c_write(fh, psuAddresses[i], 0x00, 1, (char *) &data, 1);
			if( success ) {
				std::cout << "readThermometers - page change - " << sub_strerror(sub_errno) << std::endl;
				continue;
			}
			
			// Write-protect all entries but WRITE_PROTECT (0x10)
			data = ((1 << 7) & 1) << 8;
			success = sub_i2c_write(fh, psuAddresses[i], 0x10, 1, (char *) &data, 1);
			if( success ) {
				std::cout << "readThermometers - write settings - " << sub_strerror(sub_errno) << std::endl;
				continue;
			}
		#endif
		
		/**************************
		* Get System Temperatures *
		**************************/
		success = sub_i2c_read(fh, psuAddresses[i], 0x8D, 1, (char *) &data, 2);
		if( success ) {
			std::cout << "readThermometers - get temperature #1 - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}
		std::cout << "0x" << std::hex << (int) psuAddresses[i] << std::dec << " Case " << (data/4.0) << std::endl;
		
		success = sub_i2c_read(fh, psuAddresses[i], 0x8E, 1, (char *) &data, 2);
		if( success ) {
			std::cout << "readThermometers - get temperature #2 - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}
		std::cout << "0x" << std::hex << (int) psuAddresses[i] << std::dec << " PrimarySide " << (data/4.0) << std::endl;
	}
	
	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);

	return 0;
}
