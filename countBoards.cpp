/*****************************************************
countBoards - Utility for identifying the number of 
ARX boards connected to the SPI bus.  The exit code 
contains the number of boards found.
 
Usage:
  countBoards <SUB-20 S/N>

Options:
  None
*****************************************************/


#include <iostream>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>

#include "libsub.h"
#include "aspCommon.h"


int main(int argc, char** argv) {
  /*************************
	* Command line parsing   *
	*************************/
  // Make sure we have the right number of arguments to continue
	if( argc < 1+1 ) {
		std::cout << "countBoards - Need 1 argument, %i provided, " << argc-1 << " provided" << std::endl;
		exit(1);
	}
  
  std::string requestedSN = std::string(argv[1]);
  
  /************************************
	* SUB-20 device selection and ready *
	************************************/
  sub_device dev = NULL;
  sub_handle fh = NULL;
	
	// Find the right SUB-20
	bool found = false;
  char foundSN[20];
	int success, openTries = 0;
	while( (!found) && (dev = sub_find_devices(dev)) ) {
		// Open the USB device (or die trying)
		fh = sub_open(dev);
		while( (fh == NULL) && (openTries < SUB20_OPEN_MAX_ATTEMPTS) ) {
			openTries++;
			std::this_thread::sleep_for(std::chrono::milliseconds(SUB20_OPEN_WAIT_US/1000));
			
			fh = sub_open(dev);
		}
		if( fh == NULL ) {
			continue;
		}
		
		success = sub_get_serial_number(fh, foundSN, sizeof(foundSN));
		if( !success ) {
			continue;
		}
		
		if( !strcmp(foundSN, requestedSN.c_str()) ) {
			std::cout << "Found SUB-20 device S/N: " << foundSN << std::endl;
			found = true;
		} else {
			sub_close(fh);
		}
	}
	
	// Make sure we actually have a SUB-20 device
	if( !found ) {
		std::cout << "coundBoards - Cannot find or open SUB-20 " << requestedSN << std::endl;
		exit(1);
	}
  
		
	/****************************************
	* Send the command and get the response *
	****************************************/
  // Enable the SPI bus operations on the SUB-20 board
	int j = 0;
	success = sub_spi_config(fh, 0, &j);
	if( success ) {
		std::cout << "coundBoards - get config - " << sub_strerror(sub_errno) << std::endl;
	}
	
	success = 1;
	while( success ) {
		success = sub_spi_config(fh, ARX_SPI_CONFIG, NULL);
		if( success ) {
			std::cout << "coundBoards - set config - " << sub_strerror(sub_errno) << std::endl;
			exit(1);
		}
	}
	
  // Process the commands
  uint16_t *commands, *responses;
  commands = (uint16_t*) calloc(sizeof(uint16_t), 2*(MAX_BOARDS*STANDS_PER_BOARD+1));
  responses = (uint16_t*) calloc(sizeof(uint16_t), 2*(MAX_BOARDS*STANDS_PER_BOARD+1));
  
  commands[0] = SPI_COMMAND_MARKER;
  int num = 0;
  while( (responses[num] != SPI_COMMAND_MARKER) && (num < (STANDS_PER_BOARD*(17+1))) ) {
    num += STANDS_PER_BOARD;
    
    ::memset(responses, 0, sizeof(uint16_t)*2*(MAX_BOARDS*STANDS_PER_BOARD+1));
    
    success = sub_spi_transfer(fh, (char*) commands, (char*) responses, 2*num+2, SS_CONF(0, SS_LO));
  	if( success ) {
  		std::cout << "coundBoards - SPI write failed - " << sub_strerror(sub_errno) << std::endl;
  	}
  }
  if( num > STANDS_PER_BOARD*17 ) {
    num = 0;
  }
  num /= STANDS_PER_BOARD;
	
	/*******************
	* Cleanup and exit *
	*******************/
  ::free(commands);
  ::free(responses);
  
	sub_close(fh);
	
	// Report
	printf("Found %i ARX boards (%i stands)\n", num, num*STANDS_PER_BOARD);

	return num;
}
