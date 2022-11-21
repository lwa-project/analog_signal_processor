#------------------------------------------------------------------------------
# Main Targets
#------------------------------------------------------------------------------
all: LIBSUB sendARXDevice \
     countBoards countPSUs countThermometers \
     readPSU readThermometers \
     onoffPSU configPSU \
     readARXDevice

#------------------------------------------------------------------------------
# Config
#------------------------------------------------------------------------------
OS = $(shell uname)

#------------------------------------------------------------------------------
# Compiler
#------------------------------------------------------------------------------

# C/C++ Compiler and Linker Executable
CC      := $(CROSS)gcc
CXX     := $(CROSS)g++


#------------------------------------------------------------------------------
# Inc && Lib 
#------------------------------------------------------------------------------
LDLIB   := 

#------------------------------------------------------------------------------
# Compiler Flags 
#------------------------------------------------------------------------------
CFLAGS  := -O2 -Wall

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
%.o:	%.cpp 
	$(CXX) -c $(CFLAGS) -o $@ $<


#------------------------------------------------------------------------------
# Target Builds
# 
#------------------------------------------------------------------------------
LIBSUB: 
	make -C libsub

sendARXDevice: sendARXDevice.o
	$(CXX) -o $@ $^ $(LDFLAGS)

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

configPSU: configPSU.o
	$(CC) -o $@ $^ $(LDFLAGS)

readARXDevice: readARXDevice.o
	$(CC) -o $@ $^ $(LDFLAGS)

install:
	cp sendARXDevice /usr/local/bin
	cp countBoards /usr/local/bin
	cp countPSUs /usr/local/bin
	cp countThermometers /usr/local/bin
	cp readPSU /usr/local/bin
	cp readThermometers /usr/local/bin
	cp onoffPSU /usr/local/bin
	cp configPSU /usr/local/bin
	cp readARXDevice /usr/local/bin
	chown root:root /usr/local/bin/sendARXDevice \
                        /usr/local/bin/countBoards /usr/local/bin/countPSUs /usr/local/bin/countThermometers \
                        /usr/local/bin/readPSU /usr/local/bin/readThermometers \
                        /usr/local/bin/onoffPSU /usr/local/bin/configPSU \
                        /usr/local/bin/readARXDevice
	chmod +s /usr/local/bin/sendARXDevice \
                 /usr/local/bin/countBoards /usr/local/bin/countPSUs /usr/local/bin/countThermometers \
                 /usr/local/bin/readPSU /usr/local/bin/readThermometers \
                 /usr/local/bin/onoffPSU /usr/local/bin/configPSU \
                 /usr/local/bin/readARXDevice

clean:
	rm -f *.o *.out *.err *.exe *.a *.so
	rm -f sendARXDevice countBoards countPSUs countThermometers readPSU readThermometers onoffPSU configPSU readARXDevice
	make -C libsub clean
