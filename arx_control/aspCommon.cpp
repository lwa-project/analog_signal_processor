#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <filesystem>

#include "aspCommon.hpp"

std::list<std::string> list_possible_atmegas() {
  std::list<std::string> devices;
  
  for(auto const& dir_entry: std::filesystem::directory_iterator{"/sys/bus/usb/devices/"}) {
    std::string modalias = dir_entry.path()/"modalias";
    
    if( std::filesystem::exists(modalias) ) {
      int md = open(modalias.c_str(), O_RDONLY);
      char contents[256] = {0};
      if( md >= 0 ) {
        read(md, &contents, 255);
        close(md);
      }

      std::string scontents = std::string(contents);
      if( scontents.find("usb:v0403p6001") != -1 ) {
        for(auto const& child_entry: std::filesystem::directory_iterator{dir_entry}) {
          std::string entry_name = child_entry.path();
          
          if( entry_name.find("ttyUSB") != -1 ) {
            devices.push_back(std::string("/dev/")+std::string(child_entry.path().filename()));
          }
        }
      }
    }
  }
  
  return devices;
}

std::list<std::string> list_atmegas() {
  std::list<std::string> devices, atmega_sns;
  
  devices = list_possible_atmegas();
  
  for(std::string const& dev_name: devices) {
    std::cout << " " << dev_name << std::endl;
    
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
      atmega::configure_port(fd);
      
      atmega::buffer cmd, resp;
      cmd.command = atmega::COMMAND_READ_SN;
      cmd.size = htons(0);
      
      open_attempts = 0;
      int n = atmega::send_command(fd, &cmd, &resp);
      while( (n == 0) && (open_attempts < ATMEGA_OPEN_MAX_ATTEMPTS) ) {
        open_attempts++;
        std::this_thread::sleep_for(std::chrono::milliseconds(ATMEGA_OPEN_WAIT_MS));
        n = atmega::send_command(fd, &cmd, &resp);
      }
      resp.size = ntohs(resp.size);
      
      if( resp.command != atmega::COMMAND_FAILURE ) {
        std::string sn;
        for(int i=0; i<resp.size; i++) {
          sn.append((char*) &(resp.buffer[i]));
        }
        atmega_sns.push_back(sn);
      }
    } catch(const std::exception& e) {}
    atmega::close(fd);
  }
  return atmega_sns;
}

bool ATmega::open() {
  std::list<std::string> devices = list_possible_atmegas();
    
  bool found = false;
  for(std::string const& dev_name: devices) {
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
    
    if( fd < 0 ) {
      continue;
    }
    
    try {
      atmega::configure_port(fd);
      
      atmega::buffer cmd, resp;
      cmd.command = atmega::COMMAND_READ_SN;
      cmd.size = htons(0);
      
      this->_send(&cmd, &resp);
      if( resp.command != atmega::COMMAND_FAILURE ) {
        std::string sn;
        for(int i=0; i<resp.size; i++) {
          sn.append((char*) &(resp.buffer[i]));
        }
        
        if( _sn.compare(sn) == 0 ) {
          found = true;
          _fd = fd;
          _dev = dev_name;
        }
      }
    } catch(const std::exception& e) {}
    
    if( found ) {
      break;
    } else {
      atmega::close(fd);
    }
  }
  
  return found;
}


std::string ATmega::get_version() {
  std::string version;
  if( _fd < 0 ) {
    return version;
  }
  
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_READ_VER;
  cmd.size = 0;
  
  this->_send(&cmd, &resp);
  if( resp.command == atmega::COMMAND_FAILURE ) {
    return version;
  }
  
  for(int i=0; i<resp.size; i++) {
    version.append((char*) &(resp.buffer[i]));
  }
  return version;
}


bool ATmega::transfer_spi(const char* inputs, char* outputs, int size) {
  if( _fd < 0 ) {
    return false;
  }
  
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_TRANSFER_SPI;
  cmd.size = htons(size);
  ::memcpy(&(cmd.buffer[0]), inputs, size);
  
  this->_send(&cmd, &resp);
  if( resp.command == atmega::COMMAND_FAILURE ) {
    return false;
  }
  
  ::memcpy(outputs, &(resp.buffer[0]), resp.size);
  return true;
}

std::list<uint8_t> ATmega::list_i2c_devices() {
  std::list<uint8_t> i2c_addresses_list;
  if( _fd < 0 ) {
    return i2c_addresses_list;
  }
  
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_SCAN_I2C;
  cmd.size = 0;
  
  this->_send(&cmd, &resp);
  if( resp.command == atmega::COMMAND_FAILURE ) {
    return i2c_addresses_list;
  }
  
  for(int i=0; i<resp.size; i++) {
    i2c_addresses_list.push_back(resp.buffer[i]);
  }
  return i2c_addresses_list;
}
  

bool ATmega::read_i2c(uint8_t addr, uint8_t reg, char* data, int size) {
  if( _fd < 0 ) {
    return false;
  }
  
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_READ_I2C;
  cmd.size = htons(size);
  
  this->_send(&cmd, &resp);
  if( resp.command == atmega::COMMAND_FAILURE ) {
    return false;
  }
  
  ::memcpy(data, &(resp.buffer[0]), resp.size);
  return true;
}

bool ATmega::write_i2c(uint8_t addr, uint8_t reg, const char* data, int size) {
  if( _fd < 0 ) {
    return false;
  }
  
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_WRITE_I2C;
  cmd.size = htons(size);
  ::memcpy(&(cmd.buffer[0]), data, size);
  
  this->_send(&cmd, &resp);
  if( resp.command == atmega::COMMAND_FAILURE ) {
    return false;
  }
  
  return true;
}


std::list<float> ATmega::read_adcs() {
  std::list<float> values;
  if( _fd < 0 ) {
    return values;
  }
  
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_READ_ADCS;
  cmd.size = htons(0);
  
  this->_send(&cmd, &resp);
  if( resp.command == atmega::COMMAND_FAILURE ) {
    return values;
  }
  
  float value = 0.0;
  for(int i=0; i<resp.size; i+=sizeof(int)) {
    ::memcpy(&value, &(resp.buffer[i]), sizeof(int));
    value = ntohl(value);
    values.push_back(value/1023.*5);
  }
  
  return values;
}
