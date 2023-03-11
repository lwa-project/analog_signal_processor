#include "libatmega.hpp"

atmega::handle atmega::open(std::string device_name) {
  atmega::handle fd = ::open(device_name.c_str(), O_RDONLY | O_NOCTTY);
  if( fd < 0 ) {
    throw(std::runtime_error(std::string("Failed to open device: ") \
                             +std::string(strerror(errno))));
  }
  return fd;
}

void atmega::configure_port(atmega::handle fd, bool exclusive_access) {
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
}

int atmega::send_command(atmega::handle fd, const buffer* command, buffer* response) {
  // Empty the response and set the command value to 0xFF
  ::memset(response, 0, sizeof(buffer));
  response->command = COMMAND_FAILURE;
  
  // Send the command
  int n = ::write(fd, command, 3+command->size);
  if( n == 0 ) {
    throw(std::runtime_error(std::string("Failed to send command")));
  }
  
  // Receive the reply
  ::ioctl(fd, FIONREAD, &n);
  n = ::read(fd, (uint8_t*) response, n);
  if( n < (sizeof(buffer) - ATMEGA_MAX_BUFFER_SIZE) ) {
    throw(std::runtime_error(std::string("Failed to receive command response")));
  }
  
  return n;
}

void atmega::close(atmega::handle fd) {
  if( fd >= 0 ) {
    ::close(fd);
  }
}
