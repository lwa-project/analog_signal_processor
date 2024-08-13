/*****************************************************
countPICs - Utility for identifying the number of 
PIC microcontrollers connected to the RS485.  The exit code 
contains the number of boards found.
 
Usage:
  countPICs [-v|--verbose] <ATmega S/N>

Options:
  None
*****************************************************/


#include <iostream>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>

#include "libatmega.hpp"
#include "aspCommon.hpp"


int main(int argc, char** argv) {
  /*************************
	* Command line parsing   *
	*************************/
  // Make sure we have the right number of arguments to continue
	std::list<std::string> arg_str;
  bool verbose = false;
  for(int i=1; i<argc; i++) {
    std::string temp = std::string(argv[i]);
    if( temp[0] != '-' ) {
      arg_str.push_back(temp);
    } else {
      if( (temp == "-v") || (temp == "--verbose") ) {
        verbose = true;
      }
    }
  }
  if( arg_str.size() != 1 ) {
    std::cerr << "countPICs - Need 1 argument, " << arg_str.size() << " provided" << std::endl;
		std::exit(EXIT_FAILURE);
  }
  
  // Unpack
  std::string requestedSN = arg_str.front();
  arg_str.pop_front();
  
  /************************************
	* ATmega device selection and ready *
	************************************/
  ATmega *atm = new ATmega(requestedSN);
  
  bool success = atm->open();
  if( !success ) {
    std::cout << "countPICs - failed to open " << requestedSN << std::endl;
	  return 0;
  }
  
	/****************************************
	* Send the command and get the response *
	****************************************/
  // Process the commands
  std::list<std::uint8_t> addrs = atm->list_rs485_devices();
  int num = addrs.size();
  
	/*******************
	* Cleanup and exit *
	*******************/
  delete atm;
	
	// Report
	std::cout << "Found " << num << " PICs" << std::endl;
  if( verbose ) {
    std::cout << "Addresses found:" << std::endl;
    for(std::uint8_t& addr: addrs) {
      std::cout << " " << (uint32_t) addr << std::endl;
    }
  }
  
	return num;
}
