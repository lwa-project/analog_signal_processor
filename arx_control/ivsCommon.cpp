#include <vector>
#include <thread>
#include <chrono>

#include "aspCommon.hpp"
#include "ivsCommon.hpp"

std::list<uint8_t> ivs_get_active_slots(ATmega *atm, uint8_t addr) {
  std::list<uint8_t> slots;
  uint16_t data;
  bool success = atm->read_i2c(addr, IVS_ACTIVE_SLOTS, (char *) &data, 2);
  if( success ) {
    for(uint8_t i=0; i<16; i++) {
      if( (data >> i) & 1 ) {
        slots.push_back(i);
      }
    }
  }
  
  return slots;
}

std::list<uint8_t> ivs_get_smart_modules(ATmega *atm, uint8_t addr) {
  std::list<uint8_t> modules;
  uint16_t data;
  bool success = atm->read_i2c(addr, IVS_SMART_MODULES, (char *) &data, 2);
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
    success = atm->write_i2c(addr, IVS_PAGE, (char *) &module, 1);
    if( !success ) {
      if( ntry < IVS_MAX_RETRY_PAGE ) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        continue;
      } else {
        return false;
      }
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
    
    success = atm->read_i2c(addr, IVS_PAGE, (char *) &page, 1);
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
    if( !atm->read_i2c(addr, IVS_STATUS_BYTE, (char*) &data, 1) ) {
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

std::string ivs_decode_module_power(uint8_t module_ver) {
  switch((module_ver >> 4 ) & 0xF) {
    case 0: return std::string("210W");  break;
    case 1: return std::string("360W");  break;
    case 2: return std::string("144W");  break;
    case 3: return std::string("600W");  break;
    case 4: return std::string("750W");  break;
    case 5: return std::string("1500W"); break;
    default: return std::string("UNK");
  }
}

std::string ivs_decode_module_voltage(uint8_t module_ver) {
  switch(module_ver & 0xF) {
    case  0: return std::string("2to5.5V"); break;
    case  1: return std::string("6to12V");  break;
    case  2: return std::string("14to20V"); break;
    case  3: return std::string("24to36V"); break;
    case  4: return std::string("42to60V"); break;
    case  5: return std::string("fixed5V"); break;
    case  6: return std::string("2to6V");   break;
    case  7: return std::string("12to15V"); break;
    case  8: return std::string("24to28V"); break;
    case  9: return std::string("24to30V"); break;
    case 10: return std::string("33to60V"); break;
    default: return std::string("UNK");
  }
}
