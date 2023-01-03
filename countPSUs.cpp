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
#include <list>

#include "libsub.h"
#include "aspCommon.hpp"

int main(int argc, char** argv) {
  /************************************
	* SUB-20 device selection and ready *
	************************************/
  std::list<std::string> sub20s = list_sub20s();
  
  int total = 0;
  std::string i2cSN = std::string("UNK");
  for(std::string& sn: sub20s) {
    Sub20 *sub20 = new Sub20(sn);
    
    bool success = sub20->open();
    if( !success ) {
      std::cerr << "countPSUs - failed to open " << sn << std::endl;
  	  continue;
    }
    
    std::cout << "Found SUB-20 device S/N: " << sn << std::endl;
    std::list<uint8_t> i2c_devices = sub20->list_i2c_devices();
    
    if( i2c_devices.size() > 0 ) {
      std::cout << "-> found " << i2c_devices.size() << " I2C devices:" << std::endl;
    } else {
      std::cout << "-> did not find any I2C devices" << std::endl;
    }
    
    int num = 0;
    uint16_t data;
    for(uint8_t& addr: i2c_devices) {
      if( addr > 0x1F ) {
        continue;
      }
      
      std::cout << " -> " << std::uppercase << std::hex << "0x" << (int) addr << std::nouppercase << std::dec << std::endl;
		  
			// Get a list of smart modules for polling
			success = sub20->read_i2c(addr, 0xD3, (char *) &data, 2);
			if( !success ) {
				std::cerr <<  "countPSUs - module status - " << sub_strerror(sub_errno) << std::endl;
				continue;
			}
		  
			for(int j=0; j<16; j++) {
				num += ((data >> j) & 1);
			}
		}
	
		if( num > 0 ) {
      std::cout << "-> " << num << " PSU modules" << std::endl;
      
			i2cSN = sn;
		}

		total += num;
    
		/**********
		* Cleanup *
		**********/
		delete sub20;
	}

	std::cout << "I2C devices appear to be on " << i2cSN << std::endl;
  
	return total;
}
