#include <vector>
#include <thread>
#include <chrono>

#include "aspCommon.hpp"
#include "ivsCommon.hpp"

std::list<uint8_t> ivs_get_smart_modules(ATmega *atm, uint8_t addr) {
  std::list<uint8_t> modules;
  uint16_t data;
  bool success = atm->read_i2c(addr, 0xD3, (char *) &data, 2);
  if( success ) {
    for(uint8_t i=0; i<16; i++) {
      if( (data >> i) & 1 ) {
        modules.push_back(i);
      }
    }
  }
  
  return modules;
}

bool ivs_select_module(ATmega *atm, uint8_t addr, uint8_t module) {
  uint16_t data;
  bool success;
  uint8_t page = 17;
  while( page != module ) {
    // Move to the correct module page
    data = module;
    success = atm->write_i2c(addr, 0x00, (char *) &data, 1);
    if( !success ) {
      return false;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    
    success = atm->read_i2c(addr, 0x00, (char *) &data, 1);
    if( !success ) {
      return false;
    }
    page = data & 0xFF;
  }

  return true;
}
