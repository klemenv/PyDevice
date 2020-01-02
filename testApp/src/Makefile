TOP=../..

include $(TOP)/configure/CONFIG
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE
#=============================

#=============================
# Build the IOC application
#
CXXFLAGS += -std=c++11 -g -ggdb -O0
CXXFLAGS += $(shell python-config --cflags)
LDFLAGS += $(shell python-config --ldflags)

pydevioc_DIR = $(IPMITOOL)/lib/.libs/

PROD_IOC = pydevioc
# pydevioc.dbd will be created and installed
DBD += pydevioc.dbd

# pydev.dbd will be made up from these files:
pydevioc_DBD += base.dbd
pydevioc_DBD += pydev.dbd

# Include dbd files from all support applications:
#pydevioc_DBD += xxx.dbd

# Add all the support libraries needed by this IOC
pydevioc_LIBS += pydev

# pydevioc_registerRecordDeviceDriver.cpp derives from ipmi.dbd
pydevioc_SRCS += pydevioc_registerRecordDeviceDriver.cpp

# Build the main IOC entry point on workstation OSs.
pydevioc_SRCS_DEFAULT += main.cpp
pydevioc_SRCS_vxWorks += -nil-

# Add support from base/src/vxWorks if needed
#pydevioc_OBJS_vxWorks += $(EPICS_BASE_BIN)/vxComLibrary

# Finally link to the EPICS Base libraries
pydevioc_LIBS += $(EPICS_BASE_IOC_LIBS)
SYS_PROD_LIBS += python2.7

#===========================

include $(TOP)/configure/RULES
#----------------------------------------
#  ADD RULES AFTER THIS LINE

