/*****************************************************
configPSU - Change configuration of the specified PSU

Usage:
  configPSU <ATmega S/N> <device address> <command>
  
  * Device addresses are two-digit hexadecimal numbers 
    (i.e. 0x1F)
  * Valid commands are:
     query - get the current configuration of the PSU
     dump - get detailed register values showing the
            current configuration of the PSU
     autoOn - Automatically turn on the DC output on 
              power up
     autoOff - Automatically turn off the DC output 
               on power up
     tempWarm ##.# - Set the temperaure warning limit
                     to the specified value in degrees
                     C
     tempFault ##.# - Set the temperature fault limit
                      to the specified value in 
                      degrees C
     voltAdjust ##.# - Set the output voltage to the
                       specified value in volts
     turnOnDelay ### - Set the turn on delays to the
                       specifed value in ms

Options:
  None
*****************************************************/


#include <iostream>
#include <string>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <thread>

#include "libatmega.hpp"
#include "aspCommon.hpp"
#include "ivsCommon.hpp"

#define MODE_UNKOWN       0
#define MODE_QUERY      101
#define MODE_AUTOON     102
#define MODE_AUTOOFF    103
#define MODE_TEMPWARN   104
#define MODE_TEMPFAULT  105
#define MODE_VOLTADJUST 106
#define MODE_ONDELAY    107
#define MODE_DUMP       108

static std::string slots_to_string(uint16_t bits) {
  std::string out;
  for(int i=0; i<16; i++) {
    if( (bits >> i) & 1 ) {
      if( out.size() ) out += ",";
      out += std::to_string(i);
    }
  }
  if( out.empty() ) out = "(none)";
  return out;
}

int main(int argc, char** argv) {
  /*************************
  * Command line parsing   *
  *************************/
  // Make sure we have the right number of arguments to continue
  if( argc < 3+1 ) {
    std::cerr << "configPSU - Need at least 3 arguments, " << argc-1 << " provided" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  char *endptr;
  std::string requestedSN = std::string(argv[1]);
  uint32_t i2c_device = std::strtod(argv[2], &endptr);
  std::string command = std::string(argv[3]);
  float arg_value = 0.0;
  uint16_t mode = MODE_UNKOWN;
  if( command ==  "query" ) {
    mode = MODE_QUERY;
  } else if( command == "dump" ) {
    mode = MODE_DUMP;
  } else if( command == "autoOn" ) {
    mode = MODE_AUTOON;
  } else if( command == "autoOff" ) {
    mode = MODE_AUTOOFF;
  } else if( command == "tempWarn" ) {
    mode = MODE_TEMPWARN;
    if( argc < 4+1 ) {
      std::cerr << "configPSU - Setting 'tempWarn' requires an additional argument" << std::endl;
      std::exit(EXIT_FAILURE);
    }
    arg_value = std::strtod(argv[4], &endptr);
  } else if( command == "tempFault" ) {
    mode = MODE_TEMPFAULT;
    if( argc < 4+1 ) {
      std::cerr << "configPSU - Setting 'tempFault' requires an additional argument" << std::endl;
      std::exit(EXIT_FAILURE);
    }
    arg_value = std::strtod(argv[4], &endptr);
  } else if( command == "voltAdjust" ) {
    mode = MODE_VOLTADJUST;
    if( argc < 4+1 ) {
      std::cerr << "configPSU - Setting 'voltAdjust' requires an additional argument" << std::endl;
      std::exit(EXIT_FAILURE);
    }
    arg_value = std::strtod(argv[4], &endptr);
  } else if( command == "turnOnDelay" ) {
    mode = MODE_ONDELAY;
    if( argc < 4+1 ) {
      std::cerr << "configPSU - Setting 'turnOnDelay' requires an additional argument" << std::endl;
      std::exit(EXIT_FAILURE);
    }
    arg_value = std::strtod(argv[4], &endptr);
  } else {
    std::cerr << "configPSU - Invalid command '" << command << "'" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  /************************************
  * ATmega device selection and ready *
  ************************************/
  ATmega *atm = new ATmega(requestedSN);
  
  bool success = atm->open();
  if( !success ) {
    std::cerr << "configPSU - failed to open " << requestedSN << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  /********************
  * Read from the I2C *
  ********************/
  std::list<uint8_t> i2c_devices = atm->list_i2c_devices();
  
  uint16_t data;
  bool found = false;
  PSUSetup psu_setup;
  PSUConfig psu_config;
  CaseStatusByte case_status;
  CaseFaultByte case_fault;
  StatusByte status_byte;
  for(uint8_t& addr: i2c_devices) {
    if( addr != i2c_device ) {
      continue;
    }
    
    std::list<uint8_t> modules;
    if( (mode == MODE_QUERY) || (mode == MODE_VOLTADJUST) || (mode == MODE_ONDELAY) ) {
      // Get a list of smart modules tha we need to update
      modules = ivs_get_smart_modules(atm, addr);
    }
    
    if( mode != MODE_QUERY ) {
      // Enable writing to the OPERATION address (0x01) so we can change modules
      success = ivs_enable_all_writes(atm, addr);
      if( !success ) {
        std::cerr << "configPSU - write settings failed" << std::endl;
        continue;
      }
    }
    
    // Go!
    switch(mode) {
      case MODE_QUERY:
        // Querty the PSU setup
        success = atm->read_i2c(addr, IVS_PSU_SETUP, (char *) &psu_setup, 1);
        if( !success ) {
          std::cerr << "configPSU - get setup failed" << std::endl;
          continue;
        }

        switch (psu_setup.config_data) {
          case 3:
            std::cout << "Config. Data Source:       User" << std::endl;
            break;
          case 2:
            std::cout << "Config. Data Source:       Default" << std::endl;
            break;
          case 1:
            std::cout << "Config. Data Source:       Firmware" << std::endl;
            break;
          default:
            std::cout << "Config. Data Source:       Memory" << std::endl;
        }
        std::cout << "DC Used for Input:         " << (int) psu_setup.dc_input << std::endl;
        
        // Query the PSU configuation
        success = atm->read_i2c(addr, IVS_PSU_CONFIG, (char *) &psu_config, 1);
        if( !success ) {
          std::cerr << "configPSU - get configuration failed" << std::endl;
          continue;
        }
        std::cout << "Fan Alarm Disabled:        " << (int) psu_config.fan_alarm_disabled << std::endl;
        std::cout << "Fan Off at Standby:        " << (int) psu_config.fan_off_standby << std::endl;
        std::cout << "Fan Direction Reversed:    " << (int) psu_config.fan_reversed << std::endl;
        std::cout << "DC Output ON with Power:   " << (int) psu_config.startup_mode_on << std::endl;
        
        // Query temperature limits
        success = atm->read_i2c(addr, IVS_OT_WARN_LIMIT, (char *) &data, 2);
        if( !success ) {
          std::cerr << "configPSU - get temperature warning failed" << std::endl;
          continue;
        }
        std::cout << "Temperature Warning Limit: " << (float) data/4.0 << " C" << std::endl;
        
        success = atm->read_i2c(addr, IVS_OT_FAULT_LIMIT, (char *) &data, 2);
        if( !success ) {
          std::cerr << "configPSU - get temperature fault failed" << std::endl;
          continue;
        }
        std::cout << "Temperature Fault Limit:   " << (float) data/4.0 << " C" << std::endl;
        
        // Query power limits
        uint64_t wide_data;
        wide_data = 0;
        success = atm->read_i2c(addr, IVS_OVER_POWER_LIMITS, (char *) &wide_data, 5);
        if( !success ) {
          std::cerr << "configPSU - get power limits failed" << std::endl;
          continue;
        }
        std::cout << "Low Power Limit:           " << (int) ((wide_data >> 8) & 0xFFFF) << " W" << std::endl;
        std::cout << "High Power Limit:          " << (int) ((wide_data >> 24) & 0xFFFF) << " W" << std::endl;
        
        // Query turn on delay
        for(uint8_t& module: modules) {
          success = ivs_select_module(atm, addr, module);
          if( !success ) {
            std::cerr << "configPSU - page change failed" << std::endl;
            continue;
          }
          
          success = atm->read_i2c(addr, IVS_TON_DELAY, (char *) &data, 2);
          if( !success ) {
            std::cerr << "configPSU - get turn on delay failed" << std::endl;
            continue;
          }
          data &= 0xFF;
          std::cout << "Module " << module << " Turn On Delay: " << (int) data << " ms" << std::endl;
        }
        
        break;
        
      case MODE_DUMP:
        // OPERATION (0x01) - current on/off state (bit 7)
        data = 0;
        success = atm->read_i2c(addr, IVS_OPERATION, (char *) &data, 1);
        std::cout << "OPERATION     (0x01): " << (success ? "" : "[READ FAIL] ")
                  << "0x" << std::hex << (int) data << std::dec
                  << "  outputOn=" << ((data >> 7) & 1) << std::endl;

        // ON_OFF_CONFIG (0x02) - governs power-up behavior
        data = 0;
        success = atm->read_i2c(addr, IVS_ON_OFF_CONFIG, (char *) &data, 1);
        std::cout << "ON_OFF_CONFIG (0x02): " << (success ? "" : "[READ FAIL] ")
                  << "0x" << std::hex << (int) data << std::dec
                  << "  bit4(needCmdToStart)=" << ((data >> 4) & 1)
                  << " bit3(useOPERATION)=" << ((data >> 3) & 1)
                  << " bit2(useCONTROLpin)=" << ((data >> 2) & 1) << std::endl;

        // ACTIVE_SLOTS
        data = 0;
        success = atm->read_i2c(addr, IVS_ACTIVE_SLOTS, (char *) &data, 2);
        std::cout << "ACTIVE_SLOTS  (0xD2): " << (success ? "" : "[READ FAIL] ")
                  << "0x" << std::hex << data << std::dec
                  << "  b" << std::bitset<16>(data)
                  << "  slots {" << slots_to_string(data) << "}" << std::endl;

        // SMART_MODULES
        data = 0;
        success = atm->read_i2c(addr, IVS_SMART_MODULES, (char *) &data, 2);
        std::cout << "SMART_MODULES (0xD3): " << (success ? "" : "[READ FAIL] ")
                  << "0x" << std::hex << data << std::dec
                  << "  b" << std::bitset<16>(data)
                  << "  slots {" << slots_to_string(data) << "}" << std::endl;

        // PSU_CONFIG
        data = 0;
        success = atm->read_i2c(addr, IVS_PSU_CONFIG, (char *) &data, 1);
        psu_config = (uint8_t) (data & 0xFF);
        std::cout << "PSU_CONFIG    (0xD5): " << (success ? "" : "[READ FAIL] ")
                  << "0x" << std::hex << (int) data << std::dec
                  << "  StartupOn=" << psu_config.startup_mode_on << std::endl;

        // PSU_SETUP (config data source)
        data = 0;
        success = atm->read_i2c(addr, IVS_PSU_SETUP, (char *) &data, 1);
        psu_setup = (uint8_t) (data & 0xFF);
        const char *cfg_source;
        switch (psu_setup.config_data) {
          case 3: cfg_source = "User";     break;
          case 2: cfg_source = "Default";  break;
          case 1: cfg_source = "Firmware"; break;
          default: cfg_source = "Memory";
        }
        std::cout << "PSU_SETUP     (0xD6): " << (success ? "" : "[READ FAIL] ")
                  << "0x" << std::hex << (int) data << std::dec
                  << "  cfgDataSource=" << cfg_source
                  << "  DCinput=" << psu_setup.dc_input << std::endl;

        // CASE_STATUS_BYTE
        data = 0;
        success = atm->read_i2c(addr, IVS_CASE_STATUS_BYTE, (char *) &data, 1);
        case_status = (uint8_t) (data & 0xFF);
        std::cout << "CASE_STATUS   (0xD8): " << (success ? "" : "[READ FAIL] ")
                  << "0x" << std::hex << (int) data << std::dec
                  << "  PSon=" << case_status.ps_on
                  << " GlobalDCok=" << case_status.global_dc_ok
                  << " Bulkok=" << case_status.bulk_ok
                  << " ACok=" << case_status.ac_ok << std::endl;

        // CASE_FAULT_BYTE  <-- corruption flags live here
        data = 0;
        success = atm->read_i2c(addr, IVS_CASE_FAULT_BYTE, (char *) &data, 1);
        case_fault = (uint8_t) (data & 0xFF);
        std::cout << "CASE_FAULT    (0xD9): " << (success ? "" : "[READ FAIL] ")
                  << "0x" << std::hex << (int) data << std::dec << "  ";
        if( data == 0 ) std::cout << "(no faults latched)";
        else {
          if( case_fault.case_otp ) std::cout << "CaseOTP ";
          if( case_fault.case_otw ) std::cout << "CaseOTW ";
          if( case_fault.primary_otw ) std::cout << "PrimaryOTW ";
          if( case_fault.over_power_fault ) std::cout << "OverPower ";
          if( case_fault.user_config_error ) std::cout << "*USER_CFG_CORRUPT* ";
          if( case_fault.default_config_error ) std::cout << "*DEFAULT_CFG_CORRUPT* ";
          if( case_fault.primary_otp ) std::cout << "PrimaryOTP ";
          if( case_fault.command_error ) std::cout << "CommandError ";
        }
        std::cout << std::endl;

        // MODULE_COMMUNICATION_ERROR_WORD
        data = 0;
        success = atm->read_i2c(addr, IVS_MOD_COMM_ERR_WORD, (char *) &data, 2);
        std::cout << "MOD_COMM_ERR  (0xDA): " << (success ? "" : "[READ FAIL] ")
                  << "0x" << std::hex << data << std::dec
                  << "  failed-comm slots {" << slots_to_string(data) << "}" << std::endl;

        // OUTPUT_INDEX (block read)
        uint8_t blk[4];
        std::memset(blk, 0, sizeof(blk));
        success = atm->read_i2c(addr, IVS_OUTPUT_INDEX, (char *) blk, 4);
        std::cout << "OUTPUT_INDEX  (0xEC): " << (success ? "" : "[READ FAIL] ")
                  << "count=" << (int) blk[0]
                  << " index=" << (int) blk[1]
                  << " smartMods=0x" << std::hex << (int) (blk[2] | (blk[3] << 8)) << std::dec
                  << " slots {" << slots_to_string(blk[2] | (blk[3] << 8)) << "}" << std::endl;

        // Per-slot probe.  Unlock writes so we can change PAGE (as readPSU does).
        if( !ivs_enable_all_writes(atm, addr) ) {
          std::cerr << "configPSU - could not unlock to page modules" << std::endl;
        } else {
          std::cout << "--- per-slot module probe (paging 0..15) ---" << std::endl;
          for(uint8_t slot=0; slot<16; slot++) {
            uint8_t page = 0xFF;
            atm->write_i2c(addr, IVS_PAGE, (char *) &slot, 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
            if( !atm->read_i2c(addr, IVS_PAGE, (char *) &page, 1) || page != slot ) {
              continue;  // page did not take -> treat as not addressable
            }

            // Trigger + read module version/type
            uint8_t trig = 0;
            atm->write_i2c(addr, IVS_EXTRACT_MOD_VER, (char *) &trig, 1);
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
            ModuleVersion module_ver;
            bool vok = atm->read_i2c(addr, IVS_READ_MOD_VER, (char *) &module_ver, 4);

            uint8_t status = 0;
            bool sok = atm->read_i2c(addr, IVS_MODULE_STATUS, (char *) &status, 1);

            uint8_t pvcode = module_ver.power_voltage;
            bool looks_present = vok && (module_ver.three == 3);
            std::cout << "  slot " << (int) slot << ": ";
            if( looks_present ) {
              std::cout << "module fw " << (int) module_ver.firmware_major << "." << (int) module_ver.firmware_minor
                        << "  type " << ivs_decode_module_power(pvcode) << "_" << ivs_decode_module_voltage(pvcode)
                        << " (pvcode=0x" << std::hex << (int) pvcode << std::dec << ")";
            } else {
              std::cout << "no valid module version response";
            }
            if( sok ) {
              std::cout << "  status=0x" << std::hex << (int) status << std::dec
                        << (((status >> 0) & 1) ? " OutEn" : "")
                        << (((status >> 1) & 1) ? " UVP" : "")
                        << (((status >> 7) & 1) ? " SysFault" : "");
            }
            std::cout << std::endl;
          }

          // Restore page 0 and re-protect
          uint8_t zero = 0;
          atm->write_i2c(addr, IVS_PAGE, (char *) &zero, 1);
          ivs_disable_writes(atm, addr);
        }
        
        break;
        
      case MODE_AUTOOFF:
        // Query the PSU configuation
        success = atm->read_i2c(addr, IVS_PSU_CONFIG, (char *) &psu_config, 1);
        if( !success ) {
          std::cerr << "configPSU - get configuration failed" << std::endl;
          continue;
        }

        // Update the default operation flag
        psu_config.startup_mode_on = false;

        // Write the PSU configuation
        success = atm->write_i2c(addr, IVS_PSU_CONFIG, (char *) &psu_config, 1);
        if( !success ) {
          std::cerr << "configPSU - set configuration failed" << std::endl;
          continue;
        }
        
        // Save the configutation as default
        data = 0x21;
        success = atm->write_i2c(addr, IVS_STORE_USER_ALL, (char *) &data, 1);
        if( !success ) {
          std::cerr << "configPSU - save configuration failed" << std::endl;
          continue;
        }
        break;
        
      case MODE_AUTOON:
        // Query the PSU configuation
        success = atm->read_i2c(addr, IVS_PSU_CONFIG, (char *) &psu_config, 1);
        if( !success ) {
          std::cerr << "configPSU - get configuration failed" << std::endl;
          continue;
        }

        // Update the default operation flag
        psu_config.startup_mode_on = true;

        // Write the PSU configuation
        success = atm->write_i2c(addr, IVS_PSU_CONFIG, (char *) &psu_config, 1);
        if( !success ) {
          std::cerr << "configPSU - set configuration failed" << std::endl;
          continue;
        }
        
        // Save the configutation as default
        data = 0x21;
        success = atm->write_i2c(addr, IVS_STORE_USER_ALL, (char *) &data, 1);
        if( !success ) {
          std::cerr << "configPSU - save configuration failed" << std::endl;
          continue;
        }
        break;
        
      case MODE_TEMPWARN:
        // Convert to the right format
        data = (uint16_t) round(arg_value*4);
        
        // Write to memory
        success = atm->write_i2c(addr, IVS_OT_WARN_LIMIT, (char *) &data, 2);
        if( !success ) {
          std::cerr << "configPSU - set temperature warning failed" << std::endl;
          continue;
        }
        
        // Save the configutation as default
        data = 0x21;
        success = atm->write_i2c(addr, IVS_STORE_USER_ALL, (char *) &data, 1);
        if( !success ) {
          std::cerr << "configPSU - save configuration failed" << std::endl;
          continue;
        }
        break;
        
      case MODE_TEMPFAULT:
        // Convert to the right format
        data = (uint16_t) round(arg_value*4);
        
        // Write to memory
        success = atm->write_i2c(addr, IVS_OT_FAULT_LIMIT, (char *) &data, 2);
        if( !success ) {
          std::cerr << "configPSU - set temperature warning failed" << std::endl;
          continue;
        }
        
        // Save the configutation as default
        data = 0x21;
        success = atm->write_i2c(addr, IVS_STORE_USER_ALL, (char *) &data, 1);
        if( !success ) {
          std::cerr << "configPSU - save configuration failed" << std::endl;
          continue;
        }
        break;
        
      case MODE_VOLTADJUST:
      case MODE_ONDELAY:
        // Loop over modules
        for(uint8_t& module: modules) {
          success = ivs_select_module(atm, addr, module);
          if( !success ) {
            std::cerr << "configPSU - page change failed" << std::endl;
            continue;
          }
          
          if( mode == MODE_VOLTADJUST ) {
            // Read what this module is capable of
            data = 0;
            success = atm->write_i2c(addr, IVS_EXTRACT_MOD_VER, (char *) &data, 1);
            if( !success ) {
              std::cerr << "configPSU - get module info failed" << std::endl;
              continue;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            
            ModuleVersion module_ver;
            success = atm->read_i2c(addr, IVS_READ_MOD_VER, (char *) &module_ver, 4);
            if( !success ) {
              std::cerr << "configPSU - get module info failed" << std::endl;
              continue;
            }

            int modulevolts = 0;  // The only valid options are the 8V and 15V modules
            if( (module_ver.power_voltage & 15) == 1 ) {// 6V to 12V
              modulevolts = 8;
            } else if( (module_ver.power_voltage & 15) == 2 ) {// 14V to 20V
              modulevolts = 15;
            } else if( (module_ver.power_voltage & 15) == 7 ) {// 12V to 15V
              modulevolts = 15;
            }
            
            // Verify module compatibility
            if( (arg_value < (0.9*modulevolts)) || (arg_value > (1.1*modulevolts)) ) {
              std::cerr << "configPSU - requested voltage outside module range, skipping" << std::endl;
              continue;
            }
            
            // Convert to the right format
            data = (uint16_t) round(arg_value*100);
            
            // Update the output voltage
            success = atm->write_i2c(addr, IVS_VOUT_COMMAND, (char *) &data, 2);
            if( !success ) {
              std::cerr << "configPSU - output voltage update failed" << std::endl;
              continue;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            // Verify the module status
            success = atm->read_i2c(addr, IVS_STATUS_BYTE, (char *) &status_byte, 1);
            if( !success ) {
              std::cerr << "configPSU - get output voltage failed" << std::endl;
              continue;
            }
            if( status_byte.in_unexpected_state() ) {
              std::cerr << "configPSU - module in unexpected state:" << std::endl;
              if( status_byte.busy ) {// busy
                std::cerr << "            busy" << std::endl;
              }
              if( status_byte.vout_ov ) {// overvoltage
                std::cerr << "            output overvoltage" << std::endl;
              }
              if( status_byte.iout_oc ) {// overcurrent
                std::cerr << "            output overcurrent" << std::endl;
              }
              if( status_byte.vin_uv ) {// undervoltage
                std::cerr << "            input undervoltage" << std::endl;
              }
              if( status_byte.temperature ) {// temperature
                std::cerr << "            temperature" << std::endl;
              }
              if( status_byte.cml ) {// comms, memory, or logic
                std::cerr << "            comm/mem/logic" << std::endl;
              }
              if( status_byte.other ) {// other
                std::cerr << "            other" << std::endl;
              }
            }
            
          } else if( mode == MODE_ONDELAY) {
            if( (arg_value < 0) || (arg_value > 255) ) {
              std::cerr << "configPSU - requested turn on delay outside range, skipping" << std::endl;
              continue;
            }
            
            // Convert to the right format
            data = (uint16_t) round(arg_value);
            
            // Update the turn on delay
            success = atm->write_i2c(addr, IVS_TON_DELAY, (char *) &data, 2);
            if( !success ) {
              std::cerr << "configPSU - turn on delay update failed" << std::endl;
              continue;
            }
          }
        }
        
        // Save the configutation as default
        data = 0x21;
        success = atm->write_i2c(addr, IVS_STORE_USER_ALL, (char *) &data, 1);
        if( !success ) {
          std::cerr << "configPSU - save configuration failed" << std::endl;
          continue;
        }
        break;
      
      default:
        break;
    }
    
    if( mode == MODE_VOLTADJUST ) {// Clear faults after changing the voltage
      data = 0;
      success = atm->write_i2c(addr, IVS_CLEAR_FAULTS, (char *) &data, 1);
      if( !success ) {
        std::cerr << "configPSU - clear faults failed" << std::endl;
        continue;
      }
    }
    
    if( mode != MODE_QUERY ) {// Write-protect all entries but WRITE_PROTECT (0x10)
      success = ivs_disable_writes(atm, addr);
      if( !success ) {
        std::cerr << "configPSU - write settings failed" << std::endl;
        continue;
      }
    }
    
    // Mark that we have sone something
    found = true;
  }
  
  /*******************
  * Cleanup and exit *
  *******************/
  delete atm;
  
  if( !found ) {
    std::cerr << "configPSU - Cannot find device at address " << std::uppercase << std::hex << "0x" << i2c_device << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  std::exit(EXIT_SUCCESS);
}
