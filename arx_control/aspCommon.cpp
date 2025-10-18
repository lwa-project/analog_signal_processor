#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>

#include "aspCommon.hpp"

std::list<std::string> list_atmegas() {
  std::list<std::string> atmega_sns;
  
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
      
      int n = atmega::send_command(fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
      if( (n > 0) && (resp.command & atmega::COMMAND_FAILURE) == 0 ) {
        std::string sn;
        for(int i=0; i<std::min((uint16_t) 8, (uint16_t) resp.size); i++) {
          sn.push_back((char) resp.buffer[i]);
        }
        atmega_sns.push_back(sn);
      }
    } catch(const std::exception& e) {}
    
    atmega::close(fd);
  }
  return atmega_sns;
}

bool ATmega::open() {
  bool found = false;
  atmega::handle fd = -1;
  if( _sn.find("/dev") == 0 ) {
    std::cerr << "Warning: Running without device access locking" << std::endl;
    
    struct stat sb;
    if( stat(_sn.c_str(), &sb) == -1 || !S_ISCHR(sb.st_mode) ) {
      return false;
    }
    
    int open_attempts = 0;
    while( open_attempts < ATMEGA_OPEN_MAX_ATTEMPTS ) {
      try {
        fd = atmega::open(_sn);
      	break;
      } catch(const std::exception& e) {
      	open_attempts++;
      	std::this_thread::sleep_for(std::chrono::milliseconds(ATMEGA_OPEN_WAIT_MS));
      }
    }
    
    if( fd >= 0 ) {
      try {
        atmega::buffer cmd, resp;
        cmd.command = atmega::COMMAND_READ_SN;
        cmd.size = 0;
        
        int n = atmega::send_command(fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
        if( (n > 0) && (resp.command & atmega::COMMAND_FAILURE) == 0 ) {
        	found = true;
        	_fd = fd;
        }
      } catch(const std::exception& e) {}
      
      if( !found ) {
        atmega::close(fd);
      }
    }
    
    return found;
  } else {
    mode_t omsk = umask(0);
    _lock = sem_open(_sn.c_str(), O_CREAT | O_EXCL, 0666, 1);
    umask(omsk);
    if( _lock == SEM_FAILED ) {
      if( errno == EEXIST ) {
        _lock = sem_open(_sn.c_str(), 0);
      } else {
        _lock = NULL;
        return false;
      }
    }
    
    double elapsed_time = 0.0;
    auto start_time = std::chrono::steady_clock::now();
    while( sem_trywait(_lock) == -1 ) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      
      auto current_time = std::chrono::steady_clock::now();
      elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
      
      if( elapsed_time > 10000 ) {
        std::cerr << "Failed to acquire lock within 10 s" << std::endl;
        return false;
      }
    }
    
    for(std::string const& dev_name: atmega::find_devices()) {
      int open_attempts = 0;
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
        atmega::buffer cmd, resp;
        cmd.command = atmega::COMMAND_READ_SN;
        cmd.size = 0;
        
        int n = atmega::send_command(fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
        if( (n > 0) && (resp.command & atmega::COMMAND_FAILURE) == 0 ) {
          std::string sn;
          for(int i=0; i<resp.size; i++) {
            sn.push_back((char) resp.buffer[i]);
          }
          
          if( _sn.compare(sn) == 0 ) {
            found = true;
            _fd = fd;
            break;
          } else {
            _fd = -1;
          }
        }
      } catch(const std::exception& e) {}
      
      if( found ) {
        break;
      } else {
        atmega::close(fd);
      }
    }
    
    if( !found ) {
      sem_post(_lock);
      sem_close(_lock);
      _lock = NULL;
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
  
  int n = atmega::send_command(_fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
  if( (n == 0) || (resp.command & atmega::COMMAND_FAILURE) ) {
    std::cerr << "Warning: " << atmega::strerror(resp.command) << std::endl;
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
  cmd.size = std::min(size, (int) sizeof(cmd.buffer));
  ::memcpy(&(cmd.buffer[0]), inputs, std::min(size, (int) sizeof(cmd.buffer)));
  
  int n = atmega::send_command(_fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
  if( (n == 0) || (resp.command & atmega::COMMAND_FAILURE) ) {
    std::cerr << "Warning: " << atmega::strerror(resp.command) << std::endl;
    return false;
  }
  
  ::memcpy(outputs, &(resp.buffer[0]), resp.size);
  return true;
}


std::list<uint8_t> ATmega::list_rs485_devices() {
  std::list<uint8_t> rs485_addresses_list;
  if( _fd < 0 ) {
    return rs485_addresses_list;
  }
  
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_SCAN_RS485;
  cmd.size = 0;
  
  int n = atmega::send_command(_fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
  if( (n == 0) || (resp.command & atmega::COMMAND_FAILURE) ) {
    std::cerr << "Warning: " << atmega::strerror(resp.command) << std::endl;
    return rs485_addresses_list;
  }
  
  for(int i=0; i<resp.size; i++) {
    rs485_addresses_list.push_back(resp.buffer[i]);
  }
  return rs485_addresses_list;
}

bool ATmega::read_rs485(uint8_t addr, char* data, int* size) {
  if( _fd < 0 ) {
    return false;
  }
  
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_READ_RS485;
  cmd.size = 0;
  
  int n = 0;
  try {
    n = atmega::send_command(_fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
    if( (n == 0) || (resp.command & atmega::COMMAND_FAILURE) ) {
      std::cerr << "Warning: " << atmega::strerror(resp.command) << std::endl;
      return false;
    }
  } catch(const std::exception& e) {
    return false;
  }
  
  *size = resp.size;
  ::memcpy(data, &(resp.buffer[0]), resp.size);
  return true;
}

bool ATmega::write_rs485(uint8_t addr, const char* data, int size) {
  if( _fd < 0 ) {
    return false;
  }
  
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_WRITE_RS485;
  cmd.size = 1 + size;
  cmd.buffer[0] = addr;
  ::memcpy(&(cmd.buffer[1]), data, size);
  
  try {
    int n = atmega::send_command(_fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
    if( (n == 0) || (resp.command & atmega::COMMAND_FAILURE) ) {
      std::cerr << "Warning: " << atmega::strerror(resp.command) << std::endl;
      return false;
    }
  } catch(const std::exception& e) {
    return false;
  }
  
  return true;
}

bool ATmega::send_rs485(uint8_t addr, const char* in_data, int in_size, char* out_data, int* out_size) {
  if( _fd < 0 ) {
    return false;
  }
  
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_SEND_RS485;
  cmd.size = 1 + in_size;
  cmd.buffer[0] = addr;
  ::memcpy(&(cmd.buffer[1]), in_data, in_size);
 
  int n = 0; 
  try {
    n = atmega::send_command(_fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
    if( (n == 0) || (resp.command & atmega::COMMAND_FAILURE) ) {
      std::cerr << "Warning: " << atmega::strerror(resp.command) << std::endl;
      return false;
    }
  } catch(const std::exception& e) {
    return false;
  }
  
  *out_size = resp.size;
  ::memcpy(out_data, &(resp.buffer[0]), resp.size);
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
  
  int n = atmega::send_command(_fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
  if( (n == 0) || (resp.command & atmega::COMMAND_FAILURE) ) {
    std::cerr << "Warning: " << atmega::strerror(resp.command) << std::endl;
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
  cmd.size = 3;
  cmd.buffer[0] = addr;
  cmd.buffer[1] = reg;
  cmd.buffer[2] = size;
  
  try {
    int n = atmega::send_command(_fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
    if( (n == 0) || (resp.command & atmega::COMMAND_FAILURE) ) {
      std::cerr << "Warning: " << atmega::strerror(resp.command) << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(ATMEGA_I2C_WAIT_MS));
      return false;
    }
  } catch(const std::exception& e) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ATMEGA_I2C_WAIT_MS));
    return false;
  }
  
  ::memcpy(data, &(resp.buffer[0]), size);
  
  std::this_thread::sleep_for(std::chrono::milliseconds(ATMEGA_I2C_WAIT_MS));
  return true;
}

bool ATmega::write_i2c(uint8_t addr, uint8_t reg, const char* data, int size) {
  if( _fd < 0 ) {
    return false;
  }
  
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_WRITE_I2C;
  cmd.size = 2 + size;
  cmd.buffer[0] = addr;
  cmd.buffer[1] = reg;
  ::memcpy(&(cmd.buffer[2]), data, size);
  
  try {
    int n = atmega::send_command(_fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
    if( (n == 0) || (resp.command & atmega::COMMAND_FAILURE) ) {
      std::cerr << "Warning: " << atmega::strerror(resp.command) << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(ATMEGA_I2C_WAIT_MS));
      return false;
    }
  } catch(const std::exception& e) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ATMEGA_I2C_WAIT_MS));
    return false;
  }
  
  std::this_thread::sleep_for(std::chrono::milliseconds(ATMEGA_I2C_WAIT_MS));
  return true;
}


bool ATmega::clear_fault() {
  if( _fd < 0 ) {
    return false;
  }
  
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_CLR_FAULT;
  cmd.size = 0;
  
  int n = atmega::send_command(_fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
  if( resp.command & atmega::COMMAND_FAILURE ) {
    std::cerr << "Warning: " << atmega::strerror(resp.command) << std::endl;
    return false;
  }
  
  return true;
}


bool ATmega::locate() {
  if( _fd < 0 ) {
    return false;
  }
  
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_LOCATE;
  cmd.size = 0;
  
  int n = atmega::send_command(_fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
  if( resp.command & atmega::COMMAND_FAILURE ) {
    std::cerr << "Warning: " << atmega::strerror(resp.command) << std::endl;
    return false;
  }
  
  return true;
}


bool ATmega::reset() {
  if( _fd < 0 ) {
    return false;
  }
  
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_RESET;
  cmd.size = 0;
  
  int n = atmega::send_command(_fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
  
  std::this_thread::sleep_for(std::chrono::milliseconds(ATMEGA_OPEN_WAIT_MS));
  
  return true;
}
