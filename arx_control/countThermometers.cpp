/*****************************************************
countThermometers - Utility for identifying the 
temperature sensors associated with each power supply.
The exit code contains the number of sensors found.

Note:  There can be more than one temperature sensor
       per power supply.
 
Usage:
  countThermometers [-h|--help] <ATmega S/N>

Options:
  -h, --help  Show this help message and exit
*****************************************************/

#include <iostream>
#include <string>
#include <cstring>
#include <cstdint>

#include "libatmega.hpp"
#include "aspCommon.hpp"
#include "ivsCommon.hpp"

static const char cmdHelp[] =
#include "countThermometers_help.h"
;


int main(int argc, char** argv) {
  /*************************
  * Command line parsing   *
  *************************/
  // Make sure we have the right number of arguments to continue
  for(int i=1; i<argc; i++) {
    std::string temp = std::string(argv[i]);
    if( temp[0] == '-' ) {
      if( (temp == "-h") || (temp == "--help") ) {
        std::cout << cmdHelp << std::endl;
        std::exit(EXIT_SUCCESS);
      }
    }
  }
  if( argc < 1+1 ) {
    std::cerr << "countThermometers - Need 1 argument, " << argc-1 << " provided" << std::endl;
    return 0;
  }
  
  std::string requestedSN = std::string(argv[1]);
  
  /************************************
  * ATmega device selection and ready *
  ************************************/
  ATmega *atm = new ATmega(requestedSN);
  
  bool success = atm->open();
  if( !success ) {
    std::cerr << "countThermometers - failed to open " << requestedSN << std::endl;
    return 0;
  }
  
  /********************
  * Read from the I2C *
  ********************/
  std::list<uint8_t> i2c_devices = atm->list_i2c_devices();
  
  int num = 0;
  for(uint8_t& addr: i2c_devices) {
    if( addr > 0x1F ) {
      continue;
    }

#ifdef __INCLUDE_MODULE_TEMPS__
      // Get a list of smart modules for polling
      std::list<uint8_t> modules = ivs_get_smart_modules(atm, addr);
      num += modules.size();
#endif
  
    // And there are two overall sensors per PSU
    num += 2;
  }
  
  std::cout << "Found " << num << " PSU thermometers" << std::endl;
  
  /*******************
  * Cleanup and exit *
  *******************/
  delete atm;

  return num;
}
