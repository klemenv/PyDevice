# Makefile
TOP = ..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

USR_LDFLAGS += $(shell python-config --ldflags)
USR_CXXFLAGS += $(subst -Wstrict-prototypes,,$(shell python-config --cflags))
USR_CXXFLAGS += -std=c++11
#CXXFLAGS += -g -ggdb -O0

LIBRARY_IOC += pydev

DBD += pydev.dbd

pydev_SRCS += asyncexec.cpp
pydev_SRCS += epicsdevice.cpp
pydev_SRCS += pywrapper.cpp
pydev_SRCS += util.cpp

pydevice_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES

