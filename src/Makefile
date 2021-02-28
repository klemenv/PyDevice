# Makefile
TOP = ..
include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

USR_LDFLAGS += $(shell $(PYTHON_CONFIG) --ldflags)
USR_CXXFLAGS += $(subst -Wstrict-prototypes,,$(shell $(PYTHON_CONFIG) --cflags))
USR_CXXFLAGS += -std=c++11 -O2
#CXXFLAGS += -g -ggdb -O0

LIBRARY_IOC += pydev

DBD += pydev.dbd

# 3.15 and above support lsi, lso and printf records  
ifdef BASE_3_15
  USR_CXXFLAGS += -DHAVE_LSREC
  DBD += dev315.dbd
endif

pydev_SRCS += asyncexec.cpp
pydev_SRCS += epicsdevice.cpp
pydev_SRCS += pywrapper.cpp
pydev_SRCS += util.cpp

pydevice_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES

