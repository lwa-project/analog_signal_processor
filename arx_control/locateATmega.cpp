/*****************************************************
listATmegaSN - List the internal serial numbers of all
ATmega devices.
 
Usage:
  listATmegaSN <device name>

Options:
  None
*****************************************************/

#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>

#include "libatmega.hpp"
#include "aspCommon.hpp"


int main(int argc, char* argv[]) {
  /*************************
	* Command line parsing   *
	*************************/
  // Make sure we have the right number of arguments to continue
	if( argc < 1+1 ) {
		std::cerr << "locateATmega - Need at least 1 arguments, " << argc-1 << " provided" << std::endl;
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
