/*****************************************************
setATmegaSN - Set the internal serial number of a
ATmega device to that of its USB interface chip.
 
Usage:
  readARXDevice <device name>

Options:
  None
*****************************************************/


#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>
#include <cstdio>
#include <arpa/inet.h>

#include "libatmega.hpp"


int main(int argc, char* argv[]) {
  /*************************
	* Command line parsing   *
	*************************/
  // Make sure we have the right number of arguments to continue
	if( argc < 1+1 ) {
		std::cerr << "setATmegaSA - Need at least 1 argument, " << argc-1 << " provided" << std::endl;
		std::exit(EXIT_FAILURE);
	}
  
  std::string device_name = std::string(argv[1]);
  
  /***************************************
  * Find the serial number using udevadm *
  ****************************************/
  std::string device_sn = "";
  std::string udev_lookup = std::string("udevadm info --name=")+device_name;
  std::unique_ptr<FILE, decltype(&::pclose)> pipe(::popen(udev_lookup.c_str(), "r"), ::pclose);
  if( pipe != nullptr ) {
    char buffer[256];
    while( fgets(buffer, 256, pipe.get()) != nullptr ) {
      if( strstr(buffer, "ID_SERIAL_SHORT=") != nullptr) {
        device_sn = std::string(&(buffer[16]));
      }
    }
  }
  if( device_sn.size() == 0 ) {
    std::cerr << "setATmegaSN - Failed to find a serial number for " << device_name << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  /******************************************
	* ATmega device selection and programming *
	*******************************************/
  atmega::handle fd = atmega::open(device_name);
  if( fd < 0 ) {
    std::cerr << "setATmegaSN - Failed to open device" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  /************************
	* Set the serial number *
	*************************/
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_UNLOCK;
  cmd.size = htons(0);
  atmega::send_command(fd, &cmd, &resp);
  if( resp.command == atmega::COMMAND_FAILURE ) {
    atmega::close(fd);
    std::cerr << "setATmegaSN - Failed to unlock device" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  cmd.command = atmega::COMMAND_WRITE_SN;
  cmd.size = htons(device_name.size());
  ::memcpy(&(cmd.buffer[0]), device_name.c_str(), device_name.size());
  atmega::send_command(fd, &cmd, &resp);
  if( resp.command == atmega::COMMAND_FAILURE ) {
    atmega::close(fd);
    std::cerr << "setATmegaSN - Failed to write serial number to device" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  cmd.command = atmega::COMMAND_LOCK;
  cmd.size = htons(0);
  atmega::send_command(fd, &cmd, &resp);
  if( resp.command == atmega::COMMAND_FAILURE ) {
    atmega::close(fd);
    std::cerr << "setATmegaSN - Failed to lock device" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
	/*******************
	* Cleanup and exit *
	*******************/
	atmega::close(fd);
	
	std::exit(EXIT_SUCCESS);
}
