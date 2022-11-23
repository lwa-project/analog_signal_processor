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
*****************************************************/


#include <iostream>
#include <string>
#include <cstring>
#include <cmath>
#include <chrono>
#include <thread>

#include "libsub.h"
#include "aspCommon.h"

#define MODE_UNKOWN      0
#define MODE_QUERY     101
#define MODE_AUTOON    102
#define MODE_AUTOOFF   103
#define MODE_TEMPWARN  104
#define MODE_TEMPFAULT 105


int main(int argc, char** argv) {
  /*************************
  * Command line parsing   *
  *************************/
  // Make sure we have the right number of arguments to continue
  if( argc < 3+1 ) {
    std::cout << "configPSU - Need at least 3 arguments, " << argc-1 << " provided" << std::endl;
    exit(1);
  }
  
  char *endptr;
  std::string requestedSN = std::string(argv[1]);
  uint32_t i2c_device = std::strtod(argv[2], &endptr);
  std::string command = std::string(argv[3]);
  float arg_value = 0.0;
  uint16_t mode = MODE_UNKOWN;
  if( command ==  "query" ) {
		mode = MODE_QUERY;
	} else if( command == "autoOn" ) {
		mode = MODE_AUTOON;
	} else if( command == "autoOff" ) {
		mode = MODE_AUTOOFF;
	} else if( command == "tempWarn" ) {
		mode = MODE_TEMPWARN;
    if( argc < 4+1 ) {
      std::cout << "configPSU - Setting 'tempWarn' or 'tempFault' requires an additional argument" << std::endl;
      exit(1);
    }
    arg_value = std::strtod(argv[4], &endptr);
  } else if( command == "tempFault" ) {
		mode = MODE_TEMPFAULT;
    if( argc < 4+1 ) {
      std::cout << "configPSU - Setting 'tempWarn' or 'tempFault' requires an additional argument" << std::endl;
      exit(1);
    }
    arg_value = std::strtod(argv[4], &endptr);
	} else {
    std::cout << "configPSU - Invalid command '" << command << "'" << std::endl;
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
		std::cout << "configPSU - Cannot find or open SUB-20 " << requestedSN << std::endl;
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
  
  uint16_t data;
	for(int i=0; i<nPSU; i++) {
		if( psuAddresses[i] != i2c_device ) {
			continue;
		}
    
    if( mode != MODE_QUERY ) {
			// Enable writing to the OPERATION address (0x01) so we can change modules
			data = 0;
			success = sub_i2c_write(fh, psuAddresses[i], 0x10, 1, (char *) &data, 1);
			if( success ) {
				std::cout << "configPSU - write settings - " << sub_strerror(sub_errno) << std::endl;
				continue;
			}
		}
		
		// Go!
		switch(mode) {
			case MODE_QUERY:
				// Querty the PSU setup
				data = 0;
				success = sub_i2c_read(fh, psuAddresses[i], 0xD6, 1, (char *) &data, 1);
				if( success ) {
					std::cout << "configPSU - get setup - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
        
        data &= 0xFF;
				switch (data & 3) {
					case 3:
						std::cout << "Config. Data Source:       User" << std::endl;
						break;
					case 2:
						std::cout << "Config. Data Source:       Default" << std::endl;
						break;
					case 1:
						std::cout << "Config. Data Source:       Firmware" << std::endl;
						break;
					default:
						std::cout << "Config. Data Source:       Memory" << std::endl;
						break;
				}
				std::cout << "DC Used for Input:         " << (int) ((data >> 3) & 1) << std::endl;
				
				// Query the PSU configuation
				data = 0;
				success = sub_i2c_read(fh, psuAddresses[i], 0xD5, 1, (char *) &data, 1);
				if( success ) {
					std::cout << "configPSU - get configuration - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
        data &= 0xFF;
				std::cout << "Fan Alarm Disabled:        " << (int) ((data >> 0) & 1) << std::endl;
				std::cout << "Fan Off at Standby:        " << (int) ((data >> 1) & 1) << std::endl;
				std::cout << "Fan Direction Reversed:    " << (int) ((data >> 2) & 1) << std::endl;
				std::cout << "DC Output ON with Power:   " << (int) ((data >> 7) & 1) << std::endl;
				
				// Query temperature limits
				success = sub_i2c_read(fh, psuAddresses[i], 0x51, 1, (char *) &data, 2);
				if( success ) {
					std::cout << "configPSU - get temperature warning - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
			  std::cout << "Temperature Warning Limit: " << (float) data/4.0 << " C" << std::endl;
				
				success = sub_i2c_read(fh, psuAddresses[i], 0x4F, 1, (char *) &data, 2);
				if( success ) {
					std::cout << "configPSU - get temperature fault - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				std::cout << "Temperature Fault Limit:   " << (float) data/4.0 << " C" << std::endl;
				
				// Query power limits
        uint64_t wide_data;
        wide_data = 0;
				success = sub_i2c_read(fh, psuAddresses[i], 0xEB, 1, (char *) &wide_data, 5);
				if( success ) {
					std::cout << "configPSU - get power limits - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				std::cout << "Low Power Limit:           " << (int) ((wide_data >> 8) & 0xFFFF) << " W" << std::endl;
				std::cout << "High Power Limit:          " << (int) ((wide_data >> 24) & 0xFFFF) << " W" << std::endl;
				break;
				
			case MODE_AUTOOFF:
				// Query the PSU configuation
				data = 0;
				success = sub_i2c_read(fh, psuAddresses[i], 0xD5, 1, (char *) &data, 1);
				if( success ) {
					std::cout << "configPSU - get configuration - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				
				// Update the default operation flag
        data = (data & 63) & ~(1 << 7);
				
				// Write the PSU configuation
				success = sub_i2c_write(fh, psuAddresses[i], 0xD5, 1, (char *) &data, 1);
				if( success ) {
					std::cout << "configPSU - set configuration - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				
				// Save the configutation as default
			  data = 0x21;
				success = sub_i2c_write(fh, psuAddresses[i], 0x15, 1, (char *) &data, 1);
				if( success ) {
					std::cout << "configPSU - save configuration - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				break;
				
			case MODE_AUTOON:
				// Query the PSU configuation
				data = 0;
				success = sub_i2c_read(fh, psuAddresses[i], 0xD5, 1, (char *) &data, 1);
				if( success ) {
					std::cout << "configPSU - get configuration - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				
				// Update the default operation flag
				data = (data & 63) | (1 << 7);
				
				// Write the PSU configuation
				success = sub_i2c_write(fh, psuAddresses[i], 0xD5, 1, (char *) &data, 1);
				if( success ) {
					std::cout << "configPSU - set configuration - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				
				// Save the configutation as default
				data = 0x21;
				success = sub_i2c_write(fh, psuAddresses[i], 0x15, 1, (char *) &data, 1);
				if( success ) {
					std::cout << "configPSU - save configuration - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				break;
				
			case MODE_TEMPWARN:
				// Convert to the right format
				data = (uint16_t) round(arg_value*4);
				
				// Write to memory
				success = sub_i2c_write(fh, psuAddresses[i], 0x51, 1, (char *) &data, 2);
				if( success ) {
					std::cout << "configPSU - set temperature warning - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				
				// Save the configutation as default
				data = 0x21;
				success = sub_i2c_write(fh, psuAddresses[i], 0x15, 1, (char *) &data, 1);
				if( success ) {
					std::cout << "configPSU - save configuration - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				break;
				
			case MODE_TEMPFAULT:
				// Convert to the right format
				data = (uint16_t) round(arg_value*4);
				
				// Write to memory
				success = sub_i2c_write(fh, psuAddresses[i], 0x4F, 1, (char *) &data, 2);
				if( success ) {
					std::cout << "configPSU - set temperature warning - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				
				// Save the configutation as default
				data = 0x21;
				success = sub_i2c_write(fh, psuAddresses[i], 0x15, 1, (char *) &data, 1);
				if( success ) {
					std::cout << "configPSU - save configuration - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				break;
				
			default:
				break;
		}
		
    if( mode != MODE_QUERY ) {// Write-protect all entries but WRITE_PROTECT (0x10)
			data = ((1 << 7) & 1);
			success = sub_i2c_write(fh, psuAddresses[i], 0x10, 1, (char *) &data, 1);
			if( success ) {
				std::cout << "configPSU - write settings - " << sub_strerror(sub_errno) << std::endl;
				continue;
			}
		}
    
    // Mark that we have sone something
		found = true;
	}
	
	/*******************
	* Cleanup and exit *
	*******************/
	sub_close(fh);
	
  if( !found ) {
		std::cout << "configPSU - Cannot find device at address " << std::hex << "0x%" << i2c_device << std::endl;
		exit(1);
	}
	return 0;
}
