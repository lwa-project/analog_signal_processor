#include "libatmega.hpp"

std::list<std::string> atmega::find_devices() {
  std::list<std::string> devices;
  
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

  udev_list_entry *devices = udev_enumerate_get_list_entry(enumerate);

  udev_list_entry *dev_list_entry;
  udev_list_entry_foreach(dev_list_entry, devices) {
    const char *dev_path = udev_list_entry_get_name(dev_list_entry);
    udev_device *device = udev_device_new_from_syspath(udev, dev_path);
    if( device == nullptr ) {
      continue;
    }
    
    uint8_t match = 0;
    const char *vendor_id = udev_device_get_property_value(device, "ID_VENDOR_ID");
    const char *product_id = udev_device_get_property_value(device, "ID_MODEL_ID");
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
    
    udev_device_unref(device);
  }
  
  udev_enumerate_unref(enumerate);
  udev_unref(udev);
  
  return devices;
}

atmega::handle atmega::open(std::string device_name, bool exclusive_access) {
  atmega::handle fd = ::open(device_name.c_str(), O_RDONLY | O_NOCTTY);
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

ssize_t atmega::send_command(atmega::handle fd, const buffer* command, buffer* response) {
  // Empty the response and set the command value to 0xFF
  ::memset(response, 0, sizeof(buffer));
  response->command = COMMAND_FAILURE;
  
  // Send the command
  ssize_t n = ::write(fd, command, 3+command->size);
  if( n == 0 ) {
    throw(std::runtime_error(std::string("Failed to send command")));
  }
  
  // Receive the reply
  ::ioctl(fd, FIONREAD, &n);
  n = ::read(fd, (uint8_t*) response, n);
  if( n < (ssize_t) (sizeof(buffer) - ATMEGA_MAX_BUFFER_SIZE) ) {
    throw(std::runtime_error(std::string("Failed to receive command response")));
  }
  
  return n;
}

void atmega::close(atmega::handle fd) {
  if( fd >= 0 ) {
    ::close(fd);
  }
}
