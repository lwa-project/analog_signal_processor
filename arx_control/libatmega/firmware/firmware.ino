#include <SPI.h>
#include <EEPROM.h>
#include <Wire.h>

// Chip select pin for SPI operations
#define SPI_SS_PIN 10

// Maximum command length
#define MAX_CMD_LEN 530

// Maximum serial number length
#define MAX_SN_LEN 8

// Timeout in ms before flushing the command input buffer
#define CMD_TIMEOUT_MS 50

// Version string
#define VERSION "v0.0.1"

// Command input buffer
long int last_char = -1;
boolean in_progress = false, is_ready = false;
uint16_t idx = 0;
uint8_t buffer[MAX_CMD_LEN+6] = {0};

// Command
uint8_t command = 0x00;
uint16_t nargs = 0;

// Control variables
uint16_t i;
uint8_t locked = 1;
char device_sn[MAX_SN_LEN] = {'\0'};

uint16_t bswap16(uint16_t v) {
  // Helper function to byte swap a unsigned short value
  return ((v & 0xFF) << 8) | ((v >> 8) & 0xFF);
}

uint32_t bswap32(uint32_t v) {
  // Helper function to byte swap a unsigned int value
  return ((v & 0xFF) << 24) | (((v >> 8) & 0xFF) << 16) | (((v >> 16) & 0xFF) << 8) | ((v >> 24) & 0xFF);
}

void serial_write16(uint16_t value) {
  // Helper function byte swap and write a unsigned short value to Serial
  value = bswap16(value);
  Serial.write((uint8_t*) &value, sizeof(uint16_t));
}

void serial_write32(uint32_t value) {
  // Helper function byte swap and write a unsigned int value to Serial
  value = bswap32(value);
  Serial.write((uint8_t*) &value, sizeof(uint32_t));
}

void serial_sendresp(uint8_t status, uint16_t size, uint8_t *data) {
  // Send a response to a command
  /* Preamble */
  Serial.write((uint8_t) 60);
  Serial.write((uint8_t) 60);
  Serial.write((uint8_t) 60);
  /* Status code */
  Serial.write(status);
  /* Message size */
  serial_write16(size);
  /* Message body */
  if( data != NULL && size > 0 ) {
    Serial.write((uint8_t*) data, size);
  }
  /* Postamble */
  Serial.write((uint8_t) 62);
  Serial.write((uint8_t) 62);
  Serial.write((uint8_t) 62);
  Serial.flush();

}

void invalid_arguments(uint16_t nargs, uint8_t* argv) {
  // Invalid argument count
  serial_sendresp(0xFA, 0, NULL);
}

void invalid_state(uint16_t nargs, uint8_t* argv) {
  // Invalid device state
  serial_sendresp(0xFB, 0, NULL);
}

void invalid_bus_error(uint16_t nargs, uint8_t* argv) {
  // Invalid or not data received over the bus
  serial_sendresp(0xFC, 0, NULL);
}

void invalid_command(uint16_t nargs, uint8_t* argv) {
  // Invalid command
  serial_sendresp(0xFF, 0, NULL);
}

void read_sn(uint16_t nargs, uint8_t* argv) {
  // Read the MAX_SN_LEN character device serial number from EEPROM and return it
  //   Input: 0 arguments
  //   Ouput: MAX_SN_LEN char
  if( nargs > 0) {
    invalid_arguments(nargs, argv);
  } else {
    EEPROM.get(0, device_sn);

    serial_sendresp(0, MAX_SN_LEN, (uint8_t*) &device_sn[0]);
  }
}

void read_version(uint16_t nargs, uint8_t* argv) {
  // Return the version string for this firmware
  //   Input: 0 arguments
  //   Output: strlen(VERSION) characters
  if( nargs > 0) {
    invalid_arguments(nargs, argv);
  } else {
    const char* version = VERSION;

    serial_sendresp(0, strlen(VERSION), (uint8_t*) &version[0]);
  }
}

void read_max_cmd_len(uint16_t nargs, uint8_t* argv) {
  // Return the maximum payload size for a command
  //   Input: 0 arguments
  //   Output: uint16_t of maximum command length
  if( nargs > 0) {
    invalid_arguments(nargs, argv);
  } else {
    uint16_t value = MAX_CMD_LEN;
    value = bswap16(value);

    serial_sendresp(0, sizeof(uint16_t), (uint8_t*) &value);
  }
}
  
void echo(uint16_t nargs, uint8_t* argv) {
  // Echo the arguments back to the sender
  //   Input: N arguments (N >= 0)
  //   Output: N arguments provided

  serial_sendresp(0, nargs, argv);
}

void transfer_spi(uint16_t nargs, uint8_t* argv) {
  // Transfer the given data over SPI and return the full response from the bus
  //   Input: N arguments (N >= 1)
  //   Output: N responses from the SPI transfer
  if( nargs == 0 ) {
    invalid_arguments(nargs, argv);
  } else {
    SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE3));
    digitalWrite(SPI_SS_PIN, LOW);
    SPI.transfer(argv, nargs);
    digitalWrite(SPI_SS_PIN, HIGH);
    SPI.endTransaction();
    
    serial_sendresp(0, nargs, argv);
  }
}

void read_adcs(uint16_t nargs, uint8_t* argv) {
  // Read the first four ADCs and return the unintepretted values in the range of [0,1023]
  //   Input: 0 arguments
  //   Output: Four uint32_t values in the order of A0, A1, A2, and A3
  uint32_t value[4] = {0};
  if( nargs > 0) {
    invalid_arguments(nargs, argv);
  } else {
    value[0] = analogRead(A0);
    value[1] = analogRead(A1);
    value[2] = analogRead(A2);
    value[3] = analogRead(A3);
    for(i=0; i<4; i++) {
      value[i] = bswap32(value[i]);
    }

    serial_sendresp(0, 4*sizeof(uint32_t), (uint8_t*) &value[0]);
  }
}

void scan_i2c(uint16_t nargs, uint8_t* argv) {
  // Scan the I2C bus and return all of the valid addresses found
  //   Input: 0 arguments
  //   Output:  N uint8_t values for device addressed (N >= 0)
  int ndevice = 0;
  byte addr, err;
  byte found_addr[128];
  if( nargs > 0) {
    invalid_arguments(nargs, argv);
  } else {
    for(addr=0; addr<127; addr++) {
      Wire.beginTransmission(addr);
      err = Wire.endTransmission();
      if( err == 0 ) {
        found_addr[ndevice] = addr;
        ndevice++;
      }
    }

    serial_sendresp(0, ndevice, (uint8_t*) &found_addr[0]);
  }
}

void read_i2c(uint16_t nargs, uint8_t* argv) {
  // Read a given number of bytes from the provided register at the provided address
  //   Input: 3 arguments - address (uint8_t), register (uint8_t), read size (uint8_t)
  //   Output: "size" uint8_t values from the register
  byte addr = argv[0];
  byte reg = argv[1];
  byte size = argv[2];
  if( nargs < 3 ) {
    invalid_arguments(nargs, argv);
  } else {
    byte err;
    Wire.beginTransmission(addr);  
    Wire.write(reg);
    Wire.endTransmission(false);

    delay(5);
    
    Wire.requestFrom(addr, size);
    Wire.readBytes(argv, size);
    err = Wire.endTransmission(); 
    if( err == 0 ) {
      serial_sendresp(0, size, argv);
    } else {
      invalid_bus_error(nargs, argv);
    }
  }
}

void write_i2c(uint16_t nargs, uint8_t* argv) {
  // Write the given number of bytes to the provided register at the provided address
  //   Input: 3+ arguments - address (uint8_t), register (uint8_t), value (N uint8_t)
  //   Output: Value written as N uint8_t
  byte addr = argv[0];
  byte reg = argv[1];
  uint16_t size = nargs - 2;
  if( nargs < 3 ) {
    invalid_arguments(nargs, argv);
  } else {
    byte err;
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.endTransmission(false);

    delay(5);
    
    Wire.write((uint8_t*) &(argv[2]), size);
    err = Wire.endTransmission();
    if( err == 0 ) {
      serial_sendresp(0, size, argv);
    } else {
      invalid_bus_error(nargs, argv);
    }
  }
}

void unlock_sn(uint16_t nargs, uint8_t* argv) {
  // Unlock the write_sn function
  //   Input: 0 arguments
  //   Output:  0 values

  if( nargs > 0) {
    invalid_arguments(nargs, argv);
  } else {
    locked = 0;

    serial_sendresp(0, 0, NULL);
  }
}

void lock_sn(uint16_t nargs, uint8_t* argv) {
  // Lock the write_sn function
  //   Input: 0 arguments
  //   Output:  0 values
  
  
  if( nargs > 0) {
    invalid_arguments(nargs, argv);
  } else {
    locked = 1;

    serial_sendresp(0, 0, NULL);
  }
}

void write_sn(uint16_t nargs, uint8_t* argv) {
  // Write the at most MAX_SN_LEN character device serial number to EEPROM and return it
  //   Input: N char (0 < N <= MAX_SN_LEN)
  //   Output: MAX_SN_LEN char (see read_sn)
  if( nargs == 0) {
    invalid_arguments(nargs, argv);
  } else if( locked == 1 ) {
    invalid_state(nargs, argv);
  } else {
    for(i=0; i<MAX_SN_LEN; i++) {
      if(i < nargs) {
        device_sn[i] = argv[i];
      } else {
        device_sn[i] = '\0';
      }
    }
    EEPROM.put(0, device_sn);
    read_sn(0, argv);
  }
}

void setup() {
  // Serial setup
  Serial.begin(9600);

  // SPI setup
  pinMode(SPI_SS_PIN, OUTPUT);
  SPI.begin();

  // I2C setup
  Wire.begin();
}

void loop() {
  // Read in a command over the serial port
  while( Serial.available() > 0 && is_ready == 0 ) {
    buffer[idx++] = Serial.read();
    last_char = millis();
    
    if( !in_progress && idx > 2 ) {
      if( buffer[idx-3] == 60 && buffer[idx-2] == 60 && buffer[idx-1] == 60 ) {
        // Start of a new command
        in_progress = true;
        is_ready = false;
        idx = 0;
      }
    } else if( in_progress && idx > 2 ) {
      if( buffer[idx-3] == 62 && buffer[idx-2] == 62 && buffer[idx-1] == 62 ) {
        // End of a command, mark it ready for processing
        in_progress = false;
        is_ready = true;
      }
    }

    if( idx == sizeof(buffer) ) {
      idx = 0;
    }
  }
  
  if( is_ready ) {
    // Load the command
    command = buffer[0];
    nargs = (buffer[1] << 8) | buffer[2];
    
    // Process the command
    switch(command) {
      case 0x00: break;
      case 0x01: read_sn(nargs, &buffer[3]); break;
      case 0x02: read_version(nargs, &buffer[3]); break; 
      case 0x03: read_max_cmd_len(nargs, &buffer[3]); break; 
      case 0x04: echo(nargs, &buffer[3]); break;
      case 0x11: transfer_spi(nargs, &buffer[3]); break;
      case 0x21: read_adcs(nargs, &buffer[3]); break;
      case 0x31: scan_i2c(nargs, &buffer[3]); break;
      case 0x32: read_i2c(nargs, &buffer[3]); break;
      case 0x33: write_i2c(nargs, &buffer[3]); break;
      case 0xA1: lock_sn(nargs, &buffer[3]); break;
      case 0xA2: unlock_sn(nargs, &buffer[3]); break;
      case 0xA3: write_sn(nargs, &buffer[3]); break;
      default: invalid_command(nargs, &buffer[3]);
    }
    
    is_ready = false;
    idx = 0;
    buffer[0] = buffer[1] = buffer[2] = 0;
    
  } else if( in_progress && millis() - last_char > CMD_TIMEOUT_MS ) {
    // Reset the command buffer
    in_progress = false;
    is_ready = false;
    idx = 0;
  }
}
