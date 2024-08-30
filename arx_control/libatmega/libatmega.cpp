#include "libatmega.hpp"
#include <iostream>
#include <chrono>
#include <thread>
#include <sys/select.h>

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
        
        if( (vendor_id == 0x0403) || (vendor_id == 0x2341) || (vendor_id == 0x2886) ) {
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
        
        if( (product_id == 0x6001) || (product_id == 0x0001) || (product_id == 0x802f) ) {
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
  ::cfsetispeed(&tty, B115200);
  ::cfsetospeed(&tty, B115200);
  
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
    throw(std::runtime_error(std::string("Failed to assert DTR: ") \
                             +std::string(strerror(errno))));
  };
  ctl = TIOCM_RTS;
  if( ::ioctl(fd, TIOCMBIS, &ctl) != 0 ) {
    throw(std::runtime_error(std::string("Failed to assert RTS: ") \
                             +std::string(strerror(errno))));
  }
  
  return fd;
}

ssize_t atmega::send_command(atmega::handle fd, const atmega::buffer* command, atmega::buffer* response,
                             int max_retry, int retry_wait_ms) {
  // Empty the response and set the command value to 0xFF
  ::memset(response, 0, sizeof(buffer));
  response->command = atmega::COMMAND_FAILURE;
  
  // Empty the buffer
  ::tcdrain(fd);
  
  ssize_t nsend = 0, nrecv = 0, nbatch = 0, nleft=0;
  const char *start = "<<<";
  const char *stop = ">>>";
  char *temp;
  temp = (char*) ::calloc(1, 6+sizeof(buffer));
  for(int i=0; i<max_retry+1; i++) {
    if( i > 0 ) {
      std::this_thread::sleep_for(std::chrono::milliseconds(retry_wait_ms));
    }
    
    // Send the command
    nsend = ::write(fd, start, 3);
    nsend += ::write(fd, command, 3+command->size);
    nsend += ::write(fd, stop, 3);
    #if defined(ATMEGA_DEBUG) && ATMEGA_DEBUG
    std::cout << "sent: " << command->buffer << std::endl;
    #endif
    
    // Set the timeout based on the command type
    int cmd_wait_ms = 5;
    if( (   (command->command == atmega::COMMAND_READ_I2C)
         || (command->command == atmega::COMMAND_WRITE_I2C)) ) {
      // I2C operations are slow
      cmd_wait_ms = 50;
    } else if( (   (command->command == atmega::COMMAND_READ_SN)
                || (command->command == atmega::COMMAND_WRITE_SN)) ) {
      // EEPROM operations are really slow
      cmd_wait_ms = 100;
    } else if( (   (command->command == atmega::COMMAND_READ_RS485)
                || (command->command == atmega::COMMAND_WRITE_RS485)
                || (command->command == atmega::COMMAND_SEND_RS485) ) ) {
      // RS485 has a 1000 ms timeout
      cmd_wait_ms = 1050;
    } else if( command ->command == atmega::COMMAND_SCAN_RS485 ) {
      // RS485 scan has a 2 *s* timeout
      cmd_wait_ms = 2000;
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    
    // Receive the reply
    nrecv = 0;
    nleft = 6 + sizeof(buffer);
    fd_set read_fds;
    struct timeval batch_timeout;
    batch_timeout.tv_sec = 0;
    batch_timeout.tv_usec = 1000;
    
    FD_ZERO(&read_fds);
    FD_SET(fd, &read_fds);
    
    int timeout_count = 0;
    double elapsed_time = 0.0;
    auto start_time = std::chrono::steady_clock::now();
    while( (nleft > 0) && (timeout_count < 5) && (elapsed_time <= cmd_wait_ms) ) {
      int ret = ::select(fd + 1, &read_fds, nullptr, nullptr, &batch_timeout);
      #if defined(ATMEGA_DEBUG) && ATMEGA_DEBUG
      std::cout << "poll with " << ret << " after " << nleft << "; " << timeout_count << "; " << elapsed_time << std::endl;
      #endif
      if( ret == -1 ) {
        // Something when wrong, giving up.
        break;
      } else if( ret == 0 ) {
        if( nrecv > 0 ) {
          // We've received some data but now we are timing out.  Increment the
          // timeout counter so we know when to give up.
          timeout_count++;
        }
        
        // Update the elapsed time
        auto current_time = std::chrono::steady_clock::now();
        elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
        
        // Try again...
        continue;
      }
      
      if( FD_ISSET(fd, &read_fds) ) {
        nbatch = ::read(fd, (uint8_t*) (temp+nrecv), nleft);
        #if defined(ATMEGA_DEBUG) && ATMEGA_DEBUG
        std::cout << "nrecv: " << nrecv << " and nbatch: " << nbatch << std::endl;
        #endif
        if( nbatch > 0 ) {
          // We've received something.  Update the data counters and reset the
          // timeout counter.
          nrecv += nbatch;
          nleft -= nbatch;
          timeout_count = 0;
          
          // Update the elapsed time
          auto current_time = std::chrono::steady_clock::now();
          elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - start_time).count();
        }
      }
    }
    #if defined(ATMEGA_DEBUG) && ATMEGA_DEBUG
    std::cout << "nrecv: " << nrecv << std::endl;
    #endif
    
    FD_CLR(fd, &read_fds);
    
    if( nsend >= 9 && nrecv >= 9 ) {
      if( (strncmp(temp, start, 3) == 0) && (strncmp(temp+nrecv-3, stop, 3) == 0) ) {
        ::memcpy((uint8_t*) response, temp+3, nrecv-6);
        break;
      }
    }
  }
  
  ::free(temp);
  
  if( nsend < 9 || nrecv < 9 ) {
    throw(std::runtime_error(std::string("Failed to send command")));
  }
  
  return nrecv;
}

void atmega::close(atmega::handle fd) {
  if( fd >= 0 ) {
    ::close(fd);
  }
}
