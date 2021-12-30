# Makefile
TOP = ..
include $(TOP)/configure/CONFIG
include $(TOP)/configure/CONFIG.PyDevice
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

USR_CXXFLAGS += -std=c++11 -O2 -DUSE_TYPED_RSET
CXXFLAGS += -g -ggdb -O0

LIBRARY_IOC += pydev

DBD += pydev.dbd
DBDINC += pycalcRecord

pydev_DBD += pycalcRecord.dbd

pydev_SRCS += asyncexec.cpp
pydev_SRCS += epicsdevice.cpp
pydev_SRCS += pywrapper.cpp
pydev_SRCS += util.cpp
pydev_SRCS += pydev_ai.cpp
pydev_SRCS += pydev_ao.cpp
pydev_SRCS += pydev_bi.cpp
pydev_SRCS += pydev_bo.cpp
pydev_SRCS += pydev_mbbi.cpp
pydev_SRCS += pydev_mbbo.cpp
pydev_SRCS += pydev_longin.cpp
pydev_SRCS += pydev_longout.cpp
pydev_SRCS += pydev_stringin.cpp
pydev_SRCS += pydev_stringout.cpp
pydev_SRCS += pydev_waveform.cpp
pydev_SRCS += pycalcRecord.cpp

# 3.15 and above support lsi, lso and printf records  
ifdef BASE_3_15
  DBD += pydev315.dbd
  pydev_SRCS += pydev_lsi.cpp
  pydev_SRCS += pydev_lso.cpp
endif

pydevice_LIBS += $(EPICS_BASE_IOC_LIBS)

include $(TOP)/configure/RULES

