#ifndef __IVSCOMMON_HPP
#define __IVSCOMMON_HPP

/*
  ivsCommon.hpp - Header library with helper functions to work with iVS power
  supplies over I2C using an ATmega device
*/

#include <vector>
#include <chrono>
#include <thread>
#include <string>

#include "aspCommon.hpp"


#define IVS_MAX_RETRY_PAGE 3


// Commands
#define IVS_PAGE              0x00
#define IVS_OPERATION         0x01
#define IVS_ON_OFF_CONFIG     0x02
#define IVS_CLEAR_FAULTS      0x03
#define IVS_WRITE_PROTECT     0x10
#define IVS_STORE_USER_ALL    0x15
#define IVS_VOUT_COMMAND      0x21
#define IVS_OT_FAULT_LIMIT    0x4F
#define IVS_OT_WARN_LIMIT     0x51
#define IVS_TON_DELAY         0x60
#define IVS_STATUS_BYTE       0x78
#define IVS_READ_VIN          0x88
#define IVS_READ_IIN          0x89
#define IVS_READ_VOUT         0x8B
#define IVS_READ_IOUT         0x8C
#define IVS_READ_TEMP_1       0x8D
#define IVS_READ_TEMP_2       0x8E
#define IVS_READ_TEMP_3       0x8F
#define IVS_ACTIVE_SLOTS      0xD2
#define IVS_SMART_MODULES     0xD3
#define IVS_PSU_CONFIG        0xD5
#define IVS_PSU_SETUP         0xD6
#define IVS_CASE_STATUS_BYTE  0xD8
#define IVS_CASE_FAULT_BYTE   0xD9
#define IVS_MOD_COMM_ERR_WORD 0xDA
#define IVS_MODULE_STATUS     0xDB
#define IVS_EXTRACT_MOD_VER   0xDE
#define IVS_READ_MOD_VER      0xDF
#define IVS_OVER_POWER_LIMITS 0xEB
#define IVS_OUTPUT_INDEX      0xEC


// Data structures
typedef struct __attribute__((packed)) _StatusByte {
  union {
    uint8_t status;
    struct {
      bool other:1;         // Another fault or warning that isn't covered by the other bits
      bool cml:1;           // Communication, memory, or logic fault
      bool temperature:1;   // Temperature fault or warning
      bool vin_uv:1;        // Input over voltage fault
      bool iout_oc:1;       // Output over current fault
      bool vout_ov:1;       // Output over voltage fault
      bool off:1;           // The unit is not providing power, regardless of the reason
      bool busy:1;          // The device is busy
    };
  };
  
  _StatusByte(uint8_t status=0) : status(status) {}

  // True if the module is busy or reporting any fault; the 'off' bit on its
  // own is a normal idle state and is deliberately ignored here
  bool in_unexpected_state() const {
    return other || cml || temperature || vin_uv || iout_oc || vout_ov || busy;
  }

} StatusByte;

typedef struct __attribute__((packed)) _PSUSetup {
  union {
    uint8_t setup;
    struct {
      uint8_t config_data:2;    // Which configuration data was loaded
      bool unused0:1;
      bool dc_input:1;          // Whether or not the input voltage is DC
      uint8_t unused1:4;
    };
  };
  
  _PSUSetup(uint8_t setup=0) : setup(setup) {}
  
} PSUSetup;

typedef struct __attribute__((packed)) _PSUConfig {
  union {
    uint8_t config;
    struct {
      bool fan_alarm_disabled:1;    // Fan fault assertion disabled
      bool fan_off_standby:1;       // Fans off during standby
      bool fan_reversed:1;          // Whether or not the fans have reversed airflow
      uint8_t unused0:4;
      bool startup_mode_on:1;       // Whether or not the initial state is ON
    };
  };
  
  _PSUConfig(uint8_t config=0) : config(config) {}
  
} PSUConfig;

typedef struct __attribute__((packed)) _CaseStatusByte {
  union {
    uint8_t status;
    struct {
      bool inhibit_enable0:1;     // Control signal state 1
      bool inhibit_enable1:1;     // Control signal state 2
      bool ac_ok:1;               // Input AC state
      bool bulk_ok:1;             // Bulk voltage state
      bool global_dc_ok:1;        // State of all module outputs
      bool fan1_ok:1;             // Fan 1 state
      bool fan2_ok:1;             // Fan 2 state
      bool ps_on:1;               // Power supply operating state
    };
  };
  
  _CaseStatusByte(uint8_t status=0) : status(status) {}
  
} CaseStatusByte;

typedef struct __attribute__((packed)) _CaseFaultByte {
  union {
    uint8_t faults;
    struct {
      bool case_otp:1;                // Case over temperature limit reached
      bool case_otw:1;                // Case temperature warning reached
      bool primary_otw:1;             // Primary over temperature warning reached
      bool over_power_fault:1;        // Smart module power limit reached
      bool user_config_error:1;       // User config memory corrupted
      bool default_config_error:1;    // Default config memory corrupted
      bool primary_otp:1;             // Primary over temperature limit reached
      bool command_error:1;           // General command error
    };
  };
  
  _CaseFaultByte(uint8_t faults=0) : faults(faults) {}
  
} CaseFaultByte;

typedef struct __attribute__((packed)) _ModuleStatusFlag {
  union {
    uint8_t flags;
    struct {
      bool output_enabled:1;    // Module output enabled
      bool uvp_fault:1;         // Module undervoltage fault
      bool dc_ok:1;             // Wheter or not a module's output is regulated
      bool ocp_fault:1;         // Module overcurrent fault
      bool otp_fault:1;         // Module over temperature fault
      bool otp_warning:1;       // Module temperature warning reached
      bool ovp_fault:1;         // Module over voltage fault
      bool system_fault:1;      // General module fault
    };
  };
  
  _ModuleStatusFlag(uint8_t flags=0) : flags(flags) {}
  
} ModuleStatusFlags;

typedef struct __attribute__((packed)) _ModuleVersion {
  union {
    uint32_t long_word;
    struct {
      uint8_t three;
      uint8_t firmware_major;
      uint8_t firmware_minor;
      uint8_t power_voltage;
    };
  };
  
  _ModuleVersion(uint32_t long_word=0) : long_word(long_word) {}
  
} ModuleVersion;


// Disable writes
inline bool ivs_disable_writes(ATmega *atm, uint8_t addr) {
  uint8_t data = (1 << 7) | 1;
  bool success = atm->write_i2c(addr, IVS_WRITE_PROTECT , (char *) &data, 1);
  if( !success ) {
    return false;
  }
  
  return true;
}

// Enable all writes
inline bool ivs_enable_all_writes(ATmega *atm, uint8_t addr) {
  uint8_t data = 0;
  bool success = atm->write_i2c(addr, IVS_WRITE_PROTECT , (char *) &data, 1);
  if( !success ) {
    return false;
  }
  
  return true;
}

// Enable operation and page writes
inline bool ivs_enable_operation_page_writes(ATmega *atm, uint8_t addr) {
  uint8_t data = (1 << 6) | 1;
  bool success = atm->write_i2c(addr, IVS_WRITE_PROTECT , (char *) &data, 1);
  if( !success ) {
    return false;
  }
  
  return true;
}

// Get whether or not the unit is outputting DC
inline bool ivs_is_on(ATmega *atm, uint8_t addr) {
  uint8_t data;
  bool success = atm->read_i2c(addr, IVS_OPERATION, (char *) &data, 1);
  if( !success ) {// Interesting case: if we can't tell we say it's on to be safe
    return true;
  }
  
  return (data >> 7) & 1;
}

// Get a list of active slots (smart or otherwise)
std::list<uint8_t> ivs_get_active_slots(ATmega *atm, uint8_t addr);

// Get a list of smart modules
std::list<uint8_t> ivs_get_smart_modules(ATmega *atm, uint8_t addr);

// Set the current page to the requested module
bool ivs_select_module(ATmega *atm, uint8_t addr, uint8_t module);

// Wait for the busy flag to clear out of the STATUS_BYTE
bool ivs_wait_not_busy(ATmega *atm, uint8_t addr, int timeout_ms=1000);

// Decode the module power code in IVS_READ_MOD_VER
std::string ivs_decode_module_power(uint8_t module_ver);

// Decode the module voltage code in IVS_READ_MOD_VER
std::string ivs_decode_module_voltage(uint8_t module_ver);

#endif
