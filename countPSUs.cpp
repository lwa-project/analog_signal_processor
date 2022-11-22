/*****************************************************
countPSUs - Utility for identifying the power supply
modules connected to the I2C bus. The exit code 
contains the number of modules found.

Note:  There can be more than one module per power
       supply chassis.
 
Usage:
  countPSUs

Options:
  None
*****************************************************/

#include <iostream>
#include <string>
#include <cstring>

#include "libsub.h"
#include "aspCommon.h"

int main(int argc, char** argv) {
  /************************************
	* SUB-20 device selection and ready *
	************************************/
  sub_device dev = NULL;
  sub_handle fh = NULL;
	
	// Find the right SUB-20
	char foundSN[20];
	int success, nPSU, total = 0;
  char psuAddresses[128];
  std::string i2cSN = std::string("UNK");
	while( (dev = sub_find_devices(dev)) ) {
		// Open the USB device (or die trying)
		fh = sub_open(dev);
		if( fh == NULL ) {
			continue;
		}
		
		success = sub_get_serial_number(fh, foundSN, sizeof(foundSN));
		if( !success ) {
			continue;
		}
		
	  std::cout << "Found SUB-20 device S/N: " << foundSN << std::endl;
    
    success = sub_i2c_scan(fh, &nPSU, psuAddresses);
    if( success ) {
      std::cout <<  "countPSUs - get PSUs - " << sub_strerror(sub_errno) << std::endl;
			exit(1);
    }
    
    if( nPSU == 0 ) {
      std::cout << "-> did not find any I2C devices" << std::endl;
      sub_close(fh);
      continue;
    }
    
    std::cout << "-> found " << nPSU << " I2C devices:" << std::endl;
		for(int i=0; i<nPSU; i++) {
      std::cout << " -> " << std::hex << "0x" << (int) psuAddresses[i] << std::dec << std::endl;
		}
    
    int num = 0;
    uint16_t data;
		for(int i=0; i<nPSU; i++) {
			if( psuAddresses[i] > 0x1F ) {
				continue;
			}
		
			// Get a list of smart modules for polling
			success = sub_i2c_read(fh, psuAddresses[i], 0xD3, 1, (char *) &data, 2);
			if( success ) {
				std::cout <<  "countPSUs - module status - " << sub_strerror(sub_errno) << std::endl;
				continue;
			}
		  
			for(int j=0; j<16; j++) {
				num += ((data >> j) & 1);
			}
		}
	
		std::cout << "-> " << num << " PSU modules" << std::endl;
		if( num > 0 ) {
			i2cSN = std::string(foundSN);
		}

		total += num;
    
		/*******************
		* Cleanup and exit *
		*******************/
		sub_close(fh);
	}

	std::cout << "I2C devices appear to be on " << i2cSN << std::endl;
	return total;
}
