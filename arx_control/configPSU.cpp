/*****************************************************
configPSU - Change configuration of the specified PSU

Usage:
  configPSU <ATmega S/N> <device address> <command>
  
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
#include <cstdint>
#include <cmath>
#include <chrono>
#include <thread>

#include "libatmega.hpp"
#include "aspCommon.hpp"

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
    std::cerr << "configPSU - Need at least 3 arguments, " << argc-1 << " provided" << std::endl;
    std::exit(EXIT_FAILURE);
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
      std::cerr << "configPSU - Setting 'tempWarn' requires an additional argument" << std::endl;
      std::exit(EXIT_FAILURE);
    }
    arg_value = std::strtod(argv[4], &endptr);
  } else if( command == "tempFault" ) {
		mode = MODE_TEMPFAULT;
    if( argc < 4+1 ) {
      std::cerr << "configPSU - Setting 'tempFault' requires an additional argument" << std::endl;
      std::exit(EXIT_FAILURE);
    }
    arg_value = std::strtod(argv[4], &endptr);
	} else {
    std::cerr << "configPSU - Invalid command '" << command << "'" << std::endl;
		std::exit(EXIT_FAILURE);
	}
  
  /************************************
  * ATmega device selection and ready *
  ************************************/
  ATmega *atm = new ATmega(requestedSN);
  
  bool success = atm->open();
  if( !success ) {
    std::cerr << "configPSU - failed to open " << requestedSN << std::endl;
		std::exit(EXIT_FAILURE);
  }
  
  /********************
	* Read from the I2C *
	********************/
  std::list<uint8_t> i2c_devices = atm->list_i2c_devices();
  
  uint16_t data;
  bool found = false;
  for(uint8_t& addr: i2c_devices) {
    if( addr != i2c_device ) {
      continue;
    }
    
    if( mode != MODE_QUERY ) {
			// Enable writing to the OPERATION address (0x01) so we can change modules
			data = 0;
			success = atm->write_i2c(addr, 0x10, (char *) &data, 1);
			if( !success ) {
				std::cerr << "configPSU - write settings failed" << std::endl;
				continue;
			}
		}
		
		// Go!
		switch(mode) {
			case MODE_QUERY:
				// Querty the PSU setup
				data = 0;
				success = atm->read_i2c(addr, 0xD6, (char *) &data, 1);
				if( !success ) {
					std::cerr << "configPSU - get setup failed" << std::endl;
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
				success = atm->read_i2c(addr, 0xD5, (char *) &data, 1);
				if( !success ) {
					std::cerr << "configPSU - get configuration failed" << std::endl;
					continue;
				}
        data &= 0xFF;
				std::cout << "Fan Alarm Disabled:        " << (int) ((data >> 0) & 1) << std::endl;
				std::cout << "Fan Off at Standby:        " << (int) ((data >> 1) & 1) << std::endl;
				std::cout << "Fan Direction Reversed:    " << (int) ((data >> 2) & 1) << std::endl;
				std::cout << "DC Output ON with Power:   " << (int) ((data >> 7) & 1) << std::endl;
				
				// Query temperature limits
				success = atm->read_i2c(addr, 0x51, (char *) &data, 2);
				if( !success ) {
					std::cerr << "configPSU - get temperature warning failed" << std::endl;
					continue;
				}
			  std::cout << "Temperature Warning Limit: " << (float) data/4.0 << " C" << std::endl;
				
				success = atm->read_i2c(addr, 0x4F, (char *) &data, 2);
				if( !success ) {
					std::cerr << "configPSU - get temperature fault failed" << std::endl;
					continue;
				}
				std::cout << "Temperature Fault Limit:   " << (float) data/4.0 << " C" << std::endl;
				
				// Query power limits
        uint64_t wide_data;
        wide_data = 0;
				success = atm->read_i2c(addr, 0xEB, (char *) &wide_data, 5);
				if( !success ) {
					std::cerr << "configPSU - get power limits failed" << std::endl;
					continue;
				}
				std::cout << "Low Power Limit:           " << (int) ((wide_data >> 8) & 0xFFFF) << " W" << std::endl;
				std::cout << "High Power Limit:          " << (int) ((wide_data >> 24) & 0xFFFF) << " W" << std::endl;
				break;
				
			case MODE_AUTOOFF:
				// Query the PSU configuation
				data = 0;
				success = atm->read_i2c(addr, 0xD5, (char *) &data, 1);
				if( !success ) {
					std::cerr << "configPSU - get configuration failed" << std::endl;
					continue;
				}
				
				// Update the default operation flag
        data = (data & 63) & ~(1 << 7);
				
				// Write the PSU configuation
				success = atm->write_i2c(addr, 0xD5, (char *) &data, 1);
				if( !success ) {
					std::cerr << "configPSU - set configuration failed" << std::endl;
					continue;
				}
				
				// Save the configutation as default
			  data = 0x21;
				success = atm->write_i2c(addr, 0x15, (char *) &data, 1);
				if( !success ) {
					std::cerr << "configPSU - save configuration failed" << std::endl;
					continue;
				}
				break;
				
			case MODE_AUTOON:
				// Query the PSU configuation
				data = 0;
				success = atm->read_i2c(addr, 0xD5, (char *) &data, 1);
				if( !success ) {
					std::cerr << "configPSU - get configuration failed" << std::endl;
					continue;
				}
				
				// Update the default operation flag
				data = (data & 63) | (1 << 7);
				
				// Write the PSU configuation
				success = atm->write_i2c(addr, 0xD5, (char *) &data, 1);
				if( !success ) {
					std::cerr << "configPSU - set configuration failed" << std::endl;
					continue;
				}
				
				// Save the configutation as default
				data = 0x21;
				success = atm->write_i2c(addr, 0x15, (char *) &data, 1);
				if( !success ) {
					std::cerr << "configPSU - save configuration failed" << std::endl;
					continue;
				}
				break;
				
			case MODE_TEMPWARN:
				// Convert to the right format
				data = (uint16_t) round(arg_value*4);
				
				// Write to memory
				success = atm->write_i2c(addr, 0x51, (char *) &data, 2);
				if( !success ) {
					std::cerr << "configPSU - set temperature warning failed" << std::endl;
					continue;
				}
				
				// Save the configutation as default
				data = 0x21;
				success = atm->write_i2c(addr, 0x15, (char *) &data, 1);
				if( !success ) {
					std::cerr << "configPSU - save configuration failed" << std::endl;
					continue;
				}
				break;
				
			case MODE_TEMPFAULT:
				// Convert to the right format
				data = (uint16_t) round(arg_value*4);
				
				// Write to memory
				success = atm->write_i2c(addr, 0x4F, (char *) &data, 2);
				if( !success ) {
					std::cerr << "configPSU - set temperature warning failed" << std::endl;
					continue;
				}
				
				// Save the configutation as default
				data = 0x21;
				success = atm->write_i2c(addr, 0x15, (char *) &data, 1);
				if( !success ) {
					std::cerr << "configPSU - save configuration failed" << std::endl;
					continue;
				}
				break;
				
			default:
				break;
		}
		
    if( mode != MODE_QUERY ) {// Write-protect all entries but WRITE_PROTECT (0x10)
			data = ((1 << 7) & 1);
			success = atm->write_i2c(addr, 0x10, (char *) &data, 1);
			if( !success ) {
				std::cerr << "configPSU - write settings failed" << std::endl;
				continue;
			}
		}
    
    // Mark that we have sone something
		found = true;
	}
	
	/*******************
	* Cleanup and exit *
	*******************/
	delete atm;
	
  if( !found ) {
		std::cerr << "configPSU - Cannot find device at address " << std::uppercase << std::hex << "0x" << i2c_device << std::endl;
		std::exit(EXIT_FAILURE);
	}
  
	std::exit(EXIT_SUCCESS);
}
