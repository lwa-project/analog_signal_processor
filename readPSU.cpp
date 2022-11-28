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

#include <iostream>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>

#include "libsub.h"
#include "aspCommon.hpp"

std::string getModuleName(uint16_t page, uint8_t moduleCode) {
	// Decode the power rating of the current module
  std::string output = std::string("Module")+std::to_string(page);
  switch((moduleCode >> 4 ) & 0xF) {
    case 0: output = output+std::string("_210W");  break;
    case 1: output = output+std::string("_360W");  break;
    case 2: output = output+std::string("_144W");  break;
    case 3: output = output+std::string("_600W");  break;
    case 4: output = output+std::string("_750W");  break;
    case 5: output = output+std::string("_1500W"); break;
    default: output = output+std::string("_UNK");
	}
  	
	// Decode the voltage range of the current module
  switch(moduleCode & 0xF) {
    case  0: output = output+std::string("_2to5.5V"); break;
    case  1: output = output+std::string("_6to12V");  break;
    case  2: output = output+std::string("_14to20V"); break;
    case  3: output = output+std::string("_24to36V"); break;
    case  4: output = output+std::string("_42to60V"); break;
    case  5: output = output+std::string("_fixed5V"); break;
    case  6: output = output+std::string("_2to6V");   break;
    case  7: output = output+std::string("_12to15V"); break;
    case  8: output = output+std::string("_24to28V"); break;
    case  9: output = output+std::string("_24to30V"); break;
    case 10: output = output+std::string("_33to60V"); break;
    default: output = std::string("UNK");
  }
	
  return output;
}


std::string getModulePower(uint8_t statusCode) {
	// Decode the output enabled flag
  std::string output;
	if( statusCode & 1 ) {
		output = std::string("ON");
	} else {
		output = std::string("OFF");
	}
  
  return output;
}


std::string getModuleStatus(uint8_t statusCode) {
	// Decode the various status and fault flags
  std::string output;
	if( (statusCode >> 1) & 1 ) {
    output = std::string("UnderVolt");
	}
	if( (statusCode >> 2) & 1 ) {
    if( output.size() > 0 ) {
      output = output+std::string("&");
    }
    output = output+std::string("OK");
	}
	if( (statusCode >> 3) & 1 ) {
    if( output.size() > 0 ) {
      output = output+std::string("&");
    }
    output = output+std::string("OverCurrent");
	}
	if( (statusCode >> 4) & 1 ) {
    if( output.size() > 0 ) {
      output = output+std::string("&");
    }
    output = output+std::string("OverTemperature");
	}
	if( (statusCode >> 5) & 1 ) {
    if( output.size() > 0 ) {
      output = output+std::string("&");
    }
    output = output+std::string("WarningTemperature");
	}
	if( (statusCode >> 6) & 1 ) {
    if( output.size() > 0 ) {
      output = output+std::string("&");
    }
    output = output+std::string("OverVolt");
	}
	if( (statusCode >> 7) & 1 ) {
    if( output.size() > 0 ) {
      output = output+std::string("&");
    }
    output = output+std::string("ModuleFault");
	}
	
	// Make sure we return something...
	if( output.size() == 0 ) {
		output = std::string("UNK");
	}
  
  return output;
}


int main(int argc, char** argv) {
  /*************************
  * Command line parsing   *
  *************************/
  // Make sure we have the right number of arguments to continue
  if( argc < 2+1 ) {
    std::cout << "readPSU - Need 2 arguments, " << argc-1 << " provided" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  char *endptr;
  std::string requestedSN = std::string(argv[1]);
  uint32_t i2c_device = std::strtod(argv[2], &endptr);
  
  /************************************
  * SUB-20 device selection and ready *
  ************************************/
  Sub20 *sub20 = new Sub20(requestedSN);
  
  bool success = sub20->open();
  if( !success ) {
    std::cout << "readPSU - failed to open " << requestedSN << std::endl;
		std::exit(EXIT_FAILURE);
  }
  
  /********************
	* Read from the I2C *
	********************/
  std::list<uint8_t> i2c_devices = sub20->list_i2c_devices();
  
  uint16_t data, page, modules;
  bool found = false;
  for(auto addr=std::begin(i2c_devices); addr!=std::end(i2c_devices); addr++) {
    if( *addr > 0x1F ) {
      continue;
    }
    
		// Get a list of smart modules for polling
		success = sub20->read_i2c(*addr, 0xD3, (char *) &data, 2);
		if( !success ) {
			std::cout << "readPSU - module status - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}
		modules = data;
    
		// Enable writing to all of the supported command so we can change 
		// modules/poll module type
		data = 0;
		success = sub20->write_i2c(*addr, 0x10, (char *) &data, 1);
		if( !success ) {
			std::cout << "readPSU - write settings - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}

		// Loop over modules 0 through 15
		int nMod = 0;
    std::string moduleName, modulePower, moduleStatus;
    float voltage = 0.0, current = 0.0;
		for(int j=0; j<16; j++) {
			/****************
			* Module Change *
			****************/
			
			// Skip "dumb" modules
			if( ((modules >> j) & 1) == 0 ) {
				continue;
			}
			nMod += 1;
			
			page = 17;
			while( page != j ) {
				// Jump to the correct page and give the PSU a second to get ready
				data = j;
				success = sub20->write_i2c(*addr, 0x00, (char *) &data, 1);
				if( !success ) {
					std::cout << "readPSU - page change - " << sub_strerror(sub_errno) << std::endl;
				  delete sub20;
          std::exit(EXIT_FAILURE);
				}
				
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				
				// Verify the current page
				success = sub20->read_i2c(*addr, 0x00, (char *) &data, 1);
				if( !success ) {
					std::cout << "readPSU - get page - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				page = data & 0xFF;
			}

			/***********************
			* Module Name and Type *
			***********************/
			
			#ifdef __DECODE_MODULE_TYPE__
        uint32_t wide_data;
        uint8_t code;
				data = 0;
				success = sub20->write_i2c(*addr, 0xDE, (char *) &data, 1);
				if( !success ) {
					std::cout << "readPSU - get type - " << sub_strerror(sub_errno) << std::endl;
					delete sub20;
          std::exit(EXIT_FAILURE);
				}
				
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				
				success = sub20->read_i2c(*addr, 0xDF, (char *) &wide_data, 4);
				if( !success ) {
					std::cout << "readPSU - get type - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				code = (uint8_t) (wide_data & 0xFF);
        if( moduleName.size() > 0 ) {
          moduleName = moduleName+std::string("|");
        }
				moduleName = moduleName+getModuleName(page, code);
			#else
				if( moduleName.size() > 0 ) {
					moduleName = moduleName+std::string("|");
				}
        moduleName = moduleName+std::string("Module")+std::to_string(page);
			#endif

			/*************************
			* Power State and Status *
			*************************/
			
      data = 0;
			success = sub20->read_i2c(*addr, 0xDB, (char *) &data, 1);
			if( !success ) {
				std::cout << "readPSU - get status - " << sub_strerror(sub_errno) << std::endl;
				continue;
			}
      data &= 0xFF;
			modulePower = getModulePower((uint8_t) data);
      if( moduleStatus.size() > 0 ) {
        moduleStatus = moduleStatus+std::string("|");
      }
			moduleStatus = moduleStatus+getModuleStatus((uint8_t) data);

			/*****************
			* Output Voltage *
			*****************/
			
			success = sub20->read_i2c(*addr, 0x8B, (char *) &data, 2);
			if( !success ) {
				std::cout << "readPSU - get output voltage - " << sub_strerror(sub_errno) << std::endl;
				continue;
			}
			voltage += (float) data /100.0;

			/*****************
			* Output Current *
			*****************/
			
			#ifdef __USE_INPUT_CURRENT__
				success = sub20->read_i2c(*addr, 0x89, (char *) &data, 2);
				if( !success ) {
					std::cout << "readPSU - get input current - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				current = (float) data /100.0 * 0.95;		// Removes the ~5% power conversion loss
			#else
				success = sub20->read_i2c(*addr, 0x8C, (char *) &data, 2);
				if( !success ) {
					std::cout << "readPSU - get output current - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				current += (float) data /100.0;
			#endif
		}
		
		// Print mean output voltage and the total current
		if( nMod != 0 ) {
			voltage /= (float) nMod;
		}
    std::cout << std::hex << "0x" << (int) *addr << std::dec 
              << " " << moduleName << " " << modulePower << " " << moduleStatus
              << " " << voltage << " " << current << std::endl;
		
		// Set the module number back to 0
		data = 0;
		success = sub20->write_i2c(*addr, 0x00, (char *) &data, 1);
		if( !success ) {
			std::cout << "readPSU - page change - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}

		// Write-protect all entries but WRITE_PROTECT (0x10)
		data = (1 << 7) & 1;
		success = sub20->write_i2c(*addr, 0x10, (char *) &data, 1);
		if( !success ) {
			std::cout << "readPSU - write settings - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}
    
    // Mark that we have sone something
		found = true;
	}
	
	/*******************
	* Cleanup and exit *
	*******************/
	delete sub20;
	
  if( !found ) {
		std::cout << "readPSU - Cannot find device at address " << std::hex << "0x%" << i2c_device << std::endl;
		std::exit(EXIT_FAILURE);
	}
  
	std::exit(EXIT_SUCCESS);
}
