#include <SPI.h>
#include <Wire.h>
#include <FlashStorage_SAMD.h>
#include <SoftwareSerial.h>

// Chip select pin for SPI operations
#define SPI_SS_PIN D3

// Maximum command length
#define MAX_CMD_LEN 530

// Maximum serial number length
#define MAX_SN_LEN 8

// Timeout in ms before flushing the command input buffer
#define CMD_TIMEOUT_MS 50

// Version string
#define VERSION "v0.0.1"

// RS485 RX/TX enable pin
#define RS485_EN D2

// RS485 RX timeout in ms
#define RS485_TIMEOUT_MS 1000

// Fault LED pin
#define FAULT_PIN D0

// Locate LED pin
#define LOCATE_PIN D1

SoftwareSerial SoftSerial1(D7, D6);

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
uint8_t locate = 0;
uint8_t lstate = 0;

void serial_sendresp(uint8_t status, uint16_t sze, uint8_t *data) {
  // Send a response to a command
  /* Preamble */
  Serial.write((uint8_t) 60);
  Serial.write((uint8_t) 60);
  Serial.write((uint8_t) 60);
  /* Status code */
  Serial.write(status);
  /* Message size */
  Serial.write((uint8_t*) &sze, sizeof(uint16_t));
  /* Message body */
  if( data != NULL && sze > 0 ) {
    Serial.write((uint8_t*) data, sze);
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

void invalid_rs485_command(uint16_t nargs, uint8_t* argv) {
  // Invalid RS485 ARX board command
  serial_sendresp(0xFD, 0, NULL);
}

void timeout_rs485_command(uint16_t nargs, uint8_t* argv) {
  // Timeout of a RS485 ARX board command
  serial_sendresp(0xFE, 0, NULL);
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

void scan_rs485(uint16_t nargs, uint8_t* argv) {
  // Scan the RS485 bus to look for ARX boards and return all of
  // the valid addresses found
  //   Input: 0 arguments
  //   Output: N uint8_t values for device addressed (N >= 0)
  int ndevice = 0;
  byte addr;
  char cmd[8] = "ECHOAAA";
  byte found_addr[126];
  if( nargs > 0) {
    invalid_arguments(nargs, argv);
  } else {
    for(addr=1; addr<127; addr++) {
      SoftSerial1.flush();
      
      digitalWrite(RS485_EN, HIGH);
      delayMicroseconds(250);
      SoftSerial1.write((uint8_t) (0x80 + addr) & 0xFF);
      SoftSerial1.write((uint8_t*) &(cmd[0]), 7);
      SoftSerial1.write('\r');
      SoftSerial1.flush();
      
      digitalWrite(RS485_EN, LOW);

      unsigned long t_start = millis();
      while( (millis() - t_start) < 10 ) {
        if( SoftSerial1.available() > 4 ) {
          found_addr[ndevice++] = addr;
          break;
        }
      }

      delay(5);
    }
    
    serial_sendresp(0, ndevice, (uint8_t*) &found_addr[0]);
  }
}

void read_rs485(uint16_t nargs, uint8_t* argv) {
  // Read from the ARX board address until we get a \r or a timeout
  //  Input: 0 arguments
  //  Output: N uint8_t bytes
  int i = 0;
  byte response[80] = {0};
  if( nargs > 0 ) {
    invalid_arguments(nargs, argv);
  } else {
    unsigned long t_start = millis();
    while( ((millis() - t_start) < RS485_TIMEOUT_MS) && (i < 80) ) {
      if( SoftSerial1.available() > 0 ) {
        response[i] = SoftSerial1.read();
        if( response[i++] == '\r' ) {
          i--;
          break;
        }
      } else {
        delayMicroseconds(250);
      }
    }
    
    if( i == 0 ) {
      timeout_rs485_command(nargs, argv);
    } else {
      if( response[1] != 0x06 ) {
        invalid_rs485_command(nargs, argv);
      } else {
        serial_sendresp(0, i-1, (uint8_t*) &(response[1]));
      }
    }
  }
}

void write_rs485(uint16_t nargs, uint8_t* argv) {
  // Write the given number of bytes to the provided ARX board address
  //   Input: 2+ arguments - address (uint8_t), value (N uint8_t)
  //   Output: 0 values
  byte addr = argv[0];
  uint16_t size = nargs - 1;
  if( nargs < 2 ) {
    invalid_arguments(nargs, argv);
  } else {
    SoftSerial1.flush();
    
    digitalWrite(RS485_EN, HIGH);
    delayMicroseconds(250);
    SoftSerial1.write((uint8_t) (0x80 + addr) & 0xFF);
    SoftSerial1.write((uint8_t*) &(argv[1]), size);
    SoftSerial1.write('\r');
    SoftSerial1.flush();
    
    digitalWrite(RS485_EN, LOW);

    serial_sendresp(0, 0, argv);
  }
}

void send_rs485(uint16_t nargs, uint8_t* argv) {
  // Write the given number of bytes to the provided ARX board address
  //   Input: 2+ arguments - address (uint8_t), value (N uint8_t)
  //   Output: Value written as N uint8_t (N >= 0)
  byte addr = argv[0];
  uint16_t size = nargs - 1;
  int i = 0;
  byte response[80] = {0};
  if( nargs < 2 ) {
    invalid_arguments(nargs, argv);
  } else {
    SoftSerial1.flush();

    digitalWrite(RS485_EN, HIGH);
    delayMicroseconds(250);
    SoftSerial1.write((uint8_t) (0x80 + addr) & 0xFF);
    SoftSerial1.write((uint8_t*) &(argv[1]), size);
    SoftSerial1.write('\r');
    SoftSerial1.flush();
    
    digitalWrite(RS485_EN, LOW);
    
    unsigned long t_start = millis();
    while( ((millis() - t_start) < RS485_TIMEOUT_MS) && (i < 80) ) {
      if( SoftSerial1.available() > 0 ) {
        response[i] = SoftSerial1.read();
        if( response[i++] == '\r' ) {
          i--;
          break;
        }
      } else {
        delayMicroseconds(250);
      }
    }

    if( i == 0 ) {
      if( size == 1) {
         if( argv[1] == 'W' ) {
          // Don't expect anything from W(AKE)
          serial_sendresp(0, 0, (uint8_t*) &(response[0]));
        } else {
          timeout_rs485_command(nargs, argv);
        }
      } else if( size == 4 ) {
        if( (argv[1] == 'R') && (argv[2] == 'S') && (argv[3] == 'E') && (argv[4] == 'T') ) {
          // Don't expect anything from RSET
          serial_sendresp(0, 0, (uint8_t*) &(response[0]));
        } else if( (argv[1] == 'S') && (argv[2] == 'L') && (argv[3] == 'E') && (argv[4] == 'P') ) {
          // Don't expect anything from SLEP
          serial_sendresp(0, 0, (uint8_t*) &(response[0]));
        } else {
          timeout_rs485_command(nargs, argv);
        }
      } else {
        timeout_rs485_command(nargs, argv);
      }
    } else {
      if( response[1] != 0x06 ) {
        invalid_rs485_command(nargs, argv);
      } else {
        serial_sendresp(0, i-1, (uint8_t*) &(response[1]));
      }
    }
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
    for(addr=1; addr<127; addr++) {
      Wire.beginTransmission(addr);
      err = Wire.endTransmission();
      if( err == 0 ) {
        found_addr[ndevice++] = addr;
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
  byte sze = argv[2];
  if( nargs < 3 ) {
    invalid_arguments(nargs, argv);
  } else {
    for(i=0; i<3; i++) {
      argv[i] = 0;
    }
    
    byte err;
    Wire.beginTransmission(addr);  
    Wire.write(reg);
    Wire.endTransmission();

    Wire.requestFrom(addr, sze);
    while( Wire.available() < sze ) {
      delay(1);
    }
    Wire.readBytes(argv, sze);
    err = Wire.endTransmission(); 
    if( err == 0 ) {
      serial_sendresp(0, sze, argv);
    } else {
      digitalWrite(FAULT_PIN, HIGH);
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
    Wire.write((uint8_t*) &(argv[2]), size);
    err = Wire.endTransmission();
    if( err == 0 ) {
      serial_sendresp(0, 0, argv);
    } else {
      digitalWrite(FAULT_PIN, HIGH);
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

void clear_fault(uint16_t nargs, uint8_t* argv) {
  // Clear the fault indicator
  //   Input: 0 arguments
  //   Output: 0 values
  if( nargs > 0) {
    invalid_arguments(nargs, argv);
  } else {
    digitalWrite(FAULT_PIN, LOW);
    
    serial_sendresp(0, 0, NULL);
  }
}

void toggle_locate(uint16_t nargs, uint8_t* argv) {
  // Set/unset the locate LED
  //   Input: 0 arguments
  //   Output: 0 values
  if( nargs > 0) {
    invalid_arguments(nargs, argv);
  } else {
    locate ^= 1;
    digitalWrite(LOCATE_PIN, locate ? HIGH : LOW);
    lstate = locate;
    
    serial_sendresp(0, 0, NULL);
  }
}

void(* reset) (void) = 0; //declare reset function @ address 0

void setup() {
  // Serial setup
  Serial.begin(115200);
  
  // RS485 setup
  SoftSerial1.begin(19200);
  SoftSerial1.setTimeout(RS485_TIMEOUT_MS);
  pinMode(RS485_EN, OUTPUT);
  digitalWrite(RS485_EN, LOW);
  
  // SPI setup
  pinMode(SPI_SS_PIN, OUTPUT);
  SPI.begin();

  // I2C setup
  Wire.setClock(100000);
  Wire.begin();

  // LEDs
  pinMode(FAULT_PIN, OUTPUT);   // Fault
  pinMode(LOCATE_PIN, OUTPUT);   // Locate
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
    nargs = (buffer[2] << 8) | buffer[1];
    
    // Process the command
    switch(command) {
      case 0x00: break;
      case 0x01: read_sn(nargs, &buffer[3]); break;
      case 0x02: read_version(nargs, &buffer[3]); break; 
      case 0x03: read_max_cmd_len(nargs, &buffer[3]); break; 
      case 0x04: echo(nargs, &buffer[3]); break;
      case 0x11: transfer_spi(nargs, &buffer[3]); break;
      case 0x21: scan_rs485(nargs, &buffer[3]); break;
      case 0x22: read_rs485(nargs, &buffer[3]); break;
      case 0x23: write_rs485(nargs, &buffer[3]); break;
      case 0x24: send_rs485(nargs, &buffer[3]); break;
      case 0x31: scan_i2c(nargs, &buffer[3]); break;
      case 0x32: read_i2c(nargs, &buffer[3]); break;
      case 0x33: write_i2c(nargs, &buffer[3]); break;
      case 0xA1: lock_sn(nargs, &buffer[3]); break;
      case 0xA2: unlock_sn(nargs, &buffer[3]); break;
      case 0xA3: write_sn(nargs, &buffer[3]); break;
      case 0xA4: clear_fault(nargs, &buffer[3]); break;
      case 0xA5: toggle_locate(nargs, &buffer[3]); break;
      case 0xAF: reset(); break;
      default: invalid_command(nargs, &buffer[3]);
    }

    in_progress = false;
    is_ready = false;
    idx = 0;
    buffer[0] = buffer[1] = buffer[2] = buffer[3] = 0;
    
  } else if( in_progress && millis() - last_char > CMD_TIMEOUT_MS ) {
    // Reset the command buffer
    in_progress = false;
    is_ready = false;
    idx = 0;
    buffer[0] = buffer[1] = buffer[2] = buffer[3] = 0;
  }

  // Blink the locate LED if locate is requested
  if( locate ) {
    if( ((millis() / 800) % 2) == 0 ) {
      if( lstate == 0 ) {
        digitalWrite(LOCATE_PIN, HIGH);
        lstate = 1;
      }
    } else {
      if( lstate == 1 ) {
        digitalWrite(LOCATE_PIN, LOW);
        lstate = 0;
      }
    }
  }
  
}
