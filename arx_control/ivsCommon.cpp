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
  bool success;
  uint8_t page = 17;
  int ntry = 0;
  while( page != module ) {
    ntry++;
    
    // Move to the correct module page
    success = atm->write_i2c(addr, 0x00, (char *) &module, 1);
    if( !success ) {
      if( ntry < IVS_MAX_RETRY_PAGE ) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      } else {
        return false;
      }
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    
    success = atm->read_i2c(addr, 0x00, (char *) &page, 1);
    if( !success ) {
      if( ntry < IVS_MAX_RETRY_PAGE ) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      } else {
        return false;
      }
    }
  }
  
  return true;
}

bool ivs_wait_not_busy(ATmega *atm, uint8_t addr, int timeout_ms) {
  uint8_t data;
  auto tstart = std::chrono::steady_clock::now();
  while (true) {
    if( !atm->read_i2c(addr, 0x78, (char*) &data, 1) ) {
      return false;
    }
    
    if( !((data >> 7) & 1) ) {// exit if we aren't busy
      return true;
    }
    
    auto tnow = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(tnow - tstart).count();
    if( elapsed > timeout_ms ) {
      return false;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
}
