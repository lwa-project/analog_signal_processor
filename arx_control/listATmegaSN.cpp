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
#include <cstdio>

#include "libatmega.hpp"
#include "aspCommon.hpp"


int main(int argc, char* argv[]) {
  /*************************
	* Command line parsing   *
	*************************/
  
  /************************
	* ATmega device listing *
	*************************/
  for(std::string const& dev_name: atmega::find_devices()) {
    int open_attempts = 0;
    atmega::handle fd = -1;
    while( open_attempts < ATMEGA_OPEN_MAX_ATTEMPTS ) {
      try {
        fd = atmega::open(dev_name);
        break;
      } catch(const std::exception& e) {
        open_attempts++;
        std::this_thread::sleep_for(std::chrono::milliseconds(ATMEGA_OPEN_WAIT_MS));
      }
    }
    
    if( fd < 0) {
      continue;
    }
    
    try {
      atmega::buffer cmd, resp;
      cmd.command = atmega::COMMAND_READ_SN;
      cmd.size = 0;
      
      int n = atmega::send_command(fd, &cmd, &resp, std::min(3, ATMEGA_OPEN_MAX_ATTEMPTS), ATMEGA_OPEN_WAIT_MS);
      if( (n > 0) && (resp.command & atmega::COMMAND_FAILURE) == 0 ) {
        std::string sn;
        for(int i=0; i<std::min((uint16_t) 8, (uint16_t) resp.size); i++) {
          sn.push_back((char) resp.buffer[i]);
        }
        std::cout << "Found " << sn << " at " << dev_name << std::endl;
      }
    } catch(const std::exception& e) {}
    
    atmega::close(fd);
  }
  
	/*******************
	* Cleanup and exit *
	*******************/
	
	std::exit(EXIT_SUCCESS);
}
