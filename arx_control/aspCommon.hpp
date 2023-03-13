#ifndef __ASPCOMMON_HPP
#define __ASPCOMMON_HPP

/*
  aspCommon.h - Header library with common values needed for using the ATmega
  device with the SPI and I2C buses.
*/

#include <string>
#include <vector>
#include <queue>
#include <list>
#include <thread>
#include <cstring>
#include <stdexcept>
#include <arpa/inet.h>

#include "libatmega.hpp"

// ATmega device opening control
#define ATMEGA_OPEN_MAX_ATTEMPTS 20
#define ATMEGA_OPEN_WAIT_MS  5


// ARX board configuration
#define STANDS_PER_BOARD 8
#define MAX_BOARDS 33


// Command verification marker
#define SPI_COMMAND_MARKER 0x0120


// Uncomment the next line to include polling of module temperatures
//#define __INCLUDE_MODULE_TEMPS__


// Uncommend the next line to decode the module type (causes readPSU to run slower)
//#define __DECODE_MODULE_TYPE__


// Uncomment the next line to use input rather than the module outuput current
//#define __USE_INPUT_CURRENT__

// Get a list of all USB devices that *might* be ATmega
std::list<std::string> list_possible_atmegas();


// Get a list of all ATmega serial numbers
std::list<std::string> list_atmegas();


// Class to simplify interfacing with a ATmega via the libatmega library
class ATmega {
private:
  std::string    _sn;
  std::string    _dev;
  atmega::handle _fd;
  
  inline void _send(const atmega::buffer* cmd, atmega::buffer* resp) {
    int open_attempts = 0;
    int n = atmega::send_command(_fd, cmd, resp);
    while( (n == 0) && (open_attempts < ATMEGA_OPEN_MAX_ATTEMPTS) ) {
      open_attempts++;
      std::this_thread::sleep_for(std::chrono::milliseconds(ATMEGA_OPEN_WAIT_MS));
      n = atmega::send_command(_fd, cmd, resp);
    }
    resp->size = ntohs(resp->size);
  }
public:
  ATmega(std::string sn): _sn(""), _dev(""), _fd(-1) {
    _sn = sn;
  }
  ~ATmega() {
    atmega::close(_fd);
  }
  bool open();
  std::string get_version();
  bool transfer_spi(const char* inputs, char* outputs, int size);
  std::list<uint8_t> list_i2c_devices();
  bool read_i2c(uint8_t addr, uint8_t reg, char* data, int size);
  bool write_i2c(uint8_t addr, uint8_t reg, const char* data, int size);
  std::list<float> read_adcs();
};


class CommandQueue {
private:
  uint32_t _size;
  
  std::vector<std::queue<uint16_t> > _buffer;
  
public:
  CommandQueue(uint32_t size) {
    _size = size;
    
    for(uint32_t i=0; i<_size; i++) {
      std::queue<uint16_t> device_buffer;
      _buffer.push_back(device_buffer);
    }
  }
  inline void add_command(uint32_t device, uint16_t command, bool is_read=false) {
    if( (device < 1) || (device > _size) ) {
      throw(std::runtime_error("Invalid device number"));
    }
    if( is_read ) {
       command |= 0x0080;
    }
    _buffer[device-1].push(command);
  }
  inline bool is_empty() {
    bool empty = true;
    for(uint32_t i=0; i<_size; i++) {
      if( _buffer[i].size() > 0 ) {
        empty = false;
        break;
      }
    }
    return empty;
  }
  inline uint16_t* get_commands() {
    uint16_t *commands = (uint16_t*) calloc(sizeof(uint16_t), _size+1);
    *(commands + 0) = SPI_COMMAND_MARKER;
    
    for(uint32_t i=0; i<_size; i++) {
      if( _buffer[i].size() > 0 ) {
        *(commands + _size - i) = _buffer[i].front();
        _buffer[i].pop();
      }
    }
    return commands;
  }
};

#endif // __ASPCOMMON_HPP
