#------------------------------------------------------------------------------
# Main Targets
#------------------------------------------------------------------------------
all: LIBATMEGA

#------------------------------------------------------------------------------
# Config
#------------------------------------------------------------------------------
OS=$(shell uname)

ifeq ($(OS),Linux)
  SO_EXT      = .so
  SHARED_FLAG = -shared
  SONAME_FLAG = -soname
else ifeq ($(OS),Darwin)
  SO_EXT      = .dylib
  SHARED_FLAG = -dynamiclib
  SONAME_FLAG = -install_name
else
  $(error Unsupported OS)
endif

LIBATMEGA_SO         = libatmega$(SO_EXT)
	
#------------------------------------------------------------------------------
# Compiler
#------------------------------------------------------------------------------

# C++ Compiler and Linker Executable
CXX     := $(CROSS)g++


#------------------------------------------------------------------------------
# Inc && Lib 
#------------------------------------------------------------------------------
ifeq ($(OS),Linux)
  LDLIB += -ludev
else ifeq ($(OS),Darwin)
  LDLIB += -framework IOKit -framework CoreFoundation
endif

#------------------------------------------------------------------------------
# Compiler Flags 
#------------------------------------------------------------------------------
CXXFLAGS  := -O2 -Wall -std=c++17 -Werror -g -fPIC -I.

#------------------------------------------------------------------------------
# Linker Flags 
#------------------------------------------------------------------------------
LFLAGS  := -fPIC
ifeq ($(OS),Linux)
  LFLAGS += -ludev
else ifeq ($(OS),Darwin)
  LFLAGS += -framework IOKit -framework CoreFoundation
endif

WLFLAGS ?= $(SONAME_FLAG),$(LIBATMEGA_SO)

#------------------------------------------------------------------------------
# Common rules
#------------------------------------------------------------------------------
%.o:	%.cpp 
	$(CXX) -c $(CXXFLAGS) -o $@ $<


#------------------------------------------------------------------------------
# Target Builds
# 
#------------------------------------------------------------------------------
LIBATMEGA : libatmega_static.a $(LIBATMEGA_SO)
libatmega_static.a : libatmega.o
	ar -rcsv $@ $^
$(LIBATMEGA_SO): libatmega.o
	$(CXX) $(SHARED_FLAG) -Wl,$(WLFLAGS) -o $@ libatmega.o $(LFLAGS)

clean:
	@echo $(OS) Clean
	rm -f *.o *.a *.so *.dylib
