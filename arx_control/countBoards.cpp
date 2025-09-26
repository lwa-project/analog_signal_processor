/*****************************************************
countBoards - Utility for identifying the number of 
ARX boards connected to the SPI bus.  The exit code 
contains the number of boards found.
 
Usage:
  countBoards <ATmega S/N>

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
	if( argc < 1+1 ) {
		std::cerr << "countBoards - Need 1 argument, " << argc-1 << " provided" << std::endl;
		std::exit(EXIT_FAILURE);
	}
  
  std::string requestedSN = std::string(argv[1]);
  
  /************************************
	* ATmega device selection and ready *
	************************************/
  ATmega *atm = new ATmega(requestedSN);
  
  bool success = atm->open();
  if( !success ) {
    std::cout << "countBoards - failed to open " << requestedSN << std::endl;
	  return 0;
  }
  
	/****************************************
	* Send the command and get the response *
	****************************************/
  // Process the commands
  uint16_t *commands, *responses;
  commands = (uint16_t*) calloc(sizeof(uint16_t), 2*(MAX_BOARDS*STANDS_PER_BOARD+1));
  responses = (uint16_t*) calloc(sizeof(uint16_t), 2*(MAX_BOARDS*STANDS_PER_BOARD+1));
  
  commands[0] = SPI_COMMAND_MARKER;
  int num = 0;
  while( (responses[num] != SPI_COMMAND_MARKER) && (num < (STANDS_PER_BOARD*(MAX_BOARDS+1))) ) {
    num += STANDS_PER_BOARD;
    
    ::memset(responses, 0, sizeof(uint16_t)*2*(MAX_BOARDS*STANDS_PER_BOARD+1));
    
    success = atm->transfer_spi((char*) commands, (char*) responses, 2*num+2);
  	if( !success ) {
  		std::cerr << "coundBoards - SPI write failed" << std::endl;
  	}
  }
  if( num > STANDS_PER_BOARD*MAX_BOARDS ) {
    num = 0;
  }
  num /= STANDS_PER_BOARD;
	
	/*******************
	* Cleanup and exit *
	*******************/
  ::free(commands);
  ::free(responses);
  
	delete atm;
	
	// Report
	std::cout << "Found " << num << " boards (" << (num*STANDS_PER_BOARD) << " stands)" << std::endl;
  
	return num;
}
