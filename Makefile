#------------------------------------------------------------------------------
# Main Targets
#------------------------------------------------------------------------------
all: LIBSUB initARXDevices sendARXDevice sendARXDeviceBatch writeARXLCD \
     countBoards countPSUs countThermometers \
     readPSU readThermometers \
     onoffPSU \
     readARXDevice

#------------------------------------------------------------------------------
# Config
#------------------------------------------------------------------------------
OS = $(shell uname)

#------------------------------------------------------------------------------
# Compiler
#------------------------------------------------------------------------------

# C Compiler and Linker Executable
CC      := $(CROSS)gcc


#------------------------------------------------------------------------------
# Inc && Lib 
#------------------------------------------------------------------------------
LDLIB   := 

#------------------------------------------------------------------------------
# Compiler Flags 
#------------------------------------------------------------------------------
CFLAGS  := -O2 -Wall -g

#------------------------------------------------------------------------------
# Linker Flags 
#------------------------------------------------------------------------------
LDFLAGS := -lsub -Llibsub
CFLAGS += -Ilibsub
LDFLAGS += -L/usr/local/lib -lusb-1.0 -lm

#------------------------------------------------------------------------------
# Common rules
#------------------------------------------------------------------------------
%.o:	%.c  
	$(CC) -c $(CFLAGS) -o $@ $<


#------------------------------------------------------------------------------
# Target Builds
# 
#------------------------------------------------------------------------------
LIBSUB: 
	make -C libsub

initARXDevices: initARXDevices.o
	$(CC) -o $@ $^ $(LDFLAGS)

sendARXDevice: sendARXDevice.o
	$(CC) -o $@ $^ $(LDFLAGS)

sendARXDeviceBatch: sendARXDeviceBatch.o
	$(CC) -o $@ $^ $(LDFLAGS)

writeARXLCD: writeARXLCD.o
	$(CC) -o $@ $^ $(LDFLAGS)

countBoards: countBoards.o
	$(CC) -o $@ $^ $(LDFLAGS)

countPSUs: countPSUs.o
	$(CC) -o $@ $^ $(LDFLAGS)

countThermometers: countThermometers.o
	$(CC) -o $@ $^ $(LDFLAGS)

readPSU: readPSU.o
	$(CC) -o $@ $^ $(LDFLAGS)

readThermometers: readThermometers.o
	$(CC) -o $@ $^ $(LDFLAGS)

onoffPSU: onoffPSU.o
	$(CC) -o $@ $^ $(LDFLAGS)

readARXDevice: readARXDevice.o
	$(CC) -o $@ $^ $(LDFLAGS)

install:
	cp sendARXDevice /usr/local/bin
	cp sendARXDeviceBatch /usr/local/bin
	cp initARXDevices /usr/local/bin
	cp writeARXLCD /usr/local/bin
	cp countBoards /usr/local/bin
	cp countPSUs /usr/local/bin
	cp countThermometers /usr/local/bin
	cp readPSU /usr/local/bin
	cp readThermometers /usr/local/bin
	cp onoffPSU /usr/local/bin
	cp readARXDevice /usr/local/bin
	chown root:root /usr/local/bin/sendARXDevice /usr/local/bin/sendARXDeviceBatch /usr/local/bin/initARXDevices /usr/local/bin/writeARXLCD \
                        /usr/local/bin/countBoards /usr/local/bin/countPSUs /usr/local/bin/countThermometers \
                        /usr/local/bin/readPSU /usr/local/bin/readThermometers \
                        /usr/local/bin/onoffPSU \
                        /usr/local/bin/readARXDevice
	chmod +s /usr/local/bin/sendARXDevice /usr/local/bin/sendARXDeviceBatch /usr/local/bin/initARXDevices /usr/local/bin/writeARXLCD \
                 /usr/local/bin/countBoards /usr/local/bin/countPSUs /usr/local/bin/countThermometers \
                 /usr/local/bin/readPSU /usr/local/bin/readThermometers \
                 /usr/local/bin/onoffPSU \
                 /usr/local/bin/readARXDevice

clean:
	rm -f *.o *.out *.err *.exe *.a *.so
	rm -f sendARXDevice sendARXDeviceBatch initARXDevices writeARXLCD countBoards countPSUs countThermometers readPSUs readThermometers onoffPSU
	make -C libsub clean

