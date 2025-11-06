/*****************************************************
setATmegaSN - Set the internal serial number of a
ATmega device to that of its USB interface chip.
 
Usage:
  readARXDevice <device name>

Options:
  None
*****************************************************/


#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>
#include <cstdio>

#include "libatmega.hpp"
#include "aspCommon.hpp"


int main(int argc, char* argv[]) {
  /*************************
  * Command line parsing   *
  *************************/
  // Make sure we have the right number of arguments to continue
  if( argc < 1+1 ) {
    std::cerr << "setATmegaSA - Need at least 1 argument, " << argc-1 << " provided" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  std::string device_name = std::string(argv[1]);
  
  /***************************************
  * Find the serial number using udevadm *
  ****************************************/
  std::string device_sn;
#if defined(__APPLE__) && __APPLE__
  CFMutableDictionaryRef matchingDict;
  io_iterator_t uiter;
  kern_return_t kr;
  io_service_t udevice;

  matchingDict = IOServiceMatching(kIOSerialBSDServiceValue);
  if( matchingDict == nullptr ) {
      std::exit(EXIT_FAILURE);
  }

  kr = IOServiceGetMatchingServices(kIOMainPortDefault, matchingDict, &uiter);
  if( kr != KERN_SUCCESS ) {
      std::exit(EXIT_FAILURE);
  }

  while( (udevice = IOIteratorNext(uiter)) ) {
    char *dev_path;
    char *dev_sn;
    CFStringRef devpathRef = (CFStringRef) IORegistryEntrySearchCFProperty(udevice,
                                                                           kIOServicePlane,
                                                                           CFSTR(kIOCalloutDeviceKey),
                                                                           kCFAllocatorDefault,
                                                                           kIORegistryIterateRecursively | kIORegistryIterateParents);
    dev_path = nullptr;
    dev_sn = nullptr;
    CFStringRef devsnRef = nullptr;
    if( devpathRef ) {
      dev_path = (char*) CFStringGetCStringPtr(devpathRef, kCFStringEncodingUTF8);
      if( strcmp(device_name.c_str(), dev_path) == 0 ) {
        devsnRef = (CFStringRef) IORegistryEntrySearchCFProperty(udevice,
                                                                 kIOServicePlane,
                                                                 CFSTR(kUSBSerialNumberString),
                                                                 kCFAllocatorDefault,
                                                                 kIORegistryIterateRecursively | kIORegistryIterateParents);
        if( devsnRef ) {
          dev_sn = (char*) CFStringGetCStringPtr(devsnRef, kCFStringEncodingUTF8);
          device_sn = std::string(dev_sn);
        }
      }
    }
    
    CFRelease(devpathRef);
    if( devsnRef != nullptr ) {
      CFRelease(devsnRef);
    }
    IOObjectRelease(udevice);
  }
  
  IOObjectRelease(uiter);
  
#else
  udev *udev = udev_new();
  if( udev == nullptr ) {
    std::exit(EXIT_FAILURE);
  }

  udev_enumerate *enumerate = udev_enumerate_new(udev);
  if( enumerate == nullptr ) {
    udev_unref(udev);
    std::exit(EXIT_FAILURE);
  }

  udev_enumerate_add_match_subsystem(enumerate, "tty");
  udev_enumerate_add_match_property(enumerate, "ID_BUS", "usb");
  udev_enumerate_scan_devices(enumerate);

  udev_list_entry *udevices = udev_enumerate_get_list_entry(enumerate);

  udev_list_entry *udev_list_entry;
  udev_list_entry_foreach(udev_list_entry, udevices) {
    const char *dev_path = udev_list_entry_get_name(udev_list_entry);
    udev_device *udevice = udev_device_new_from_syspath(udev, dev_path);
    if( udevice == nullptr ) {
      continue;
    }
    
    const char *dev_sn = udev_device_get_property_value(udevice, "ID_SERIAL_SHORT");
    device_sn = std::string(dev_sn);
    
    udev_device_unref(udevice);
  }

  udev_enumerate_unref(enumerate);
  udev_unref(udev);
#endif
  
  if( device_sn.size() == 0 ) {
    std::cerr << "setATmegaSN - Failed to find a serial number for " << device_name << std::endl;
    std::exit(EXIT_FAILURE);
  }
  std::cout << device_name << " -> " << device_sn.substr(0, 8) << std::endl;
  
  /******************************************
  * ATmega device selection and programming *
  *******************************************/
  int open_attempts = 0;
  atmega::handle fd = -1;
  while( open_attempts < ATMEGA_OPEN_MAX_ATTEMPTS ) {
    try {
      fd = atmega::open(device_name);
      break;
    } catch(const std::exception& e) {
      open_attempts++;
      std::this_thread::sleep_for(std::chrono::milliseconds(ATMEGA_OPEN_WAIT_MS));
    }
  }
  if( fd < 0 ) {
    std::cerr << "setATmegaSN - Failed to open device" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  /************************
  * Set the serial number *
  *************************/
  int n = 0;
  atmega::buffer cmd, resp;
  cmd.command = atmega::COMMAND_UNLOCK;
  cmd.size = 0;
  
  n = atmega::send_command(fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
  if( (n == 0) || (resp.command & atmega::COMMAND_FAILURE) ) {
    atmega::close(fd);
    std::cerr << "setATmegaSN - Failed to unlock device" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  cmd.command = atmega::COMMAND_WRITE_SN;
  cmd.size = device_sn.size();
  ::memcpy(&(cmd.buffer[0]), device_sn.c_str(), device_sn.size());
  
  n = atmega::send_command(fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
  if( (n == 0) || (resp.command & atmega::COMMAND_FAILURE) ) {
    atmega::close(fd);
    std::cerr << "setATmegaSN - Failed to write serial number to device" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  cmd.command = atmega::COMMAND_LOCK;
  cmd.size = 0;
  
  n = atmega::send_command(fd, &cmd, &resp, ATMEGA_OPEN_MAX_ATTEMPTS, ATMEGA_OPEN_WAIT_MS);
  if( (n == 0) || (resp.command & atmega::COMMAND_FAILURE) ) {
    atmega::close(fd);
    std::cerr << "setATmegaSN - Failed to lock device" << std::endl;
    std::exit(EXIT_FAILURE);
  }
  
  /*******************
  * Cleanup and exit *
  *******************/
  atmega::close(fd);
  
  std::exit(EXIT_SUCCESS);
}
