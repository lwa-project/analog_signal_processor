#------------------------------------------------------------------------------
# Main Targets
#------------------------------------------------------------------------------
all: LIBATMEGA sendARXDevice \
     countBoards countPSUs countThermometers \
     readPSU readThermometers \
     onoffPSU configPSU \
     readARXDevice \
		 atmegaConfig

#------------------------------------------------------------------------------
# Config
#------------------------------------------------------------------------------
OS = $(shell uname)

#------------------------------------------------------------------------------
# Compiler
#------------------------------------------------------------------------------

# C++ Compiler and Linker Executable
CXX     := $(CROSS)g++


#------------------------------------------------------------------------------
# Inc && Lib 
#------------------------------------------------------------------------------
LDLIB   := 

#------------------------------------------------------------------------------
# Compiler Flags 
#------------------------------------------------------------------------------
CXXFLAGS  := -O2 -Wall -std=c++17 

#------------------------------------------------------------------------------
# Linker Flags 
#------------------------------------------------------------------------------
LDFLAGS := -latmega -Llibatmega
CXXFLAGS += -Ilibatmega
LDFLAGS += -lm

#------------------------------------------------------------------------------
# Common rules
#------------------------------------------------------------------------------
%.o:	%.cpp 
	$(CXX) -c $(CXXFLAGS) -o $@ $<


#------------------------------------------------------------------------------
# Target Builds
# 
#------------------------------------------------------------------------------
LIBSUB: 
	make -C libatmega

sendARXDevice: sendARXDevice.o aspCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

countBoards: countBoards.o aspCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

countPSUs: countPSUs.o aspCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

countThermometers: countThermometers.o aspCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

readPSU: readPSU.o aspCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

readThermometers: readThermometers.o aspCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

onoffPSU: onoffPSU.o aspCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

configPSU: configPSU.o aspCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

readARXDevice: readARXDevice.o aspCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)
	
atmegaConfig: atmegaConfig.cpp
	python3 setup.py build_ext --inplace

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
	make -C libatmega clean
