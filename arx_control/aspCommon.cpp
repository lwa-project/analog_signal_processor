#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <unistd.h>

#include "aspCommon.hpp"

std::list<std::string> list_atmegas() {
  std::list<std::string> atmega_sns;

  for(const auto& dev: atmega::find_devices()) {
    if( !dev.second.empty() ) {
      atmega_sns.push_back(dev.second.substr(0, ATMEGA_MAX_SN_LEN));
    }
  }
  return atmega_sns;
}

bool ATmega::acquire_lock(const std::string& lock_path) {
  mode_t omsk = umask(0);
  _lock_fd = ::open(lock_path.c_str(), O_RDWR | O_CREAT, 0666);
  umask(omsk);
  if( _lock_fd < 0 ) {
    return false;
  }
  
  auto start_time = std::chrono::steady_clock::now();
  while( flock(_lock_fd, LOCK_EX | LOCK_NB) == -1 ) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  
    auto current_time = std::chrono::steady_clock::now();
    double elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
  
    if( elapsed_time > 10000 ) {
      std::cerr << "Failed to acquire lock within 10 s" << std::endl;
      ::close(_lock_fd);
      _lock_fd = -1;
      return false;
    }
  }
  
  return true;
}

bool ATmega::open() {
  bool found = false;
  atmega::handle fd = -1;
  int open_attempts = 0;

  // Resolve the device path to open
  std::string dev_path;
  std::string lock_path;
  if( _sn.find("/dev") == 0 ) {
    // Caller provided a device path directly
    struct stat sb;
    if( stat(_sn.c_str(), &sb) == -1 || !S_ISCHR(sb.st_mode) ) {
      return false;
    }
    dev_path = _sn;

    std::string sanitized = _sn.substr(5);  // strip "/dev/"
    std::replace(sanitized.begin(), sanitized.end(), '/', '_');
    lock_path = "/dev/shm/arx_dev_" + sanitized + ".lock";
  } else {
    // Caller provided a serial number — look up the device path via USB
    // serial descriptor (no device open required).  The USB serial may be
    // longer than what fits in the EEPROM so compare only the first
    // ATMEGA_MAX_SN_LEN characters.
    for(const auto& dev: atmega::find_devices()) {
      if( dev.second.substr(0, ATMEGA_MAX_SN_LEN) == _sn ) {
        dev_path = dev.first;
        break;
      }
    }
    if( dev_path.empty() ) {
      return false;
    }

    lock_path = "/dev/shm/arx_" + _sn + ".lock";
  }

  // Acquire the lock before touching the device
  if( !acquire_lock(lock_path) ) {
    return false;
  }

  // Open the specific device
  while( open_attempts < ATMEGA_OPEN_MAX_ATTEMPTS ) {
    try {
      fd = atmega::open(dev_path);
      break;
    } catch(const std::exception& e) {
      open_attempts++;
      std::this_thread::sleep_for(std::chrono::milliseconds(ATMEGA_OPEN_WAIT_MS));
    }
  }

  if( fd >= 0 ) {
    // Verify the EEPROM serial number matches as a sanity check
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

  if( !found ) {
    flock(_lock_fd, LOCK_UN);
    ::close(_lock_fd);
    _lock_fd = -1;
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


float ATmega::get_temperature() {
  float temp_C = -99.0;
  if( _fd < 0 ) {
    return temp_C;
  }
  
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_READ_TEMPERATURE;
  cmd.size = 0;
  
  int n = atmega::send_command(_fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
  if( (n == 0) || (resp.command & atmega::COMMAND_FAILURE) ) {
    std::cerr << "Warning: " << atmega::strerror(resp.command) << std::endl;
    return temp_C;
  }
  
  if( resp.size != sizeof(float) ) {
    std::cerr << "Warning: response is not float sized" << std::endl;
    return temp_C;
  }
  
  ::memcpy(&temp_C, resp.buffer, sizeof(float));
  return temp_C;
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
