#------------------------------------------------------------------------------
# Main Targets
#------------------------------------------------------------------------------
all: LIBSUB writeARXLCD \
     countPSUs countThermometers \
     readPSU readThermometers \
     onoffPSU configPSU

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


#------------------------------------------------------------------------------
# Target Builds
# 
#------------------------------------------------------------------------------
LIBSUB: 
	make -C libsub

writeARXLCD: writeARXLCD.o
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

install:
	cp writeARXLCD /usr/local/bin
	cp countPSUs /usr/local/bin
	cp countThermometers /usr/local/bin
	cp readPSU /usr/local/bin
	cp readThermometers /usr/local/bin
	cp onoffPSU /usr/local/bin
	cp configPSU /usr/local/bin
	chown root:root /usr/local/bin/writeARXLCD \
                        /usr/local/bin/countPSUs /usr/local/bin/countThermometers \
                        /usr/local/bin/readPSU /usr/local/bin/readThermometers \
                        /usr/local/bin/onoffPSU /usr/local/bin/configPSU
	chmod +s /usr/local/bin/writeARXLCD \
                 /usr/local/bin/countPSUs /usr/local/bin/countThermometers \
                 /usr/local/bin/readPSU /usr/local/bin/readThermometers \
                 /usr/local/bin/onoffPSU /usr/local/bin/configPSU

clean:
	rm -f *.o *.out *.err *.exe *.a *.so
	rm -f sendARXDevice writeARXLCD countBoards countPSUs countThermometers readPSU readThermometers onoffPSU configPSU readARXDevice
	make -C libsub clean

