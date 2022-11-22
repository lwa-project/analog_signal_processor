/*****************************************************
countThermometers - Utility for identifying the 
temperature sensors associated with each power supply.
The exit code contains the number of sensors found.

Note:  There can be more than one temperature sensor
       per power supply.
 
Usage:
  countThermometers

Options:
  None
*****************************************************/

#include <iostream>
#include <string>
#include <cstring>

#include "libsub.h"
#include "aspCommon.h"

int main(int argc, char** argv) {
  /*************************
  * Command line parsing   *
  *************************/
  // Make sure we have the right number of arguments to continue
  if( argc < 1+1 ) {
    std::cout << "countThermometers - Need 1 argument, %i provided, " << argc-1 << " provided" << std::endl;
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
		std::cout << "countThermometers - Cannot find or open SUB-20 " << requestedSN << std::endl;
		exit(1);
	}
  
  /********************
	* Read from the I2C *
	********************/
  int num, nPSU;
  char psuAddresses[128];
  success = sub_i2c_scan(fh, &nPSU, psuAddresses);
	if( success ) {
		std::cout << "countThermometers - get PSUs - " << sub_strerror(sub_errno) << std::endl;
		exit(1);
	}

	num = 0;
	for(int i=0; i<nPSU; i++) {
		if( psuAddresses[i] > 0x1F ) {
			continue;
		}

#ifdef __INCLUDE_MODULE_TEMPS__
			// Get a list of smart modules for polling
      uint16_t data;
			success = sub_i2c_read(fh, psuAddresses[i], 0xD3, 1, (char *) &data, 2);
			if( success ) {
				std::cout << "countThermometers - module status - " << sub_strerror(sub_errno) << std::endl;
				continue;
			}
			
			// Each module has a temperature sensor
      for(int j=0; j<16; j++) {
				num += ((data >> j) & 1);
			}
#endif
  
		// And there are two overall sensors per PSU
		num += 2;
	}
	
	std::cout << "Found " << num << " PSU thermometers" << std::endl;
	
	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);

	return num;
}
