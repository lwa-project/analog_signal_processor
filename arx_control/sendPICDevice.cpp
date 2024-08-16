/*****************************************************
sendPICDevice - Send a set of RS485 commands to the
specified device address.  An exit code of zero indicates that
no errors were encountered.
 
Usage:
  sendPICDevice [-q|--quiet] [-d|--decode] <ATmega S/N> <address> <command>
  
Options:
  None
*****************************************************/


#include <iostream>
#include <stdexcept>
#include <string>
#include <iomanip>
#include <cstring>
#include <chrono>
#include <thread>

#include "libatmega.hpp"
#include "aspCommon.hpp"

#ifndef PIC_IS_REVH
#define PIC_IS_REVH 0
#endif


typedef struct {
  int hpf;
  int lpf;
  float at1;
  float at2;
  int sig_on;
  int fee_on;
} chan_config;


void raw_to_config(uint16_t raw, chan_config* config) {
  config->hpf = raw & 1;
  config->sig_on = (raw >> 1) & 1;
  config->lpf = (raw >> 2) & 1;
  config->at1 = (((raw ^ 0xFFFF) >> 3) & 0x3F) * 0.5;
  config->at2 = (((raw ^ 0xFFFF) >> 9) & 0x3F) * 0.5;
  config->fee_on = (raw >> 15) & 1;
}


void config_to_raw(chan_config& config, uint16_t* raw) {
  *raw = 0;
  *raw |= config.hpf & 1;
  *raw |= (config.sig_on & 1) << 1;
  *raw |= (config.lpf & 1) << 2;
  *raw |= ((((uint16_t) (config.at1 * 2)) ^ 0xFFFF) & 0x3F) << 3;
  *raw |= ((((uint16_t) (config.at2 * 2)) & 0xFFFF) & 0x3F) << 9;
  *raw |= (config.fee_on & 1) << 15;
}


int main(int argc, char** argv) {
	/*************************
	* Command line parsing   *
	*************************/
  // Make sure we have the right number of arguments to continue
	std::list<std::string> arg_str;
  bool verbose = true;
  bool decode = false;
  for(int i=1; i<argc; i++) {
    std::string temp = std::string(argv[i]);
    if( temp[0] != '-' ) {
      arg_str.push_back(temp);
    } else {
      if( (temp == "-q") || (temp == "--quiet") ) {
        verbose = false;
      } else if( (temp == "-d") || (temp == "--decode") ) {
        decode = true;
      }
    }
  }
  if( (arg_str.size() < 3) || (arg_str.size() % 2 == 0) ) {
		std::cerr << "sendPICDevice - Need at 3 arguments, " << arg_str.size() << " provided" << std::endl;
		std::exit(EXIT_FAILURE);
	}
  
  // Partially unpack
  std::string requestedSN = arg_str.front();
  arg_str.pop_front();
  
  /************************************
	* ATmega device selection and ready *
	************************************/
  ATmega *atm = new ATmega(requestedSN);
  
  bool success = atm->open();
  if( !success ) {
    std::cerr << "sendPICDevice - failed to open " << requestedSN << std::endl;
	  std::exit(EXIT_FAILURE);
  }
  
  /****************************************
	* Send the command and get the response *
	****************************************/
  // Process the commands
  while( arg_str.size() > 0 ) {
    // Grab the next address/command pair
    uint32_t device_addr = std::stoi(arg_str.front());
    arg_str.pop_front();
    std::string command = arg_str.front();
    arg_str.pop_front();
    
    int size = 0;
    char buf[80] = {'\0'};
    if( command == std::string("WAKE") ) {
      command = std::string("W");
    }
    success = atm->send_rs485(device_addr, command.c_str(), command.size(), &(buf[0]), &size);
    if( !success ) {
      std::cerr << "sendPICDevice - send failed " << std::endl;
      delete atm;
  	  std::exit(EXIT_FAILURE);
    }
    
    if( verbose ) {
      std::string temp = std::string(&(buf[1]));
      std::cout << "Received: " << size << "B with status " << (uint16_t) buf[0] << std::endl;
      std::cout << "Response: " << std::quoted(temp) << std::endl;
      
      if( decode ) {
        if( command == "GTIM" ) {
          uint32_t value = std::stoi(std::string("0x") + temp, nullptr, 16);
          std::cout << "Board Time: " << value << " s" << std::endl;
          
        } else if( command == "LAST" ) {
          std::string ctype = "normal";
          if( temp.substr(0,1) == "b" ) {
            ctype = "broadcast";
          }
          try {
            std::cout << "Last Command: " << std::quoted(temp.substr(1,80)) << " (" << ctype << ")" << std::endl;
          } catch(const std::out_of_range& e) {
            std::cout << "No commands received since board startup" << std::endl;
          }
          
        } else if( command == "ARXN" ) {
          std::cout << "Serial Number:       " << temp.substr(0,4) << std::endl;
          std::cout << "Software Version:    " << temp.substr(4,4) << std::endl;
          std::cout << "Coax/Fiber Setup:    ";
          int cf_map = std::stoi(std::string("0x") + temp.substr(8,4), nullptr, 16);
          for(int i=0; i<16; i++) {
            if( (cf_map >> i) & 1 ) {
              std::cout << "F";
            } else {
              std::cout << "C";
            }
          }
          std::cout << std::endl;
          int temp_map = std::stoi(std::string("0x") + temp.substr(12,2), nullptr, 16);
          std::cout << "Temperatures mapped: " << temp_map << std::endl;
          if( temp_map > 0 ) {
            for(int i=0; i<temp_map; i++) {
              int chan_map = std::stoi(std::string("0x") + temp.substr(14+i,1), nullptr, 16);
              std::cout << i+1 << ": " << chan_map << std::endl;
            }
          }
          
        } else if( command == "GETA" ) {
          for(int i=0; i<size/4; i++) {
            uint16_t value = std::stoi(std::string("0x") + temp.substr(4*i, 4), nullptr, 16);
            chan_config cconfig;
            raw_to_config(value, &cconfig);
            std::cout << i+1 << ": \t" << "HPF = " << cconfig.hpf << std::endl;
            std::cout << "    \t" << "LPF = " << cconfig.lpf << std::endl;
            std::cout << "    \t" << "AT1 = " << cconfig.at1 << " dB" << std::endl;
            std::cout << "    \t" << "AT2 = " << cconfig.at2 << " dB" << std::endl;
            std::cout << "    \t" << "FEE = " << cconfig.fee_on << std::endl;
            std::cout << "    \t" << "SON = " << cconfig.sig_on << std::endl;
          }
          
        } else if( command.substr(0,4) == "GETC" ) {
          uint16_t value = std::stoi(std::string("0x") + temp, nullptr, 16);
          chan_config cconfig;
          raw_to_config(value, &cconfig);
          std::cout << "HPF = " << cconfig.hpf << std::endl;
          std::cout << "LPF = " << cconfig.lpf << std::endl;
          std::cout << "AT1 = " << cconfig.at1 << " dB" << std::endl;
          std::cout << "AT2 = " << cconfig.at2 << " dB" << std::endl;
          std::cout << "FEE = " << cconfig.fee_on << std::endl;
          std::cout << "SON = " << cconfig.sig_on << std::endl;
          
        } else if( command == "CURA" ) {
          for(int i=0; i<size/4; i++) {
            float value = std::stoi(std::string("0x") + temp.substr(4*i, 4), nullptr, 16);
            #if defined(PIC_IS_REVH) && PIC_IS_REVH
              value *= 0.004;
              value *= 100;
            #else
              value *= 3.3 / 1024 / 2.38;
              value *= 1000;
            #endif
            std::cout << i+1 << ": " << std::fixed << std::setprecision(1) << value << " mA" << std::endl;
          }
        
        } else if( (command.substr(0,4) == "CURC") ) {
          float value = std::stoi(std::string("0x") + temp, nullptr, 16);
          #if defined(PIC_IS_REVH) && PIC_IS_REVH
            value *= 0.004;
            value *= 100;
          #else
            value *= 3.3 / 1024 / 2.38;
            value *= 1000;
          #endif
          std::cout << std::fixed << std::setprecision(1) << value << " mA" << std::endl;
          
        } else if( command == "POWA" ) {
          for(int i=0; i<size/4; i++) {
            float value = std::stoi(std::string("0x") + temp.substr(4*i, 4), nullptr, 16);
            value *= 0.004;
            value = value/2.296*value/2.296/50*1000*1000;
            std::cout << i+1 << ": " << std::fixed << std::setprecision(1) << value << " uW" << std::endl;
          }
          
        } else if( command.substr(0,4) == "POWC" ) {
          float value = std::stoi(std::string("0x") + temp, nullptr, 16);
          value *= 0.004;
          value = value/2.296*value/2.296/50*1000*1000;
          std::cout << std::fixed << std::setprecision(1) << value << " uW" << std::endl;
          
        } else if( command == "TEMP" ) {
          float value = std::stoi(std::string("0x") + temp, nullptr, 16);
          value *= 0.1;
          std::cout << "PIC Temperature: " << std::fixed << std::setprecision(1) << value << " C" << std::endl;
          
        } else if( command == "OWDC" ) {
          std::cout << "Number of Temp. Sensors: " << std::stoi(std::string("0x") + temp, nullptr, 16) << std::endl;
          
        } else if( command == "OWTE" ) {
          for(int i=0; i<size/4; i++) {
            float value = std::stoi(std::string("0x") + temp.substr(4*i, 4), nullptr, 16);
            value *= 0.0625;
            std::cout << i+1 << ": " << std::fixed << std::setprecision(1) << value << " C" << std::endl;
          }
        }
      }
    }
  }
  
	/*******************
	* Cleanup and exit *
	*******************/
	delete atm;
  
	std::exit(EXIT_SUCCESS);
}
