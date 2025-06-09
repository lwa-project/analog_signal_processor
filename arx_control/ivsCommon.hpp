#ifndef __IVSCOMMON_HPP
#define __IVSCOMMON_HPP

/*
  ivsCommon.hpp - Header library with helper functions to work with iVS power
  supplies over I2C using an ATmega device
*/

#include <vector>
#include <chrono>
#include <thread>

#include "aspCommon.hpp"


#define IVS_MAX_RETRY_PAGE 3

// Disable writes
inline bool ivs_disable_writes(ATmega *atm, uint8_t addr) {
  uint8_t data = ((1 << 7) & 1);
  bool success = atm->write_i2c(addr, 0x10, (char *) &data, 1);
  if( !success ) {
    return false;
  }
  
  return true;
}

// Enable all writes
inline bool ivs_enable_all_writes(ATmega *atm, uint8_t addr) {
  uint8_t data = 0;
  bool success = atm->write_i2c(addr, 0x10, (char *) &data, 1);
  if( !success ) {
    return false;
  }
  
  return true;
}

// Enable operation and page writes
inline bool ivs_enable_operation_page_writes(ATmega *atm, uint8_t addr) {
  uint8_t data = (1 << 6) | 1;
  bool success = atm->write_i2c(addr, 0x10, (char *) &data, 1);
  if( !success ) {
    return false;
  }
  
  return true;
}

// Get whether or not the unit is outputting DC
inline bool ivs_is_on(ATmega *atm, uint8_t addr) {
  uint8_t data;
  bool success = atm->read_i2c(addr, 0x01, (char *) &data, 1);
  if( !success ) {// Interesting case: if we can't tell we say it's on to be safe
    return true;
  }
  
  return (data >> 7) & 1;
}

// Get a list of smart modules
std::list<uint8_t> ivs_get_smart_modules(ATmega *atm, uint8_t addr);

// Set the current page to the requested module
bool ivs_select_module(ATmega *atm, uint8_t addr, uint8_t module);

#endif
