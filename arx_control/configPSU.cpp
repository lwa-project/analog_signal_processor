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
     voltAdjust ##.# - Set the output voltage to the
                       specified value in volts
     turnOnDelay ### - Set the turn on delays to the
                       specifed value in ms

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
#include "ivsCommon.hpp"

#define MODE_UNKOWN       0
#define MODE_QUERY      101
#define MODE_AUTOON     102
#define MODE_AUTOOFF    103
#define MODE_TEMPWARN   104
#define MODE_TEMPFAULT  105
#define MODE_VOLTADJUST 106
#define MODE_ONDELAY    107


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
  } else if( command == "voltAdjust" ) {
		mode = MODE_VOLTADJUST;
    if( argc < 4+1 ) {
      std::cerr << "configPSU - Setting 'voltAdjust' requires an additional argument" << std::endl;
      std::exit(EXIT_FAILURE);
    }
    arg_value = std::strtod(argv[4], &endptr);
  } else if( command == "turnOnDelay" ) {
		mode = MODE_ONDELAY;
    if( argc < 4+1 ) {
      std::cerr << "configPSU - Setting 'turnOnDelay' requires an additional argument" << std::endl;
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
    
    std::list<uint8_t> modules;
    if( (mode == MODE_QUERY) || (mode == MODE_VOLTADJUST) || (mode == MODE_ONDELAY) ) {
      // Get a list of smart modules tha we need to update
      modules = ivs_get_smart_modules(atm, addr);
    }
    
    if( mode != MODE_QUERY ) {
			// Enable writing to the OPERATION address (0x01) so we can change modules
			success = ivs_enable_all_writes(atm, addr);
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
        
        // Query turn on delay
        for(uint8_t& module: modules) {
          success = ivs_select_module(atm, addr, module);
          if( !success ) {
            std::cerr << "configPSU - page change failed" << std::endl;
            continue;
          }
          
          success = atm->read_i2c(addr, 0x60, (char *) &data, 2);
          if( !success ) {
  					std::cerr << "configPSU - get turn on delay failed" << std::endl;
  					continue;
  				}
          data &= 0xFF;
          std::cout << "Module " << module << " Turn On Delay: " << (int) data << " ms" << std::endl;
        }
        
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
				
      case MODE_VOLTADJUST:
      case MODE_ONDELAY:
        // Loop over modules
        for(uint8_t& module: modules) {
          success = ivs_select_module(atm, addr, module);
          if( !success ) {
            std::cerr << "configPSU - page change failed" << std::endl;
            continue;
          }
          
          if( mode == MODE_VOLTADJUST ) {
            // Read what this module is capable of
            data = 0;
            success = atm->write_i2c(addr, 0xDE, (char *) &longdata, 1);
            if( !success ) {
              std::cerr << "configPSU - get module info failed" << std::endl;
              continue;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            uint32_t longdata = 0;
            success = atm->read_i2c(addr, 0xDF, (char *) &longdata, 4);
            if( !success ) {
              std::cerr << "configPSU - get module info failed" << std::endl;
              continue;
            }
            
            int modulevolts = 0;  // The only valid options are the 8V and 15V modules
            if( ((longdata >> 24) & 15) == 1 ) {// 6V to 12V
              modulevolts = 8;
            } else if( ((longdata >> 24) & 15) == 2 ) {// 14V to 20V
              modulevolts = 15;
            } else if( ((longdata >> 24) & 15) == 7 ) {// 12V to 15V
              modulevolts = 15;
            }
            
            // Verify module compatibility
            if( (arg_value < (0.9*modulevolts)) || (arg_value > (1.1*modulevolts)) ) {
              std::cerr << "configPSU - requested voltage outside module range, skipping" << std::endl;
              continue;
            }
            
            // Convert to the right format
            data = (uint16_t) round(arg_value*100);
            
            // Update the output voltage
            success = atm->write_i2c(addr, 0x21, (char *) &data, 2);
            if( !success ) {
        			std::cerr << "configPSU - output voltage update failed" << std::endl;
        			continue;
        		}
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            // Verify the module status
            success = atm->read_i2c(addr, 0x78, (char *) &data, 1);
            if( !success ) {
        			std::cerr << "configPSU - get output voltage failed" << std::endl;
        			continue;
        		}
            if( (data & 191) != 0 ) {
              std::cerr << "configPSU - module in unexpected state:" << std::endl;
              if( (data >> 7) & 1 ) {// busy
                std::cerr << "            busy" << std::endl;
              }
              if( (data >> 5) & 1 ) {// overvoltage
                std::cerr << "            output overvoltage" << std::endl;
              }
              if( (data >> 4) & 1 ) {// overcurrent
                std::cerr << "            output overcurrent" << std::endl;
              }
              if( (data >> 3) & 1 ) {// undervoltage
                std::cerr << "            input undervoltage" << std::endl;
              }
              if( (data >> 2) & 1 ) {// temperature
                std::cerr << "            temperature" << std::endl;
              }
              if( (data >> 1) & 1 ) {// comms, memory, or logic
                std::cerr << "            comm/mem/logic" << std::endl;
              }
              if( (data >> 0) & 1 ) {// other
                std::cerr << "            other" << std::endl;
              }
            }
            
          } else if( mode == MODE_ONDELAY) {
            if( (arg_value < 0) || (arg_value > 255) ) {
              std::cerr << "configPSU - requested turn on delay outside range, skipping" << std::endl;
              continue;
            }
            
            // Convert to the right format
            data = (uint16_t) round(arg_value);
            
            // Update the turn on delay
            success = atm->write_i2c(addr, 0x60, (char *) &data, 2);
            if( !success ) {
        			std::cerr << "configPSU - turn on delay update failed" << std::endl;
        			continue;
        		}
          }
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
    
    if( mode == MODE_VOLTADJUST ) {// Clear faults after changing the voltage
      data = 0;
      success = atm->write_i2c(addr, 0x03, (char *) &data, 1);
      if( !success ) {
				std::cerr << "configPSU - clear faults failed" << std::endl;
				continue;
			}
    }
		
    if( mode != MODE_QUERY ) {// Write-protect all entries but WRITE_PROTECT (0x10)
			success = ivs_disable_writes(atm, addr);
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
