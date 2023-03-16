#include <SPI.h>
#include <EEPROM.h>
#include <Wire.h>

#define SPI_SS_PIN 10

#define MAX_CMD_LEN 130
#define MAX_SN_LEN 8

#define VERSION "v0.0.0"

char device_sn[MAX_SN_LEN] = {'\0'};
uint8_t command = 0x00;
uint16_t nargs = 0;
uint8_t argv[MAX_CMD_LEN] = {0};
uint16_t i;
uint8_t locked = 1;

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

void invalid(uint16_t nargs, uint8_t* argv) {
  // Invalid command
  Serial.write((uint8_t) 0xFF);
  serial_write16(0);
}

void read_sn(uint16_t nargs, uint8_t* argv) {
  // Read the MAX_SN_LEN character device serial number from EEPROM and return it
  //   Input: 0 arguments
  //   Ouput: MAX_SN_LEN char
  for(i=0; i<MAX_SN_LEN; i++) {
    device_sn[i] = (char) EEPROM.read(i);
  }
  Serial.write((uint8_t) 0);
  serial_write16(MAX_SN_LEN);
  Serial.write((uint8_t*) &device_sn[0], MAX_SN_LEN);
}

void read_version(uint16_t nargs, uint8_t* argv) {
  // Return the version string for this firmware
  //   Input: 0 arguments
  //   Output: strlen(VERSION) characters
  const char* version = VERSION;
  Serial.write((uint8_t) 0);
  serial_write16(strlen(VERSION));
  Serial.write((uint8_t*) &version[0], strlen(VERSION));
}

void read_max_cmd_len(uint16_t nargs, uint8_t* argv) {
  // Return the maximum payload size for a command
  //   Input: 0 arguments
  //   Output: uint16_t of maximum command length
  uint16_t value = MAX_CMD_LEN;
  Serial.write((uint8_t) 0);
  serial_write16(sizeof(uint16_t));
  serial_write16(value);
}
  
void echo(uint16_t nargs, uint8_t* argv) {
  // Echo the arguments back to the sender
  //   Input: N arguments (N >= 0)
  //   Output: N arguments provided
  Serial.write((uint8_t) 0);
  serial_write16(nargs);
  Serial.write(argv, nargs);
}

void transfer_spi(uint16_t nargs, uint8_t* argv) {
  // Transfer the given data over SPI and return the full response from the bus
  //   Input: N arguments (N >= 1)
  //   Output: N responses from the SPI transfer
  if( nargs == 0 ) {
    invalid(nargs, argv);
  } else {
    SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE3));
    digitalWrite(SPI_SS_PIN, LOW);
    SPI.transfer(argv, nargs);
    digitalWrite(SPI_SS_PIN, HIGH);
    SPI.endTransaction();
  
    Serial.write((uint8_t) 0);
    serial_write16(nargs);
    Serial.write(argv, nargs);
  }
}

void read_adcs(uint16_t nargs, uint8_t* argv) {
  // Read the first four ADCs and return the unintepretted values in the range of [0,1023]
  //   Input: 0 arguments
  //   Output: Four uint32_t values in the order of A0, A1, A2, and A3
  uint32_t value[4] = {0};
  value[0] = analogRead(A0);
  value[1] = analogRead(A1);
  value[2] = analogRead(A2);
  value[3] = analogRead(A3);
  
  Serial.write((uint8_t) 0);
  serial_write16(4*sizeof(uint32_t));
  serial_write32(value[0]);
  serial_write32(value[1]);
  serial_write32(value[2]);
  serial_write32(value[3]);
}

void scan_i2c(uint16_t nargs, uint8_t* argv) {
  // Scan the I2C bus and return all of the valid addresses found
  //   Input: 0 arguments
  //   Output:  N uint8_t values for device addressed (N >= 0)
  int ndevice = 0;
  byte addr, err;
  byte found_addr[128];
  for(addr=0; addr<127; addr++) {
    Wire.beginTransmission(addr);
    err = Wire.endTransmission();
    if( err == 0 ) {
      found_addr[ndevice] = addr;
      ndevice++;
    }
  }

  Serial.write((uint8_t) 0);
  serial_write16(ndevice);
  if( ndevice > 0 ) {
    Serial.write((uint8_t*) &found_addr, ndevice*sizeof(byte));
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
    invalid(nargs, argv);
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
      Serial.write((uint8_t) 0);
      serial_write16(size);
      Serial.write(argv, size);
    } else {
      invalid(nargs, argv);
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
    invalid(nargs, argv);
  } else {
    byte err;
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.endTransmission(false);

    delay(5);
    
    Wire.write((uint8_t*) &(argv[2]), size);
    err = Wire.endTransmission();
    if( err == 0 ) {
      Serial.write((uint8_t) 0);
      serial_write16(size);
      Serial.write((uint8_t*) argv, size);
    } else {
      invalid(nargs, argv);
    }
  }
}

void unlock_sn(uint16_t nargs, uint8_t* argv) {
  // Unlock the write_sn function
  //   Input: 0 arguments
  //   Output:  0 values
  locked = 0;

  Serial.write((uint8_t) 0);
  serial_write16(0);
}

void lock_sn(uint16_t nargs, uint8_t* argv) {
  // Lock the write_sn function
  //   Input: 0 arguments
  //   Output:  0 values
  locked = 1;

  Serial.write((uint8_t) 0);
  serial_write16(0);
}

void write_sn(uint16_t nargs, uint8_t* argv) {
  // Write the at most MAX_SN_LEN character device serial number to EEPROM and return it
  //   Input: N char (0 < N <= MAX_SN_LEN)
  //   Output: MAX_SN_LEN char (see read_sn)
  if( (nargs == 0) || (locked == 1) ) {
    invalid(nargs, argv);
  } else {
    for(i=0; i<MAX_SN_LEN; i++) {
      if(i < nargs) {
        EEPROM.write(i, argv[i]);
      } else {
        EEPROM.write(i, 0);
      }
    }
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
  command = 0x00;
  nargs = 0;
  if( Serial.available() > 3 ) {
    Serial.readBytes(&command, 1);
    Serial.readBytes((uint8_t*) &nargs, 2);
    nargs = bswap16(nargs);
    
    if( nargs > 0 ) {
      if( nargs < MAX_CMD_LEN ) {
        Serial.readBytes(&argv[0], nargs);
      } else {
        while( Serial.available() > 0 ) {
          Serial.read();
        }
        command = 0xFF;
      }
    }
  }
  
  // Process the command
  switch(command) {
    case 0x00: break;
    case 0x01: read_sn(nargs, &argv[0]); break;
    case 0x02: read_version(nargs, &argv[0]); break; 
    case 0x03: read_max_cmd_len(nargs, &argv[0]); break; 
    case 0x04: echo(nargs, &argv[0]); break;
    case 0x11: transfer_spi(nargs, &argv[0]); break;
    case 0x21: read_adcs(nargs, &argv[0]); break;
    case 0x31: scan_i2c(nargs, &argv[0]); break;
    case 0x32: read_i2c(nargs, &argv[0]); break;
    case 0x33: write_i2c(nargs, &argv[0]); break;
    case 0xA1: lock_sn(nargs, &argv[0]); break;
    case 0xA2: unlock_sn(nargs, &argv[0]); break;
    case 0xA3: write_sn(nargs, &argv[0]); break;
    default: invalid(nargs, &argv[0]);
  }
  Serial.flush();
}
