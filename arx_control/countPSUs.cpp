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

#include "libatmega.hpp"
#include "aspCommon.hpp"

int main(int argc, char** argv) {
  /************************************
	* ATmega device selection and ready *
	************************************/
  std::list<std::string> atmegas = list_atmegas();
  
  int total = 0;
  std::string i2cSN = std::string("UNK");
  for(std::string& sn: atmegas) {
    ATmega *atm = new ATmega(sn);
    
    bool success = atm->open();
    if( !success ) {
      std::cerr << "countPSUs - failed to open " << sn << std::endl;
  	  continue;
    }
    
    std::cout << "Found ATmega device S/N: ATmegasn << std::endl;
    std::list<uint8_t> i2c_devices = atm->list_i2c_devices();
    
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
			success = atm->read_i2c(addr, 0xD3, (char *) &data, 2);
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
		delete atm;
	}

	std::cout << "I2C devices appear to be on " << i2cSN << std::endl;
  
	return total;
}
