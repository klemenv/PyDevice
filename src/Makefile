# Makefile
TOP = ..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

CXXFLAGS += -g -ggdb -O0
#CXXFLAGS += $(shell python-config --cflags)
CXXFLAGS += -I/usr/include/python2.7 -I/usr/include/x86_64-linux-gnu/python2.7  -fno-strict-aliasing -Wdate-time -D_FORTIFY_SOURCE=2 -g -fdebug-prefix-map=/build/python2.7-5Z483E/python2.7-2.7.17=. -fstack-protector-strong -Wformat -Werror=format-security  -DNDEBUG -g -fwrapv -Wstrict-prototypes
LDDFLAGS += $(shell python-config --ldflags)

LIBRARY_IOC += pydev

DBD += pydev.dbd

pydev_SRCS += pymodule.cpp
pydev_SRCS += epicsdevice.cpp

pydevice_LIBS += $(EPICS_BASE_IOC_LIBS)

USR_CXXFLAGS += -std=c++11 -fpermissive

include $(TOP)/configure/RULES

