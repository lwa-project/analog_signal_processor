/*****************************************************
locateATmega - Send a locate command to the ATmega
device with the specified serial number so that it can
be physically identified.
 
Usage:
  locateATmega [-h|--help] <ATmega S/N>

Options:
  -h, --help  Show this help message and exit
*****************************************************/

#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>

#include "libatmega.hpp"
#include "aspCommon.hpp"

static const char cmdHelp[] =
#include "locateATmega_help.h"
;


int main(int argc, char* argv[]) {
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
    std::cerr << "locateATmega - Need at least 1 argument, " << argc-1 << " provided" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  std::string requestedSN = std::string(argv[1]);
  
  /************************************
  * ATmega device selection and ready *
  ************************************/
  ATmega *atm = new ATmega(requestedSN);
  
  bool success = atm->open();
  if( !success ) {
    std::cerr << "locateATmega - failed to open " << requestedSN << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  /*********
  * Locate *
  *********/
  success = atm->locate();
  if( !success ) {
    std::cerr << "locateATmega - locate failed" << std::endl;
    delete atm;
    std::exit(EXIT_FAILURE);
  }
  
  /*******************
  * Cleanup and exit *
  *******************/
  delete atm;
  
  std::exit(EXIT_SUCCESS);
}
