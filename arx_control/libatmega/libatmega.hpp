#pragma once

#include <string>
#include <cstring>
#include <stdexcept>
#include <list>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#if defined(__APPLE__) && __APPLE__

#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/serial/IOSerialKeys.h>

#else

#include <libudev.h>

#endif

#define ATMEGA_MAX_BUFFER_SIZE 530

namespace atmega {
  // Device file handle
  typedef int handle;
  
  // Command values
  typedef enum Command_: uint8_t {
    COMMAND_SUCCESS       = 0x00,
    COMMAND_READ_SN       = 0x01,
    COMMAND_READ_VER      = 0x02,
    COMMAND_READ_MLEN     = 0x03,
    COMMAND_ECHO          = 0x04,
    COMMAND_TRANSFER_SPI  = 0x11,
    COMMAND_SCAN_RS485    = 0x21,
    COMMAND_READ_RS485    = 0x22,
    COMMAND_WRITE_RS485   = 0x23,
    COMMAND_SCAN_I2C      = 0x31,
    COMMAND_READ_I2C      = 0x32,
    COMMAND_WRITE_I2C     = 0x33,
    COMMAND_LOCK          = 0xA1,
    COMMAND_UNLOCK        = 0xA2,
    COMMAND_WRITE_SN      = 0xA3,
    COMMAND_CLR_FAULT     = 0xA4,
    COMMAND_LOCATE        = 0xA5,
    COMMAND_RESET         = 0xA7,
    COMMAND_FAILURE       = 0xF0,
    COMMAND_FAILURE_ARG   = 0xFA,
    COMMAND_FAILURE_STA   = 0xFB,
    COMMAND_FAILURE_BUS   = 0xFC,
    COMMAND_FAILURE_RS485 = 0xFD,
    COMMAND_FAILURE_CMD   = 0xFF
  } Command;
  
  // Command/response data structure
  typedef struct __attribute__((packed)) buffer_ {
    Command  command;
    uint16_t size;    // NOTE: little endian
    uint8_t  buffer[ATMEGA_MAX_BUFFER_SIZE];
  } buffer;
  
  // List all devices found
  std::list<std::string> find_devices();
  
  // Open a device and get it ready for running commands
  handle open(std::string device_name, bool exclusive_access=true);
  
  // Send a command, return the number of bytes received
  ssize_t send_command(handle fd, const buffer* command, buffer* response, 
                       int max_retry=0, int retry_wait_ms=50);
  
  // Close an open device
  void close(handle fd);
}
