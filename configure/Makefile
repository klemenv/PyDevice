TOP=..

include $(TOP)/configure/CONFIG
include $(TOP)/configure/CONFIG_SITE

TARGETS = $(CONFIG_TARGETS) CONFIG.PyDevice
CONFIGS += $(subst ../,,$(wildcard $(CONFIG_INSTALLS)))

include $(TOP)/configure/RULES

# Starting with Python 3.8, python-config needs --embed flag
PYTHON_EMBED := $(shell $(PYTHON_CONFIG) --help | grep embed)
ifeq ($(PYTHON_EMBED),)
	PYDEV_SYS_PROD_LIBS = $(shell $(PYTHON_CONFIG) --ldflags | sed 's/-[^l][^ ]*//g' | sed 's/-l//g')
else
	PYDEV_SYS_PROD_LIBS = $(shell $(PYTHON_CONFIG) --ldflags --embed | sed 's/-[^l][^ ]*//g' | sed 's/-l//g')
endif

CONFIG.PyDevice:
	@echo "SYS_PROD_LIBS += $(PYDEV_SYS_PROD_LIBS)" > $(TOP)/configure/$@
