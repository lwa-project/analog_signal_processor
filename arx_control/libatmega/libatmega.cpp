#include "libatmega.hpp"

int configure_port(int fd) {
  struct termios tty;
  if( tcgetattr(fd, &tty) != 0 ) {
    std::cout << "Failed to get attributes: " << strerror(errno) << std::endl;
    return 1;
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
  cfsetispeed(&tty, B9600);
  cfsetospeed(&tty, B9600);
  
  if( tcsetattr(fd, TCSANOW, &tty) != 0 ) {
    std::cout << "Failed to st attributes: " << strerror(errno) << std::endl;
    return 1;
  }
  
  if( tcgetattr(fd, &tty) != 0 ) {
    std::cout << "Failed to get attributes: " << strerror(errno) << std::endl;
    return 1;
  }
  
  // // Grab the port
  // if (ioctl(fd, TIOCEXCL, NULL) != 0 ) {
  //   std::cout << "Failed to grab the port: " << strerror(errno) << std::endl;
  // }
  
  // Ready the port
  int ctl = TIOCM_DTR;
  if( ioctl(fd, TIOCMBIS, &ctl) != 0 ) {
    std::cout << "Failed to set ready: " << strerror(errno) << std::endl;
    return 1;
  };
  ctl = TIOCM_RTS;
  if( ioctl(fd, TIOCMBIS, &ctl) != 0 ) {
    std::cout << "Failed to signal ready: " << strerror(errno) << std::endl;
    return 1;
  }
  
  return 0;
}

int send_command(int fd, atmega_buffer* command, atmega_buffer* response) {
  // Empty the response and set the command value to 0xFF
  ::memset(response, 0, sizeof(atmega_buffer));
  response->command = ATMEGA_COMMAND_FAILED;
  
  // Send the command
  int n = write(fd, command, 3+command->size);
  
  // Receive the reply
  ioctl(fd, FIONREAD, &n);
  n = read(fd, (uint8_t*) response, n);
  return n;
}
