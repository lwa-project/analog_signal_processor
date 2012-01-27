#------------------------------------------------------------------------------
# Main Targets
#------------------------------------------------------------------------------
all: LIBSUB initARXDevices sendARXDevice writeARXLCD

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

install:
	cp sendARXDevice /usr/local/bin
	cp initARXDevices /usr/local/bin
	cp writeARXLCD /usr/local/bin
	chown root:root /usr/local/bin/sendARXDevice /usr/local/bin/initARXDevices /usr/local/bin/writeARXLCD
	chmod +s /usr/local/bin/sendARXDevice /usr/local/bin/initARXDevices /usr/local/bin/writeARXLCD

clean:
	rm -f *.o *.out *.err *.exe *.a *.so
	rm -f sendARXDevice initARXDevices writeARXLCD
	make -C libsub clean

