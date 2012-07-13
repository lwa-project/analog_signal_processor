#------------------------------------------------------------------------------
# Main Targets
#------------------------------------------------------------------------------
all: LIBSUB initARXDevices sendARXDevice writeARXLCD \
     countBoards countPSUs countThermometers \
     readPSUs readThermometers \
     updatePython

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

writeARXLCD: writeARXLCD.o
	$(CC) -o $@ $^ $(LDFLAGS)

countBoards: countBoards.o
	$(CC) -o $@ $^ $(LDFLAGS)

countPSUs: countPSUs.o
	$(CC) -o $@ $^ $(LDFLAGS)

countThermometers: countThermometers.o
	$(CC) -o $@ $^ $(LDFLAGS)

readPSUs: readPSUs.o
	$(CC) -o $@ $^ $(LDFLAGS)

readThermometers: readThermometers.o
	$(CC) -o $@ $^ $(LDFLAGS)

updatePython: spiCommon.h
	cat spiCommon.h | grep STANDS | sed -e 's/.*BOARD //;'

install:
	cp sendARXDevice /usr/local/bin
	cp initARXDevices /usr/local/bin
	cp writeARXLCD /usr/local/bin
	cp countBoards /usr/local/bin
	cp countPSUs /usr/local/bin
	cp countThermometers /usr/local/bin
	cp readPSUs /usr/local/bin
	cp readThermometers /usr/local/bin
	chown root:root /usr/local/bin/sendARXDevice /usr/local/bin/initARXDevices /usr/local/bin/writeARXLCD \
                        /usr/local/bin/countBoards /usr/local/bin/countPSUs /usr/local/bin/countThermometers \
                        /usr/local/bin/readPSUs /usr/local/bin/readThermometers
	chmod +s /usr/local/bin/sendARXDevice /usr/local/bin/initARXDevices /usr/local/bin/writeARXLCD \
                 /usr/local/bin/countBoards /usr/local/bin/countPSUs /usr/local/bin/countThermometers \
                 /usr/local/bin/readPSUs /usr/local/bin/readThermometers

clean:
	rm -f *.o *.out *.err *.exe *.a *.so
	rm -f sendARXDevice initARXDevices writeARXLCD countBoards countPSUs countThermometers readPSUs readThermometers
	make -C libsub clean

