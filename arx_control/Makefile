#------------------------------------------------------------------------------
# Main Targets
#------------------------------------------------------------------------------
all: sendARXDevice sendPICDevice \
     countBoards countPSUs countThermometers countPICs \
     readPSU readThermometers \
     onoffPSU configPSU \
     readARXDevice setATmegaSN listATmegaSN locateATmega \
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
LDFLAGS := -Llibatmega -latmega_static
CXXFLAGS += -Ilibatmega
LDFLAGS += -lm -lpthread
ifeq ($(OS),Linux)
  LDFLAGS += -ludev
else ifeq ($(OS),Darwin)
  LDFLAGS += -framework IOKit -framework CoreFoundation
endif

#------------------------------------------------------------------------------
# libatmega dependencies
#------------------------------------------------------------------------------
LIBATMEGA_LIB := libatmega/libatmega_static.a
LIBATMEGA_HDR := libatmega/libatmega.hpp

#------------------------------------------------------------------------------
# Common rules
#------------------------------------------------------------------------------
%.o: %.cpp $(LIBATMEGA_HDR)
	$(CXX) -c $(CXXFLAGS) -o $@ $<


#------------------------------------------------------------------------------
# Target Builds
# 
#------------------------------------------------------------------------------
$(LIBATMEGA_LIB): libatmega/libatmega.cpp $(LIBATMEGA_HDR)
	$(MAKE) -C libatmega

sendARXDevice: sendARXDevice.o aspCommon.o $(LIBATMEGA_LIB)
	$(CXX) -o $@ $(filter %.o,$^) $(LDFLAGS)

sendPICDevice: sendPICDevice.o aspCommon.o $(LIBATMEGA_LIB)
	$(CXX) -o $@ $(filter %.o,$^) $(LDFLAGS)

countBoards: countBoards.o aspCommon.o $(LIBATMEGA_LIB)
	$(CXX) -o $@ $(filter %.o,$^) $(LDFLAGS)

countPSUs: countPSUs.o aspCommon.o ivsCommon.o $(LIBATMEGA_LIB)
	$(CXX) -o $@ $(filter %.o,$^) $(LDFLAGS)

countThermometers: countThermometers.o aspCommon.o ivsCommon.o $(LIBATMEGA_LIB)
	$(CXX) -o $@ $(filter %.o,$^) $(LDFLAGS)

countPICs: countPICs.o aspCommon.o $(LIBATMEGA_LIB)
	$(CXX) -o $@ $(filter %.o,$^) $(LDFLAGS)

readPSU: readPSU.o aspCommon.o ivsCommon.o $(LIBATMEGA_LIB)
	$(CXX) -o $@ $(filter %.o,$^) $(LDFLAGS)

readThermometers: readThermometers.o aspCommon.o ivsCommon.o $(LIBATMEGA_LIB)
	$(CXX) -o $@ $(filter %.o,$^) $(LDFLAGS)

onoffPSU: onoffPSU.o aspCommon.o ivsCommon.o $(LIBATMEGA_LIB)
	$(CXX) -o $@ $(filter %.o,$^) $(LDFLAGS)

configPSU: configPSU.o aspCommon.o ivsCommon.o $(LIBATMEGA_LIB)
	$(CXX) -o $@ $(filter %.o,$^) $(LDFLAGS)

readARXDevice: readARXDevice.o aspCommon.o $(LIBATMEGA_LIB)
	$(CXX) -o $@ $(filter %.o,$^) $(LDFLAGS)

setATmegaSN: setATmegaSN.o $(LIBATMEGA_LIB)
	$(CXX) -o $@ $(filter %.o,$^) $(LDFLAGS)

listATmegaSN: listATmegaSN.o $(LIBATMEGA_LIB)
	$(CXX) -o $@ $(filter %.o,$^) $(LDFLAGS)

locateATmega: locateATmega.o aspCommon.o $(LIBATMEGA_LIB)
	$(CXX) -o $@ $(filter %.o,$^) $(LDFLAGS)
		
atmegaConfig: atmegaConfig.cpp
	python3 setup.py build_ext --inplace

install:
	rm -f /dev/shm/sem.* /dev/shm/arx_*.lock /tmp/arx_*.lock
	cp sendARXDevice /usr/local/bin
	cp sendPICDevice /usr/local/bin
	cp countBoards /usr/local/bin
	cp countPSUs /usr/local/bin
	cp countThermometers /usr/local/bin
	cp countPICs /usr/local/bin
	cp readPSU /usr/local/bin
	cp readThermometers /usr/local/bin
	cp onoffPSU /usr/local/bin
	cp configPSU /usr/local/bin
	cp readARXDevice /usr/local/bin
	cp locateATmega /usr/local/bin
	cp listATmegaSN /usr/local/bin

clean:
	rm -f *.o *.out *.err *.exe *.a *.so *.dylib
	rm -f sendARXDevice sendPICDevice \
	countBoards countPSUs countThermometers countPICs \
	readPSU readThermometers onoffPSU configPSU readARXDevice \
	setATmegaSN listATmegaSN locateATmega
	make -C libatmega clean
