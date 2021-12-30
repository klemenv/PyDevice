TOP=..

include $(TOP)/configure/CONFIG
include $(TOP)/configure/CONFIG_SITE

TARGETS = $(CONFIG_TARGETS) CONFIG.PyDevice
CONFIGS += $(subst ../,,$(wildcard $(CONFIG_INSTALLS)))

include $(TOP)/configure/RULES

# Starting with Python 3.8, python-config needs --embed flag
ifeq (,$(findstring embed,$(shell $(PYTHON_CONFIG) --help)))
	PYDEV_SYS_PROD_LIBS = $(patsubst -l%,%,$(filter -l%,$(shell $(PYTHON_CONFIG) --ldflags)))
else
	PYDEV_SYS_PROD_LIBS = $(patsubst -l%,%,$(filter -l%,$(shell $(PYTHON_CONFIG) --ldflags --embed)))
endif
PYDEV_USR_CXXFLAGS = $(filter -I%,$(shell $(PYTHON_CONFIG) --cflags))

CONFIG.PyDevice:
	@echo "SYS_PROD_LIBS += $(PYDEV_SYS_PROD_LIBS)" > $(TOP)/configure/$@
	@echo "USR_CXXFLAGS += $(PYDEV_USR_CXXFLAGS)" >> $(TOP)/configure/$@
