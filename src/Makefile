# Makefile
TOP = ..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

LDFLAGS += $(shell python-config --ldflags)
CXXFLAGS += $(subst -Wstrict-prototypes,,$(shell python-config --cflags))
CXXFLAGS += -std=c++11
#CXXFLAGS += -DLINK_VALUE_NEEDLE=\"%value%\"
#CXXFLAGS += -g -ggdb -O0

LIBRARY_IOC += pydev

DBD += pydev.dbd

pydev_SRCS += pywrapper.cpp
pydev_SRCS += epicsdevice.cpp
pydev_SRCS += asyncexec.cpp

pydevice_LIBS += $(EPICS_BASE_IOC_LIBS)

USR_CXXFLAGS += -std=c++11 -fpermissive

include $(TOP)/configure/RULES

