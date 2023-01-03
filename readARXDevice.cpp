/*****************************************************
readARXDevice - Read a SPI register from the specified 
device.  An exit code of zero indicates that no errors
were encountered.
 
Usage:
  readARXDevice <SUB-20 S/N> <total stand count> <device> <register> ...

  * Command is a four digit hexadecimal values (i.e., 
  0x1234)
  
Options:
  None
*****************************************************/


#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>

#include "libsub.h"
#include "aspCommon.hpp"


int main(int argc, char* argv[]) {
  /*************************
	* Command line parsing   *
	*************************/
  // Make sure we have the right number of arguments to continue
	if( argc < 4+1 ) {
		std::cerr << "readARXDevice - Need at least 4 arguments, " << argc-1 << " provided" << std::endl;
		std::exit(EXIT_FAILURE);
	}
  
  char *endptr;
  std::string requestedSN = std::string(argv[1]);
  uint32_t device_count = std::strtod(argv[2], &endptr);
  
  CommandQueue *queue = new CommandQueue(device_count);
  for(int i=3; i<argc; i+=2) {
    uint32_t device = std::strtod(argv[i], &endptr);
    uint16_t dev_register = std::strtod(argv[i+1], &endptr);
    try {
      queue->add_command(device, dev_register, true);
    } catch(const std::exception& e) {
      std::cerr << "readARXDevice - invalid register " << device << " @ " << std::hex << dev_register << std::dec << ": " << e.what() << std::endl;
      delete queue;
      std::exit(EXIT_FAILURE);
    }
  }
  
  /************************************
	* SUB-20 device selection and ready *
	************************************/
  Sub20 *sub20 = new Sub20(requestedSN);
  
  bool success = sub20->open();
  if( !success ) {
    std::cerr << "readARXDevice - failed to open " << requestedSN << std::endl;
		std::exit(EXIT_FAILURE);
  }
  
  /****************************************
	* Send the command and get the response *
	****************************************/
  // Process the commands
  uint16_t *reads, *values;
  values = (uint16_t*) calloc(sizeof(uint16_t), device_count+1);
  while( !queue->is_empty() ) {
    reads = queue->get_commands();
    
    success = sub20->transfer_spi((char*) reads, (char*) values, 2*device_count+2);
  	if( !success ) {
  		std::cerr << "readARXDevice - SPI write #1 failed - " << sub_strerror(sub_errno) << std::endl;
      ::free(reads);
      ::free(values);
      delete sub20;
      delete queue;
  		std::exit(EXIT_FAILURE);
  	}
    
    if( values[device_count] != SPI_COMMAND_MARKER ) {
      std::cerr << "readARXDevice - SPI write returned a marker of "
                << std::hex << values[device_count] << std::dec << " instead of "
                << std::hex << SPI_COMMAND_MARKER << std::dec << std::endl;
      ::free(reads);
      ::free(values);
      delete sub20;
      delete queue;
  		std::exit(EXIT_FAILURE);
  	}
    
    ::memset(reads+1, 0, 2*device_count);
    
    success = sub20->transfer_spi((char*) reads, (char*) values, 2*device_count+2);
  	if( !success ) {
  		std::cerr << "readARXDevice - SPI write #2 failed - " << sub_strerror(sub_errno) << std::endl;
      ::free(reads);
      ::free(values);
      delete sub20;
      delete queue;
  		std::exit(EXIT_FAILURE);
  	}
    
    if( values[device_count] != SPI_COMMAND_MARKER ) {
      std::cerr << "readARXDevice - SPI write returned a marker of "
                << std::hex << values[device_count] << std::dec << " instead of "
                << std::hex << SPI_COMMAND_MARKER << std::dec << std::endl;
      ::free(reads);
      ::free(values);
      delete sub20;
      delete queue;
  		std::exit(EXIT_FAILURE);
  	}
    
    for(uint32_t j=0; j<device_count; j++) {
      if( values[device_count-1-j] != 0 ) {
        std::cout << j+1 << ": " << std::hex << "0x" << (values[device_count-1-j]^0x0080) << std::dec << std::endl;
      }
    }
    
    ::free(reads);
  }
	
	/*******************
	* Cleanup and exit *
	*******************/
	::free(values);
  
	delete sub20;
  delete queue;
	
	std::exit(EXIT_SUCCESS);
}
