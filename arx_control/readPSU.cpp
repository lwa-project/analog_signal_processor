/*****************************************************
readPSUs - Program to pull information about all power
supplies found on the I2C bus.  The data polled 
includes:
 * on/off status
 * general module health (DC OK, over current, etc.)
 * output voltage
 * output current
 
Usage:
  readPSUs <ATmega S/N> <I2C address>

Options:
  None
*****************************************************/

#include <iostream>
#include <string>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <thread>

#include "libatmega.hpp"
#include "aspCommon.hpp"
#include "ivsCommon.hpp"

std::string getModuleName(uint16_t module, uint8_t moduleCode) {
  // Decode the power rating of the current module
  std::string output = std::string("Module")+std::to_string(module);
  output += std::string("_") + ivs_decode_module_power(moduleCode);
    
  // Decode the voltage range of the current module
  output += std::string("_") + ivs_decode_module_voltage(moduleCode);
  
  return output;
}


std::string getModulePower(uint8_t statusCode) {
  // Decode the output enabled flag
  std::string output;
  if( statusCode & 1 ) {
    output = std::string("ON");
  } else {
    output = std::string("OFF");
  }
  
  return output;
}


std::string getModuleStatus(uint8_t statusCode) {
  // Decode the various status and fault flags
  ModuleStatusFlags status = statusCode;
  std::string output;
  if( status.uvp_fault ) {
    output = std::string("UnderVolt");
  }
  if( status.dc_ok ) {
    if( output.size() > 0 ) {
      output = output+std::string("&");
    }
    output = output+std::string("OK");
  }
  if( status.ocp_fault ) {
    if( output.size() > 0 ) {
      output = output+std::string("&");
    }
    output = output+std::string("OverCurrent");
  }
  if( status.otp_fault ) {
    if( output.size() > 0 ) {
      output = output+std::string("&");
    }
    output = output+std::string("OverTemperature");
  }
  if( status.otp_warning ) {
    if( output.size() > 0 ) {
      output = output+std::string("&");
    }
    output = output+std::string("WarningTemperature");
  }
  if( status.ovp_fault ) {
    if( output.size() > 0 ) {
      output = output+std::string("&");
    }
    output = output+std::string("OverVolt");
  }
  if( status.system_fault ) {
    if( output.size() > 0 ) {
      output = output+std::string("&");
    }
    output = output+std::string("ModuleFault");
  }
  
  // Make sure we return something...
  if( output.size() == 0 ) {
    output = std::string("UNK");
  }
  
  return output;
}


int main(int argc, char** argv) {
  /*************************
  * Command line parsing   *
  *************************/
  // Make sure we have the right number of arguments to continue
  if( argc < 2+1 ) {
    std::cout << "readPSU - Need 2 arguments, " << argc-1 << " provided" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  char *endptr;
  std::string requestedSN = std::string(argv[1]);
  uint32_t i2c_device = std::strtod(argv[2], &endptr);
  
  /************************************
  * ATmega device selection and ready *
  ************************************/
  ATmega *atm = new ATmega(requestedSN);
  
  bool success = atm->open();
  if( !success ) {
    std::cout << "readPSU - failed to open " << requestedSN << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  /********************
  * Read from the I2C *
  ********************/
  std::list<uint8_t> i2c_devices = atm->list_i2c_devices();
  
  uint16_t data;
  bool found = false;
  for(uint8_t& addr: i2c_devices) {
    if( addr != i2c_device ) {
      continue;
    }
    
    // Get a list of smart modules for polling
    std::list<uint8_t> modules = ivs_get_smart_modules(atm, addr);
    
    // Enable writing to all of the supported command so we can change 
    // modules/poll module type
    success = ivs_enable_all_writes(atm, addr);
    if( !success ) {
      std::cout << "readPSU - write settings failed" << std::endl;
      continue;
    }

    // Loop over modules 0 through 15
    int nMod = 0;
    std::string moduleName, modulePower, moduleStatus;
    float voltage = 0.0, current = 0.0;
    for(uint8_t& module: modules) {
      success = ivs_select_module(atm, addr, module);
      if( !success ) {
        std::cerr << "readPSU - page change failed" << std::endl;
        continue;
      }
      nMod++;
      
      /***********************
      * Module Name and Type *
      ***********************/
      
      #ifdef __DECODE_MODULE_TYPE__
        ModuleVersion module_ver;
        data = 0;
        success = atm->write_i2c(addr, IVS_EXTRACT_MOD_VER, (char *) &data, 1);
        if( !success ) {
          std::cout << "readPSU - get type failed" << std::endl;
          delete atm;
          std::exit(EXIT_FAILURE);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        // Read the version bytes straight into the struct so the byte members
        // (which carry no endianness) line up with the I2C wire order
        success = atm->read_i2c(addr, IVS_READ_MOD_VER, (char *) &module_ver, 4);
        if( !success ) {
          std::cout << "readPSU - get type failed" << std::endl;
          continue;
        }
        if( moduleName.size() > 0 ) {
          moduleName = moduleName+std::string("|");
        }
        moduleName = moduleName+getModuleName(module, module_ver.power_voltage);
      #else
        if( moduleName.size() > 0 ) {
          moduleName = moduleName+std::string("|");
        }
        moduleName = moduleName+std::string("Module")+std::to_string(module);
      #endif

      /*************************
      * Power State and Status *
      *************************/
      
      data = 0;
      success = atm->read_i2c(addr, IVS_MODULE_STATUS, (char *) &data, 1);
      if( !success ) {
        std::cout << "readPSU - get status failed" << std::endl;
        continue;
      }
      data &= 0xFF;
      modulePower = getModulePower((uint8_t) data);
      if( moduleStatus.size() > 0 ) {
        moduleStatus = moduleStatus+std::string("|");
      }
      moduleStatus = moduleStatus+getModuleStatus((uint8_t) data);

      /*****************
      * Output Voltage *
      *****************/
      
      success = atm->read_i2c(addr, IVS_READ_VOUT, (char *) &data, 2);
      if( !success ) {
        std::cout << "readPSU - get output voltage failed" << std::endl;
        continue;
      }
      voltage += (float) data /100.0;

      /*****************
      * Output Current *
      *****************/
      
      #ifdef __USE_INPUT_CURRENT__
        success = atm->read_i2c(addr, IVS_READ_IIN, (char *) &data, 2);
        if( !success ) {
          std::cout << "readPSU - get input current failed" << std::endl;
          continue;
        }
        current = (float) data /100.0 * 0.95;    // Removes the ~5% power conversion loss
      #else
        success = atm->read_i2c(addr, IVS_READ_IOUT, (char *) &data, 2);
        if( !success ) {
          std::cout << "readPSU - get output current failed" << std::endl;
          continue;
        }
        current += (float) data /100.0;
      #endif
    }
    
    // Print mean output voltage and the total current
    if( nMod != 0 ) {
      voltage /= (float) nMod;
    }
    std::cout << std::uppercase << std::hex << "0x" << (int) addr << std::nouppercase << std::dec 
              << " " << moduleName << " " << modulePower << " " << moduleStatus
              << " " << voltage << " " << current << std::endl;
    
    // Set the module number back to 0
    data = 0;
    success = atm->write_i2c(addr, IVS_PAGE, (char *) &data, 1);
    if( !success ) {
      std::cout << "readPSU - page change failed" << std::endl;
      continue;
    }

    // Write-protect all entries but WRITE_PROTECT (0x10)
    success = ivs_disable_writes(atm, addr);
    if( !success ) {
      std::cout << "readPSU - write settings failed" << std::endl;
      continue;
    }
    
    // Mark that we have sone something
    found = true;
  }
  
  /*******************
  * Cleanup and exit *
  *******************/
  delete atm;
  
  if( !found ) {
    std::cout << "readPSU - Cannot find device at address " << std::uppercase << std::hex << "0x" << i2c_device << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  std::exit(EXIT_SUCCESS);
}
