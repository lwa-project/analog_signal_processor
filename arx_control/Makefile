#------------------------------------------------------------------------------
# Main Targets
#------------------------------------------------------------------------------
all: LIBATMEGA sendARXDevice sendPICDevice \
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
# Common rules
#------------------------------------------------------------------------------
%.o: %.cpp 
	$(CXX) -c $(CXXFLAGS) -o $@ $<


#------------------------------------------------------------------------------
# Target Builds
# 
#------------------------------------------------------------------------------
.PHONY: LIBATMEGA
LIBATMEGA: 
	make -C libatmega

sendARXDevice: sendARXDevice.o aspCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

sendPICDevice: sendPICDevice.o aspCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)
		
countBoards: countBoards.o aspCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

countPSUs: countPSUs.o aspCommon.o ivsCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

countThermometers: countThermometers.o aspCommon.o ivsCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

countPICs: countPICs.o aspCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

readPSU: readPSU.o aspCommon.o ivsCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

readThermometers: readThermometers.o aspCommon.o ivsCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

onoffPSU: onoffPSU.o aspCommon.o ivsCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

configPSU: configPSU.o aspCommon.o ivsCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

readARXDevice: readARXDevice.o aspCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)

setATmegaSN: setATmegaSN.o
	$(CXX) -o $@ $^ $(LDFLAGS)

listATmegaSN: listATmegaSN.o
	$(CXX) -o $@ $^ $(LDFLAGS)

locateATmega: locateATmega.o aspCommon.o
	$(CXX) -o $@ $^ $(LDFLAGS)
		
atmegaConfig: atmegaConfig.cpp
	python3 setup.py build_ext --inplace

install:
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
