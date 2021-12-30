# Makefile
TOP = ../..
include $(TOP)/configure/CONFIG
include $(TOP)/configure/CONFIG.PyDevice
#----------------------------------------
#  ADD MACRO DEFINITIONS AFTER THIS LINE

USR_CXXFLAGS += -I$(TOP)/src
USR_CXXFLAGS += -std=c++11 -g -ggdb -O0
SRC_DIRS += ../..

PROD_LIBS = Com

TESTPROD_HOST += testutil
testutil_SRCS += test_util.cpp
testutil_SRCS += util.cpp
TESTS += testutil

TESTPROD_HOST += testpywrapper
testpywrapper_SRCS += test_pywrapper.cpp
testpywrapper_SRCS += pywrapper.cpp
TESTS += testpywrapper

TESTSCRIPTS_HOST += $(TESTS:%=%.t)

include $(TOP)/configure/RULES

