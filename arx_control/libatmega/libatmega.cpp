#include "libatmega.hpp"
#include <iostream>
#include <chrono>
#include <thread>

std::list<std::string> atmega::find_devices() {
  std::list<std::string> devices;
  
#if defined(__APPLE__) && __APPLE__
  CFMutableDictionaryRef matchingDict;
  io_iterator_t uiter;
  kern_return_t kr;
  io_service_t udevice;

  matchingDict = IOServiceMatching(kIOSerialBSDServiceValue);
  if( matchingDict == nullptr ) {
      return devices;
  }
  
  kr = IOServiceGetMatchingServices(kIOMainPortDefault, matchingDict, &uiter);
  if( kr != KERN_SUCCESS ) {
      return devices;
  }
  
  while( (udevice = IOIteratorNext(uiter)) ) {
    uint8_t match = 0;
    char *dev_path;
    uint16_t vendor_id;
    uint16_t product_id;
    CFStringRef devpathRef = (CFStringRef) IORegistryEntrySearchCFProperty(udevice,
                                                                           kIOServicePlane,
                                                                           CFSTR(kIOCalloutDeviceKey),
                                                                           kCFAllocatorDefault,
                                                                           kIORegistryIterateRecursively | kIORegistryIterateParents);
    dev_path = nullptr;
    if( devpathRef ) {
      dev_path = (char*) CFStringGetCStringPtr(devpathRef, kCFStringEncodingUTF8);
    }
    
    CFNumberRef vendorRef = (CFNumberRef) IORegistryEntrySearchCFProperty(udevice,
                                                                          kIOServicePlane,
                                                                          CFSTR(kUSBVendorID),
                                                                          kCFAllocatorDefault,
                                                                          kIORegistryIterateRecursively | kIORegistryIterateParents);
    vendor_id = 0;
    if( vendorRef ) {
        CFNumberGetValue(vendorRef, kCFNumberSInt16Type, &vendor_id);
        CFRelease(vendorRef);
        
        if( (vendor_id == 0x0403) || (vendor_id == 0x2341) ) {
          match |= 1;
        }
    }
      
    CFNumberRef productRef = (CFNumberRef) IORegistryEntrySearchCFProperty(udevice,
                                                                           kIOServicePlane,
                                                                           CFSTR(kUSBProductID),
                                                                           kCFAllocatorDefault,
                                                                           kIORegistryIterateRecursively | kIORegistryIterateParents);
    product_id = 0;
    if( productRef ) {
        CFNumberGetValue(productRef, kCFNumberSInt16Type, &product_id);
        CFRelease(productRef);
        
        if( (product_id == 0x6001) || (product_id == 0x0001) ) {
          match |= 2;
        }
    }
    
    if( match == 3 && dev_path != nullptr ) {
      devices.push_back(std::string(dev_path));
    }
    
    CFRelease(devpathRef);
    IOObjectRelease(udevice);
  }
  
  /* Done, release the iterator */
  IOObjectRelease(uiter);
  
#else
  udev *udev = udev_new();
  if( udev == nullptr ) {
    return devices;
  }
  
  udev_enumerate *enumerate = udev_enumerate_new(udev);
  if( enumerate == nullptr ) {
    udev_unref(udev);
    return devices;
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
    
    uint8_t match = 0;
    const char *vendor_id = udev_device_get_property_value(udevice, "ID_VENDOR_ID");
    const char *product_id = udev_device_get_property_value(udevice, "ID_MODEL_ID");
    if( (   (strstr(vendor_id, "0403") != nullptr)
         || (strstr(vendor_id, "2341") != nullptr) ) ) {
        match |= 1;
    }
    if( (   (strstr(product_id, "6001") != nullptr )
         || (strstr(product_id, "0001") != nullptr ) ) ) {
        match |= 2;
    }
    
    if( match == 3 ) {
      devices.push_back(std::string(dev_path));
    }
    
    udev_device_unref(udevice);
  }
  
  udev_enumerate_unref(enumerate);
  udev_unref(udev);
#endif
  
  return devices;
}

atmega::handle atmega::open(std::string device_name, bool exclusive_access) {
  atmega::handle fd = ::open(device_name.c_str(), O_RDWR | O_NOCTTY);
  if( fd < 0 ) {
    throw(std::runtime_error(std::string("Failed to open device: ") \
                             +std::string(strerror(errno))));
  }
  
  struct termios tty;
  if( ::tcgetattr(fd, &tty) != 0 ) {
    throw(std::runtime_error(std::string("Failed to get attributes: ") \
                             +std::string(strerror(errno))));
  }
  
  // Configuration
  // Size, parity, and hardware flow control
  tty.c_cflag &= ~CSIZE;
  tty.c_cflag |= CS8;
  tty.c_cflag &= ~PARENB;
  tty.c_cflag &= ~CSTOPB;
  tty.c_cflag &= ~CRTSCTS;
  tty.c_cflag |= CREAD | CLOCAL;
  
  // Data intepretting (or not)
  tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG);
  
  // Sofware flow control
  tty.c_iflag &= ~(IXON | IXOFF | IXANY);
  
  // Data intepretting (or not)
  tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | INPCK | ISTRIP | INLCR | IGNCR | ICRNL);
  
  // Data intepretting (or not)
  tty.c_oflag = 0;
  tty.c_oflag &= ~OPOST;
  
  // Waiting parameters
  tty.c_cc[VTIME] = 1;
  tty.c_cc[VMIN] = 0;
  
  // Speed
  ::cfsetispeed(&tty, B9600);
  ::cfsetospeed(&tty, B9600);
  
  if( ::tcsetattr(fd, TCSANOW, &tty) != 0 ) {
    throw(std::runtime_error(std::string("Failed to set attributes: ") \
                             +std::string(strerror(errno))));
  }
  
  if( ::tcgetattr(fd, &tty) != 0 ) {
    throw(std::runtime_error(std::string("Failed to get attributes: ") \
                             +std::string(strerror(errno))));
  }
  
  if( exclusive_access ) {
    // Grab the port if needed
    if( ::ioctl(fd, TIOCEXCL, NULL) != 0 ) {
      throw(std::runtime_error(std::string("Failed to set exclusive access: ") \
                               +std::string(strerror(errno))));
    }
  }
  
  // Ready the port
  int ctl = TIOCM_DTR;
  if( ::ioctl(fd, TIOCMBIS, &ctl) != 0 ) {
    throw(std::runtime_error(std::string("Failed to assert DTS: ") \
                             +std::string(strerror(errno))));
  };
  ctl = TIOCM_RTS;
  if( ::ioctl(fd, TIOCMBIS, &ctl) != 0 ) {
    throw(std::runtime_error(std::string("Failed to assert RTS: ") \
                             +std::string(strerror(errno))));
  }
  
  return fd;
}

ssize_t atmega::send_command(atmega::handle fd, const atmega::buffer* command, atmega::buffer* response) {
  // Empty the response and set the command value to 0xFF
  ::memset(response, 0, sizeof(buffer));
  response->command = atmega::COMMAND_FAILURE;
  
  // Send the command
  const char start[3] = {'<', '<', '<'};
  const char stop[3] = {'>', '>', '>'};
  ssize_t n = ::write(fd, &(start[0]), sizeof(start));
  n += ::write(fd, command, 3+ntohs(command->size));
  n += ::write(fd, &(stop[0]), sizeof(stop));
  std::cout << "write n: " << n << std::endl;
  if( n < 9 ) {
    throw(std::runtime_error(std::string("Failed to send command")));
  }
  
  std::this_thread::sleep_for(std::chrono::milliseconds(250));
  
  // Receive the reply
  n = ::read(fd, (uint8_t*) response, 3);
  n += ::read(fd, (uint8_t*) response, sizeof(buffer));
  std::cout << "read n: " << n << std::endl;
  if( n < 9 ) {
    throw(std::runtime_error(std::string("Failed to receive command response")));
  }
  
  return n;
}

void atmega::close(atmega::handle fd) {
  if( fd >= 0 ) {
    ::close(fd);
  }
}
