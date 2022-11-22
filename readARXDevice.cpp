/*****************************************************
readARXDevice - Read a SPI register from the specified 
device.  An exit code of zero indicates that no errors
were encountered.
 
Usage:
  readARXDevice <SUB-20 S/N> <total stand count> <device> <command>

  * Command is a four digit hexadecimal values (i.e., 
  0x1234)
  
Options:
  None
*****************************************************/


#include <iostream>
#include <vector>
#include <queue>
#include <stdexcept>
#include <string>
#include <cstring>
#include <chrono>
#include <thread>

#include "libsub.h"
#include "aspCommon.h"


class ReadQueue {
private:
  uint32_t _size;
  
  std::vector<std::queue<uint16_t> > _buffer;
  
public:
  ReadQueue(uint32_t size) {
    _size = size;
    
    for(uint32_t i=0; i<_size; i++) {
      std::queue<uint16_t> device_buffer;
      _buffer.push_back(device_buffer);
    }
  }
  inline void add_read(uint32_t device, uint16_t register) {
    if( (device < 1) || (device > _size) ) {
      throw(std::runtime_error("Invalid device number"));
    }
    _buffer[device-1].push(register | 0x0080);
  }
  inline bool is_empty() {
    bool empty = true;
    for(uint32_t i=0; i<_size; i++) {
      if( _buffer[i].size() > 0 ) {
        empty = false;
        break;
      }
    }
    return empty;
  }
  inline uint16_t* get_reads() {
    uint16_t *registers = (uint16_t*) calloc(sizeof(uint16_t), _size+1);
    *(registers + 0) = SPI_COMMAND_MARKER;
    
    for(uint32_t i=0; i<_size; i++) {
      if( _buffer[i].size() > 0 ) {
        *(registers + _size - i) = _buffer[i].front();
        _buffer[i].pop();
      }
    }
    return registers;
  }
};


int main(int argc, char* argv[]) {
  /*************************
	* Command line parsing   *
	*************************/
  // Make sure we have the right number of arguments to continue
	if( argc < 4+1 ) {
		std::cout << "readARXDevice - Need at least 4 arguments, " << argc-1 << " provided" << std::endl;
		exit(1);
	}
  
  char *endptr;
  std::string requestedSN = std::string(argv[1]);
  uint32_t device_count = std::strtod(argv[2], &endptr);
  
  ReadQueue *queue = new ReadQueue(device_count);
  for(int i=3; i<argc; i+=2) {
    uint32_t device = std::strtod(argv[i], &endptr);
    uint16_t register = std::strtod(argv[i+1], &endptr);
    try {
      queue->add_read(device, register);
    } catch(const std::exception& e) {
      std::cout << "Invalid register " << device << " @ " << std::hex << register << std::dec << ": " << e.what() << std::endl;
      exit(1);
    }
  }
  
  /************************************
	* SUB-20 device selection and ready *
	************************************/
  sub_device dev = NULL;
  sub_handle fh = NULL;
	
	// Find the right SUB-20
	bool found = false;
  char foundSN[20];
	int success, openTries = 0;
	while( (!found) && (dev = sub_find_devices(dev)) ) {
		// Open the USB device (or die trying)
		fh = sub_open(dev);
		while( (fh == NULL) && (openTries < SUB20_OPEN_MAX_ATTEMPTS) ) {
			openTries++;
			std::this_thread::sleep_for(std::chrono::milliseconds(SUB20_OPEN_WAIT_US/1000));
			
			fh = sub_open(dev);
		}
		if( fh == NULL ) {
			continue;
		}
		
		success = sub_get_serial_number(fh, foundSN, sizeof(foundSN));
		if( !success ) {
			continue;
		}
		
		if( !strcmp(foundSN, requestedSN.c_str()) ) {
			std::cout << "Found SUB-20 device S/N: " << foundSN << std::endl;
			found = true;
		} else {
			sub_close(fh);
		}
	}
	
	// Make sure we actually have a SUB-20 device
	if( !found ) {
		std::cout << "readARXDevice - Cannot find or open SUB-20 " << requestedSN << std::endl;
		exit(1);
	}
  
  /****************************************
	* Send the command and get the response *
	****************************************/
	// Enable the SPI bus operations on the SUB-20 board
	int j = 0;
	success = sub_spi_config(fh, 0, &j);
	if( success ) {
		std::cout << "readARXDevice - get config - " << sub_strerror(sub_errno) << std::endl;
	}
	
	success = 1;
	while( success ) {
		success = sub_spi_config(fh, ARX_SPI_CONFIG, NULL);
		if( success ) {
			std::cout << "readARXDevice - set config - " << sub_strerror(sub_errno) << std::endl;
			exit(1);
		}
	}
	
  // Process the commands
  uint16_t *reads, *values;
  values = (uint16_t*) calloc(sizeof(uint16_t), device_count+1);
  while( !queue->is_empty() ) {
    reads = queue->get_reads();
    
    success = sub_spi_transfer(fh, (char*) reads, (char*) values, 2*device_count+2, SS_CONF(0, SS_LO));
  	if( success ) {
  		std::cout << "readARXDevice - SPI write #1 failed - " << sub_strerror(sub_errno) << std::endl;
      ::free(reads);
      ::free(values);
  		exit(2);
  	}
    
    if( values[device_count] != SPI_COMMAND_MARKER ) {
      std::cout << "readARXDevice - SPI write returned a marker of "
                << std::hex << values[device_count] << std::dec << " instead of "
                << std::hex << SPI_COMMAND_MARKER << std::dec << std::endl;
      ::free(reads);
      ::free(values);
  		exit(3);
  	}
    
    ::memset(reads+1, 0, 2*device_count);
    
    success = sub_spi_transfer(fh, (char*) reads, (char*) values, 2*device_count+2, SS_CONF(0, SS_LO));
  	if( success ) {
  		std::cout << "readARXDevice - SPI write #2 failed - " << sub_strerror(sub_errno) << std::endl;
      ::free(reads);
      ::free(values);
  		exit(2);
  	}
    
    if( values[device_count] != SPI_COMMAND_MARKER ) {
      std::cout << "readARXDevice - SPI write returned a marker of "
                << std::hex << values[device_count] << std::dec << " instead of "
                << std::hex << SPI_COMMAND_MARKER << std::dec << std::endl;
      ::free(reads);
      ::free(values);
  		exit(3);
  	}
    
    for(j=0; j<device_count; j++) {
      if( values[device_count-1-j] != 0 ) {
        std::cout << j+1 << ": " << std::hex << "0x" << (values[device_count-1-j]^0x0080) << std::dec << std::endl;
      }
    }
    
    ::free(reads);
  }
	
	/*******************
	* Cleanup and exit *
	*******************/
	::free(values);
  
	sub_close(fh);
	
	return 0;
}
