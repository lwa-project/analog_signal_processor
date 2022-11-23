/*****************************************************
onoffPSU - Change the overall power state for the 
specified device.

Usage:
  onoffPSU <SUB-20 S/N> <device address> <new power state>
  
  * Device addresses are two-digit hexadecimal numbers 
    (i.e. 0x1F)
  * Valid power states are 00 (off) and 11 (on)

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
  if( argc < 3+1 ) {
    std::cout << "onoffPSU - Need 3 arguments, " << argc-1 << " provided" << std::endl;
    exit(1);
  }
  
  char *endptr;
  std::string requestedSN = std::string(argv[1]);
  uint32_t i2c_device = std::strtod(argv[2], &endptr);
  uint32_t pwr_state = std::strtod(argv[3], &endptr);
	if( pwr_state != 0 && pwr_state != 11 ) {
		std::cout << "onoffPSU - Unknown state " << pwr_state << " (valid values are 00 and 11)" << std::endl;
		exit(1);
	}
  
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
		std::cout << "readPSU - Cannot find or open SUB-20 " << requestedSN << std::endl;
		exit(1);
	}
  
  /********************
	* Read from the I2C *
	********************/
  int nPSU;
  char psuAddresses[128];
  success = sub_i2c_scan(fh, &nPSU, psuAddresses);
	if( success ) {
		std::cout << "readPSU - get PSUs - " << sub_strerror(sub_errno) << std::endl;
		exit(1);
	}
  
  uint8_t data, status;
	for(int i=0; i<nPSU; i++) {
		if( psuAddresses[i] != i2c_device ) {
			continue;
		}
    
    // Get the current power supply state
		success = sub_i2c_read(fh, psuAddresses[i], 0x01, 1, (char *) &data, 1);
		if( success ) {
			std::cout << "onoffPSU - page change - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}
		status = (data >> 7) & 1;
		std::cout << std::hex << "0x" << (int) psuAddresses[i] << std::dec << " is in state " << (int) status << std::endl;
		
		// Enable writing to the OPERATION address (0x01) so we can change modules
		data = 0;
		success = sub_i2c_write(fh, psuAddresses[i], 0x10, 1, (char *) &data, 1);
		if( success ) {
			std::cout << "onoffPSU - write settings - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}

		// Find out the new state to put the power supply in
		if( pwr_state == 0 ) {
			// Turn off the power supply
			data = 0;
		} else {
			// Turn on the power supply
			data = (1 << 7);
		}
		
		// Toggle the power status and wait a bit for the changes to take affect
		success = sub_i2c_write(fh, psuAddresses[i], 0x01, 1, (char *) &data, 1);
		if( success ) {
			std::cout << "onoffPSU - on/off toggle - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		
		// Check the power supply status
		data = 0;
		success = sub_i2c_read(fh, psuAddresses[i], 0x01, 1, (char *) &data, 1);
		if( success ) {
			std::cout << "onoffPSU - page change - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}
		status = (data >> 7) & 1;
		std::cout << std::hex << "0x" << (int) psuAddresses[i] << std::dec << " is now in state " << (int) status << std::endl;
		
		// Write-protect all entries but WRITE_PROTECT (0x10)
		data = (1 << 7) & 1;
		success = sub_i2c_write(fh, psuAddresses[i], 0x10, 1, (char *) &data, 1);
		if( success ) {
			std::cout << "onoffPSU - write settings - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}
  }
  
	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);
	
	return 0;
}
