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
#include <cstdint>
#include <chrono>
#include <thread>

#include "libatmega.hpp"
#include "aspCommon.hpp"
#include "ivsCommon.hpp"

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
      std::list<uint8_t> modules = ivs_get_smart_modules(atm, addr);
      
      // Enable writing to the PAGE address (0x00) so we can change modules
      success = ivs_enable_operation_page_writes(atm, addr);
      if( !success ) {
        std::cerr << "readThermometers - write settings failed" << std::endl;
        continue;
      }
      
      // Loop over modules
      for(uint8_t& module: modules) {
        success = ivs_select_module(atm, addr, module);
        if( !success ) {
          std::cerr << "readThermometers - page change failed" << std::endl;
          continue;
        }
        
        /******************
        * Get Temperature *
        ******************/
        
        success = atm->read_i2c(addr, 0x8F, (char *) &data, 2);
        if( !success ) {
          std::cerr << "readThermometers - get temperature #3 failed" << std::endl;
          continue;
        }
        std::cout << "0x" << std::uppercase << std::hex << (int) addr << std::nouppercase << std::dec << " Module" << module << " " << (1.0*data) << std::endl;
      }
      
      // Write-protect all entries but WRITE_PROTECT (0x10)
      success = ivs_disable_writes(atm, addr);
      if( !success ) {
        std::cerr << "readThermometers - write settings failed" << std::endl;
        continue;
      }
    #endif
    
    /**************************
    * Get System Temperatures *
    **************************/
    success = atm->read_i2c(addr, 0x8D, (char *) &data, 2);
    if( !success ) {
      std::cerr << "readThermometers - get temperature #1 failed" << std::endl;
      continue;
    }
    std::cout << "0x" << std::uppercase << std::hex << (int) addr << std::nouppercase << std::dec << " Case " << (data/4.0) << std::endl;
    
    success = atm->read_i2c(addr, 0x8E, (char *) &data, 2);
    if( !success ) {
      std::cerr << "readThermometers - get temperature #2 failed" << std::endl;
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
