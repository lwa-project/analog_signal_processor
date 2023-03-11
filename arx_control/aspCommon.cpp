#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <filesystem>

#include "aspCommon.hpp"

std::list<std::string> list_possible_atmega() {
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

std::list<std::string> list_atmega() {
  std::list<std::string> devices, atmega_sns;
  
  devices = list_possible_atmega();
  std::cout << "devices.size()=" << devices.size() << std::endl;
  
  for(auto const& dev_name: devices) {
    std::cout << " " << dev_name << std::endl;
    
    int open_attempts = 0;
    int fd = ::open(dev_name.c_str(), O_RDONLY | O_NOCTTY);
    while( (fd < 0) && (open_attempts < ATMEGA_OPEN_MAX_ATTEMPTS) ) {
      open_attempts++;
      std::this_thread::sleep_for(std::chrono::milliseconds(ATMEGA_OPEN_WAIT_MS));
      fd = ::open(dev_name.c_str(), O_RDONLY | O_NOCTTY);
    }
    
    if( fd < 0) {
      continue;
    }
    
    std::cout << "fd=" << fd << std::endl;
    if( configure_port(fd) == 0 ) {
      atmega_buffer cmd, resp;
      cmd.command = 0x01;
      cmd.size = htons(0);
      
      open_attempts = 0;
      int n = send_command(fd, &cmd, &resp);
      while( (n == 0) && (open_attempts < ATMEGA_OPEN_MAX_ATTEMPTS) ) {
        open_attempts++;
        std::this_thread::sleep_for(std::chrono::milliseconds(ATMEGA_OPEN_WAIT_MS));
        n = send_command(fd, &cmd, &resp);
      }
      resp.size = ntohs(resp.size);
      
      if( resp.command != ATMEGA_COMMAND_FAILED ) {
        std::string sn;
        for(int i=0; i<resp.size; i++) {
          sn.append((char*) &(resp.buffer[i]));
        }
        atmega_sns.push_back(sn);
      }
    }
    close(fd);
  }
  return atmega_sns;
}

bool ATmega::open() {
  std::list<std::string> devices = list_possible_atmega();
    
  bool found = false;
  for(auto const& dev_name: devices) {
    int open_attempts = 0;
    int fd = ::open(dev_name.c_str(), O_RDONLY | O_NOCTTY);
    while( (fd < 0) && (open_attempts < ATMEGA_OPEN_MAX_ATTEMPTS) ) {
      open_attempts++;
      std::this_thread::sleep_for(std::chrono::milliseconds(ATMEGA_OPEN_WAIT_MS));
      fd = ::open(dev_name.c_str(), O_RDONLY | O_NOCTTY);
    }
    
    if( fd < 0 ) {
      continue;
    }
    
    if( configure_port(fd) != 0 ) {
      atmega_buffer cmd, resp;
      cmd.command = 0x01;
      cmd.size = htons(0);
      
      int n = this->_send(&cmd, &resp);
      if( resp.command != ATMEGA_COMMAND_FAILED ) {
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
    }
    
    if( found ) {
      break;
    } else {
      close(fd);
    }
  }
  
  return found;
}


std::string ATmega::get_version() {
  std::string version;
  if( _fd < 0 ) {
    return version;
  }
  
  atmega_buffer cmd, resp;
  cmd.command = 0x02;
  cmd.size = 0;
  
  int n = this->_send(&cmd, &resp);
  if( resp.command == ATMEGA_COMMAND_FAILED ) {
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
  
  atmega_buffer cmd, resp;
  cmd.command = 0x11;
  cmd.size = htons(size);
  ::memcpy(&(cmd.buffer[0]), inputs, size);
  
  int n = this->_send(&cmd, &resp);
  if( resp.command == ATMEGA_COMMAND_FAILED ) {
    return false;
  }
  
  ::memcpy(outputs, &(resp.buffer[0]), resp.size);
  return (n != 0);
}

std::list<uint8_t> Sub20::list_i2c_devices() {
  std::list<uint8_t> i2c_addresses_list;
  if( _fd < 0 ) {
    return i2c_addresses_list;
  }
  
  atmega_buffer cmd, resp;
  cmd.command = 0x31;
  cmd.size = 0;
  
  int n = this->_send(&cmd, &resp);
  if( resp.command == ATMEGA_COMMAND_FAILED ) {
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
  
  atmega_buffer cmd, resp;
  cmd.command = 0x32;
  cmd.size = htons(size);
  
  int n = this->_send(&cmd, &resp);
  if( resp.command == ATMEGA_COMMAND_FAILED ) {
    return false;
  }
  
  ::memcpy(data, &(resp.buffer[0]), resp.size);
  return true;
}

bool ATmega::write_i2c(uint8_t addr, uint8_t reg, const char* data, int size) {
  if( _fd < 0 ) {
    return false;
  }
  
  atmega_buffer cmd, resp;
  cmd.command = 0x33;
  cmd.size = htons(size);
  ::memcpy(&(cmd.buffer[0], data, size);
  
  int n = this->_send(&cmd, &resp);
  if( resp.command == ATMEGA_COMMAND_FAILED ) {
    return false;
  }
  
  return true;
}


std::list<float> read_adcs() {
  std::list<float> values;
  if( _fd < 0 ) {
    return values;
  }
  
  atmega_buffer cmd, resp;
  cmd.command = 4;
  cmd.size = htons(0);
  
  int n = this->_send(&cmd, &resp);
  if( resp.command == ATMEGA_COMMAND_FAILED ) {
    return values;
  }
  
  float value = 0.0;
  for(int i=0; i<resp.size; i+=sizeof(int)) {
    ::memcpy(&value, &(resp.buffer[i]), sizeof(int));
    value = ntohl(vale);
    values.push_back(value/1023.*5);
  }
  
  return values;
}
