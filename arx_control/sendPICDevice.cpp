/*****************************************************
sendPICDevice - Send a set of RS485 commands to the
specified device address.  An exit code of zero indicates that
no errors were encountered.
 
Usage:
  sendPICDevice <ATmega S/N> <address> <command>
  
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
	if( argc != 3+1 ) {
		std::cerr << "sendPICDevice - Need at 3 arguments, " << argc-1 << " provided" << std::endl;
		std::exit(EXIT_FAILURE);
	}
  
  char *endptr;
  std::string requestedSN = std::string(argv[1]);
  uint32_t device_addr = std::strtod(argv[2], &endptr);
  std::string command = std::string(argv[3]);
  
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
  // std::list<std::uint8_t> addrs = atm->list_rs485_devices();
  // std::cout << "Found " << addrs.size() << " board(s)" << std::endl;
  // for(std::uint8_t& addr: addrs) {
  //   std::cout << " " << (uint32_t) addr;
  //   if( addr == (device_addr & 0xFF) ) {
  //     std::cout << " (this is us)";
  //   }
  //   std::cout << std::endl;
  // }
  
  int size = 1;
  char buf[80] = {'\0'};
  std::cout << "Command is '" << command.c_str() << "' of size " << command.size() << std::endl;
  if( command == std::string("WAKE") ) {
    command = std::string("W");
  }
  bool status = atm->send_rs485(device_addr & 0xFF, command.c_str(), command.size(), &(buf[0]), &size);
  std::cout << "Send? " << (int32_t) status << std::endl;
  std::cout << "Received: " << size << std::endl;
  std::cout << "-> '" << std::string(buf) << "'" << std::endl;
  
	/*******************
	* Cleanup and exit *
	*******************/
	delete atm;
  
	std::exit(EXIT_SUCCESS);
}
