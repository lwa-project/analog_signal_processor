/*****************************************************
onoffPSU - Change the overall power state for the 
specified device.

Usage:
  onoffPSU <ATmega S/N> <device address> <new power state>
  
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
#include "aspCommon.hpp"


int main(int argc, char** argv) {
  /*************************
  * Command line parsing   *
  *************************/
  // Make sure we have the right number of arguments to continue
  if( argc < 3+1 ) {
    std::cerr << "onoffPSU - Need 3 arguments, " << argc-1 << " provided" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  char *endptr;
  std::string requestedSN = std::string(argv[1]);
  uint32_t i2c_device = std::strtod(argv[2], &endptr);
  uint32_t pwr_state = std::strtod(argv[3], &endptr);
	if( pwr_state != 0 && pwr_state != 11 ) {
		std::cerr << "onoffPSU - Unknown state " << pwr_state << " (valid values are 00 and 11)" << std::endl;
		std::exit(EXIT_FAILURE);
	}
  
  /************************************
  * ATmega device selection and ready *
  ************************************/
  ATmega *atm = new ATmega(requestedSN);
  
  bool success = atm->open();
  if( !success ) {
    std::cerr << "onoffPSU - failed to open " << requestedSN << std::endl;
		std::exit(EXIT_FAILURE);
  }
  
  /********************
	* Read from the I2C *
	********************/
  std::list<uint8_t> i2c_devices = atm->list_i2c_devices();
  
  uint8_t data, status;
  bool found = false;
  for(uint8_t& addr: i2c_devices) {
    if( addr != i2c_device ) {
      continue;
    }
    
    // Get the current power supply state
		success = atm->read_i2c(addr, 0x01, (char *) &data, 1);
		if( !success ) {
			std::cerr << "onoffPSU - page change - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}
		status = (data >> 7) & 1;
		std::cout << std::uppercase << std::hex << "0x" << (int) addr << std::nouppercase << std::dec << " is in state " << (int) status << std::endl;
		
		// Enable writing to the OPERATION address (0x01) so we can change modules
		data = 0;
		success = atm->write_i2c(addr, 0x10, (char *) &data, 1);
		if( !success ) {
			std::cerr << "onoffPSU - write settings - " << sub_strerror(sub_errno) << std::endl;
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
		success = atm->write_i2c(addr, 0x01, (char *) &data, 1);
		if( !success ) {
			std::cerr << "onoffPSU - on/off toggle - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		
		// Check the power supply status
		data = 0;
		success = atm->read_i2c(addr, 0x01, (char *) &data, 1);
		if( !success ) {
			std::cerr << "onoffPSU - page change - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}
		status = (data >> 7) & 1;
		std::cout << std::uppercase << std::hex << "0x" << (int) addr << std::nouppercase << std::dec << " is now in state " << (int) status << std::endl;
		
		// Write-protect all entries but WRITE_PROTECT (0x10)
		data = (1 << 7) & 1;
		success = atm->write_i2c(addr, 0x10, (char *) &data, 1);
		if( !success ) {
			std::cerr << "onoffPSU - write settings - " << sub_strerror(sub_errno) << std::endl;
			continue;
		}
    
    // Mark that we have sone something
		found = true;
  }
  
	/*******************
	* Cleanup and exit *
	*******************/
	delete atm;
	
  if( !found ) {
		std::cerr << "onoffPSU - Cannot find device at address " << std::uppercase << std::hex << "0x%" << i2c_device << std::endl;
		std::exit(EXIT_FAILURE);
	}
  
	std::exit(EXIT_SUCCESS);
}
