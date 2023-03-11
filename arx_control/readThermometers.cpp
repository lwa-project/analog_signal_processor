/*****************************************************
readThermometers - Program to pull information about 
temperature sensors found on the I2C bus.  The data 
polled includes temperatures from:
  * case
  * primary side input
  * modules
 
Usage:
  readThermometers <ATmega S/N>

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

int main(int argc, char** argv) {
  /*************************
  * Command line parsing   *
  *************************/
  // Make sure we have the right number of arguments to continue
  if( argc < 1+1 ) {
    std::cerr << "readThermometers - Need 1 argument, " << argc-1 << " provided" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  std::string requestedSN = std::string(argv[1]);
  
  /************************************
  * ATmega device selection and ready *
  ************************************/
  ATmega *atm = new ATmega(requestedSN);
  
  bool success = atm->open();
  if( !success ) {
    std::cerr << "readThermometers - failed to open " << requestedSN << std::endl;
		std::exit(EXIT_FAILURE);
  }
  
  /********************
	* Read from the I2C *
	********************/
  std::list<uint8_t> i2c_devices = atm->list_i2c_devices();
  
  uint16_t data;
  for(uint8_t& addr: i2c_devices) {
    if( addr > 0x1F ) {
      continue;
    }
    
		#ifdef __INCLUDE_MODULE_TEMPS__
      uint16_t modules, page;
      
			// Get a list of smart modules for polling
			success = atm->read_i2c(addr, 0xD3, (char *) &data, 2);
			if( !success ) {
				std::cerr << "readThermometers - module status - " << sub_strerror(sub_errno) << std::endl;
				continue;
			}
			modules = data;
			
			// Enable writing to the PAGE address (0x00) so we can change modules
      data = ((1 << 6) & 1) << 8;
			success = atm->write_i2c(addr, 0x10, (char *) &data, 1);
			if( !success ) {
				std::cerr << "readThermometers - write settings - " << sub_strerror(sub_errno) << std::endl;
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
				success = atm->write_i2c(addr, 0x00, (char *) &data, 1);
				if( !success ) {
					std::cerr << "readThermometers - page change - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(20));
				
				// Verify the current page
				success = atm->read_i2c(addr, 0x00, (char *) &data, 1);
				if( !success ) {
					std::cerr << "readThermometers - get page - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				page = (data >> 8) & 0xFFFF;
				
				/******************
				* Get Temperature *
				******************/
				
				success = atm->read_i2c(addr, 0x8F, (char *) &data, 2);
				if( !success ) {
					std::cerr << "readThermometers - get temperature #3 - " << sub_strerror(sub_errno) << std::endl;
					continue;
				}
				std::cout << "0x" << std::uppercase << std::hex << (int) addr << std::nouppercase << std::dec << " Module" << page << " " << (1.0*data) << std::endl;
			}
			
			// Set the module number back to 0
			data = 0;
			success = atm->write_i2c(addr, 0x00, (char *) &data, 1);
			if( !success ) {
				std::cerr << "readThermometers - page change - " << sub_strerror(sub_errno) << std::endl;
				continue;
			}
			
			// Write-protect all entries but WRITE_PROTECT (0x10)
			data = ((1 << 7) & 1) << 8;
			success = atm->write_i2c(addr, 0x10, (char *) &data, 1);
			if( !success ) {
				std::cerr << "readThermometers - write settings - " << sub_strerror(sub_errno) << std::endl;
				continue;
			}
		#endif
		
		/**************************
		* Get System Temperatures *
		**************************/
		success = atm->read_i2c(addr, 0x8D, (char *) &data, 2);
		if( !success ) {
			std::cerr << "readThermometers - get temperature #1 - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}
		std::cout << "0x" << std::uppercase << std::hex << (int) addr << std::nouppercase << std::dec << " Case " << (data/4.0) << std::endl;
		
		success = atm->read_i2c(addr, 0x8E, (char *) &data, 2);
		if( !success ) {
			std::cerr << "readThermometers - get temperature #2 - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}
		std::cout << "0x" << std::uppercase << std::hex << (int) addr << std::nouppercase << std::dec << " PrimarySide " << (data/4.0) << std::endl;
	}
	
	/*******************
	* Cleanup and exit *
	*******************/
	delete atm;

  std::exit(EXIT_SUCCESS);
}
