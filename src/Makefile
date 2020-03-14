# Makefile
TOP = ..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

LDDFLAGS += $(shell python-config --ldflags)
CXXFLAGS += $(shell python-config --cflags)
CXXFLAGS += -std=c++11
#CXXFLAGS += -DLINK_VALUE_NEEDLE=\"%value%\"
#CXXFLAGS += -g -ggdb -O0

LIBRARY_IOC += pydev

DBD += pydev.dbd

pydev_SRCS += pyworker.cpp
pydev_SRCS += epicsdevice.cpp
pydev_SRCS += asyncexec.cpp

pydevice_LIBS += $(EPICS_BASE_IOC_LIBS)

USR_CXXFLAGS += -std=c++11 -fpermissive

include $(TOP)/configure/RULES

