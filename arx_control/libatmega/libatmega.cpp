#include "libatmega.hpp"

std::list<std::string> atmega::find_devices() {
  std::list<std::string> devices;
  
  for(auto const& dir_entry: std::filesystem::directory_iterator{"/dev/"}) {
    std::string entry_name = dir_entry.path();
    if( (   (entry_name.find("ttyUSB") == std::string::npos) 
         && (entry_name.find("ttyACM") == std::string::npos) ) ) {
      continue;
    }
    
    std::string udev_lookup = std::string("udevadm info --name=")+entry_name;
    std::unique_ptr<FILE, decltype(&::pclose)> pipe(::popen(udev_lookup.c_str(), "r"), ::pclose);
    if( pipe == nullptr ) {
      continue;
    }
    
    char buffer[256];
    uint8_t match = 0;
    while( fgets(buffer, 256, pipe.get()) != nullptr ) {
      if( (   (strstr(buffer, "ID_VENDOR_ID=0403") != nullptr)
           || (strstr(buffer, "ID_VENDOR_ID=2341") != nullptr) ) ) {
        match |= 1;
      } else if( (   (strstr(buffer, "ID_MODEL_ID=6001") != nullptr )
                  || (strstr(buffer, "ID_MODEL_ID=0001") ) ) ) {
        match |= 2;
      }
    }
    
    if( match == 3 ) {
      devices.push_back(entry_name);
    }
  }

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
