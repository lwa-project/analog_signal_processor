#include <Wire.h>

void setup() {
  Wire.begin();
  Serial.begin(9600);
}

// Read the value stored at register "in" and return it via "out".  If an error
// occurs during reading "READ ERROR" is printed.
inline void sub_i2c_read(int device, \
                         int in, int in_size, \
                         int* out, int out_size) {
  Wire.beginTransmission(device);
  Wire.write(byte(in));
  int status = Wire.endTransmission();
  if(status != 0) {
    Serial.println("READ ERROR");
  }
  
  *out = 0;
  Wire.requestFrom(device, out_size);
  if(out_size <= Wire.available()) {
    for(int i=0; i<out_size; i++) {
      *out |= Wire.read() << (8*i);
    }
  }
}

// Write "data" to register "in".  If an error occurs during writing "WRITE
// ERROR" is printed.
inline void sub_i2c_write(int device, \
                          int in, int in_size, \
                          int* data, int data_size) {
  Wire.beginTransmission(device);
  for(int i=0; i<in_size; i++) {
    Wire.write(byte((in >> (8*i)) & 0xFF));
  }
  for(int i=0; i<data_size; i++) {
    Wire.write(byte((*data >> (8*i)) & 0xFF));
  }
  int status = Wire.endTransmission();
  if(status != 0) {
    Serial.println("WRITE ERROR");
  }
}

// Enable register writing for the device
inline void enable_register_write(int device) {
  int data = 0;
  sub_i2c_write(device, 0x10, 1, &data, 1);
}

// Disable register writing (except for the register that controls 
// register writing) for the device
inline void disable_register_write(int device) {
  int data = (1 << 7) | 1;
  sub_i2c_write(device, 0x10, 1, &data, 1);
}

int device_address = 0x1F;
int simple_data = 0;

void loop() {
  if (Serial.available() > 0) {
    String command = Serial.readStringUntil('\n');
    
    if(command == "off") {
      // Turn off the outputs
      enable_register_write(device_address);
      simple_data = 0;
      sub_i2c_write(device_address, 0x01, 1, &simple_data, 1);
      disable_register_write(device_address);
      delay(20);
    } else if(command == "on") {
      // Turn on the outputs
      enable_register_write(device_address);
      simple_data = 1 << 7;
      sub_i2c_write(device_address, 0x01, 1, &simple_data, 1);
      disable_register_write(device_address);
      delay(20);
    } else if(command == "current") {
      // Report the input current in amps
      simple_data = 0;
      sub_i2c_read(device_address, 0x89, 1, &simple_data, 2);
      Serial.print("Input current: ");
      Serial.println(simple_data/100.0 * 0.95);
    } else if(command == "fan") {
      // Report the fan speed in RPM
      simple_data = 0;
      sub_i2c_read(device_address, 0x90, 1, &simple_data, 2);
      Serial.print("Fan speed: ");
      Serial.println(simple_data*10);
    } else if(command.startsWith("device")) {
      // Query/set the device address that we are talking to
      //   No arguments - query
      //   One integer - set
      if(command.length() > 6) {
        String address = command.substring(6);
        int new_device_address = address.toInt();
        if(new_device_address > 0) {
          device_address = new_device_address;
        }
      }
      Serial.print("Address: 0x");
      if(device_address < 16) {
        Serial.print("0");
      }
      Serial.println(device_address, HEX);
    } else if(command == "global") {
      // Disable output on startup - there are some caveats about how this
      // works in that it seems to save the current output state.
      enable_register_write(device_address);
      simple_data = 0;
      sub_i2c_read(device_address, 0xD5, 1, &simple_data, 1);
      simple_data &= 127;
      sub_i2c_write(device_address, 0xD5, 1, &simple_data, 1);
      sub_i2c_write(device_address, 0x15, 1, &simple_data, 0);
      disable_register_write(device_address);
      Serial.println("Global inhibit at power up on");
    } else if(command == "help") {
      // Display what commands are avaliable
      Serial.println("Valid commands: off on current device fan global help summary temperatures voltage");
    } else if(command == "summary") {
      // Report on the module statuses as module name, on/off status, and
      // status/error code.
      simple_data = 0;
      sub_i2c_read(device_address, 0xD3, 1, &simple_data, 2);
      int modules = simple_data;
      enable_register_write(device_address);
      for(int j=0; j<16; j++) {
        if((modules >> j) & 1 == 1) {
          sub_i2c_write(device_address, 0x00, 1, &j, 1);
          delay(10);
          sub_i2c_read(device_address, 0xDB, 1, &simple_data, 1);
          Serial.print("Module ");
          Serial.print(j);
          Serial.print(": ");
          if(simple_data & 1) {
            Serial.print("On ");
          } else {
            Serial.print("Off ");
          }
          if(simple_data & 2) {
            Serial.print("UnderVolt ");
          } else if(simple_data & 4) {
            Serial.print("Ok ");
          } else if(simple_data & 8) {
            Serial.print("OverCurrent");
          } else if(simple_data & 16) {
            Serial.print("OverTemperature");
          } else if(simple_data & 32) {
            Serial.print("WarningTemperature");
          } else if(simple_data & 64) {
            Serial.print("OverVolt");
          } else if(simple_data & 128) {
            Serial.print("ModuleFault");
          } else {
            Serial.print("Unknown");
          }
          Serial.println("");
        }
      }
      disable_register_write(device_address);
    } else if(command == "temperatures") {
      // Report on the case and primary side temperatures in C
      simple_data = 0;
      sub_i2c_read(device_address, 0x8D, 1, &simple_data, 2);
      Serial.print("Temperatures: ");
      Serial.print(simple_data/4.0);
      Serial.print(" ");
      sub_i2c_read(device_address, 0x8E, 1, &simple_data, 2);
      Serial.println(simple_data/4.0);
    } else if(command == "voltage") {
      // Report the input voltage in volts
      simple_data = 0;
      sub_i2c_read(device_address, 0x88, 1, &simple_data, 2);
      Serial.print("Input voltage: ");
      Serial.println(simple_data/100.0);
    } else {
      Serial.print("Unknown command: ");
      Serial.println(command);
    }

    // Report the overall device output state
    sub_i2c_read(device_address, 0x01, 1, &simple_data, 1);
    Serial.print("Power status: ");
    if((simple_data >> 7) & 1) {
      Serial.println("On");
    } else {
      Serial.println("Off");
    }
  }
}
