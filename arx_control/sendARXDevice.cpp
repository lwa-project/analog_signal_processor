/*****************************************************
sendARXDevice - Send a set of SPI commands to the
specified devices.  An exit code of zero indicates that
no errors were encountered.
 
Usage:
  sendARXDevice <ATmega S/N> <total stand count> <device> <command> ...

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

#include "libatmega.hpp"
#include "aspCommon.hpp"


int main(int argc, char** argv) {
  /*************************
  * Command line parsing   *
  *************************/
  // Make sure we have the right number of arguments to continue
  if( argc < 4+1 ) {
    std::cerr << "sendARXDevice - Need at least 4 arguments, " << argc-1 << " provided" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  char *endptr;
  std::string requestedSN = std::string(argv[1]);
  uint32_t device_count = std::strtod(argv[2], &endptr);
  
  CommandQueue *queue = new CommandQueue(device_count);
  for(int i=3; i<argc; i+=2) {
    uint32_t device = std::strtod(argv[i], &endptr);
    uint16_t command = std::strtod(argv[i+1], &endptr);
    try {
      queue->add_command(device, command);
    } catch(const std::exception& e) {
      std::cerr << "sendARXDevice - invalid command " << device << " @ " << std::hex << command << std::dec << ": " << e.what() << std::endl;
      delete queue;
      std::exit(EXIT_FAILURE);
    }
  }
  
  /************************************
  * ATmega device selection and ready *
  ************************************/
  ATmega *atm = new ATmega(requestedSN);
  
  bool success = atm->open();
  if( !success ) {
    std::cerr << "sendARXDevice - failed to open " << requestedSN << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  /****************************************
  * Send the command and get the response *
  ****************************************/
  // Process the commands
  uint16_t *commands, *responses;
  responses = (uint16_t*) calloc(sizeof(uint16_t), device_count+1);
  while( !queue->is_empty() ) {
    commands = queue->get_commands();
    
    success = atm->transfer_spi((char*) commands, (char*) responses, 2*device_count+2);
    if( !success ) {
      std::cerr << "sendARXDevice - SPI write failed" << std::endl;
      ::free(commands);
      ::free(responses);
      delete atm;
      delete queue;
      std::exit(EXIT_FAILURE);
    }
    
    if( responses[device_count] != SPI_COMMAND_MARKER ) {
      std::cerr << "sendARXDevice - SPI write returned a marker of "
                << std::hex << responses[device_count] << std::dec << " instead of "
                << std::hex << SPI_COMMAND_MARKER << std::dec << std::endl;
      ::free(commands);
      ::free(responses);
      delete atm;
      delete queue;
      std::exit(EXIT_FAILURE);
    }
    
    ::free(commands);
  }
  
  /*******************
  * Cleanup and exit *
  *******************/
  ::free(responses);
  
  delete atm;
  delete queue;
  
  std::exit(EXIT_SUCCESS);
}
