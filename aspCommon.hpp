#ifndef __ASPCOMMON_HPP
#define __ASPCOMMON_HPP

/*
  aspCommon.h - Header library with common values needed for using the SUB-20
  device with the SPI and I2C buses.
*/

#include <string>
#include <vector>
#include <queue>
#include <list>
#include <cstring>

#include "libsub.h"

// SUB-20 device opening control
#define SUB20_OPEN_MAX_ATTEMPTS 20
#define SUB20_OPEN_WAIT_MS  5


// SPI bus configuration settings
#define ARX_SPI_CONFIG SPI_ENABLE|SPI_CPOL_FALL|SPI_SETUP_SMPL|SPI_MSB_FIRST|SPI_CLK_500KHZ
#define TRANS_SPI_INTERMEDIATE SS_CONF(0, SS_L)
#define TRANS_SPI_FINAL SS_CONF(0, SS_LO)


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

// Get a list of all SUB-20 serial numbers
std::list<std::string> list_sub20s();


// Class to simplify interfacing with a SUB-20 via the libsub library
class Sub20 {
private:
  std::string _sn;
  sub_handle  _fh;
  bool        _spi_ready;
  
  inline bool enable_spi() {
    int status, j=0;
    status = sub_spi_config(_fh, 0, &j);
    if( !status ) {
      status = sub_spi_config(_fh, ARX_SPI_CONFIG, NULL);
  	}
    _spi_ready = (status == 0);
    return _spi_ready;
  }
public:
  Sub20(std::string sn): _sn(""), _fh(NULL), _spi_ready(false) {
    _sn = sn;
  }
  ~Sub20() {
    if( _fh != nullptr ) {
      sub_close(_fh);
    }
  }
  bool open();
  inline bool transfer_spi(char* inputs, char* outputs, int size) {
    int status;
    bool success = _spi_ready;
    if( !success ) {
      success = this->enable_spi();
    }
    
    status = 1;
    if( success ) {
      status = sub_spi_transfer(_fh, inputs, outputs, size, SS_CONF(0, SS_LO));
    }
    return (status == 0);
  }
  std::list<uint8_t> list_i2c_devices();
  inline bool read_i2c(uint8_t addr, uint8_t reg, char* data, int size) {
    int status;
    status = sub_i2c_read(_fh, addr, reg, 1, data, size);
    return (status == 0);
  }
  inline bool write_i2c(uint8_t addr, uint8_t reg, char* data, int size) {
    int status;
    status = sub_i2c_write(_fh, addr, reg, 1, data, size);
    return (status == 0);
  }
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
