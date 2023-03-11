#include <list>
#include <string>
#include <filesystem>
#include <iostream>
#include <cstring>
#include <chrono>
#include <thread>

#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <arpa/inet.h>
#include <linux/serial.h>
#include <sys/ioctl.h>

#pragma once

#define ATMEGA_COMMAND_SUCCESS 0x00
#define ATMEGA_COMMAND_FAILED 0xFF

struct __attribute__((packed)) atmega_buffer {
  uint8_t  command;
  uint16_t size;
  uint8_t  buffer[256];
};

int configure_port(int fd);

int send_command(int fd, atmega_buffer* command, atmega_buffer* response);
