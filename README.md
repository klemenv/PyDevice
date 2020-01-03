
# PyDevice

PyDevice is an EPICS device for Python interpreter. It allows to connect EPICS database with Python functions from a C-based soft IOC.

Existing EPICS device extensions serve well for communicating to physical devices. This device support adds the capability to call any Python functions from EPICS records. Python is often easier to prototype with and is especially popular among the scientific community. The goal of this project is to provide easy development of custom functionality in Python, as well as provide EPICS database and IOC infrastructure for seamless integration into existing EPICS environment.
  
  ## Quick course
  
This project is self-contained and provides a working soft IOC example out-of-the box.  After building it and starting it, the IOC serves several PVs that invoke Python functions when processed.

Part of python/demo.py file:
```
def writeAo(a):
    print "writeAo(",a,")"
def readAi():
    return 10.0
```
And corresponding testApp/Db/test.db:
```
record(ao, "Py:Test:Ao") {
  field(DTYP, "pydev")
  field(OUT,  "@pydev demo writeAo")
}
record(ai, "Py:Test:Ai") {
  field(DTYP, "pydev")
  field(INP,  "@pydev demo readAi")
}
```

In order for Python interpreter to find the demo<span>.</span>py module, PYTHONPATH variable is modified in st.cmd file:
```
epicsEnvSet("PYTHONPATH","$(TOP)/python")
```

## Building

In order to build the project, EPICS 3.14+ must be provided and defined in configure/RELEASE file. Also, Python development files must be installed, in particular Python.h must be available. Debian-based Linux distributions this means installing python-dev package.

Assuming all dependencies are satisfied, project should build linkable library and testing IOC binary. Running st.cmd from test iocBoot/iocpydev will start the demo IOC. At this point database and Python code can be modified without rebuilding the PyDevice source code.


## Record support

### Supported records

A subset of EPICS records is supported:
* ao
* longout
* bo
* mbbo
* stringout
* ai
* longin
* bi
* mbbi
* stringin

### Connecting records to Python functions

Record must specify pydev as device type (DTYP). Input or outuput link specification must start with @pydev, followed by Python module (usually part of the file without .py extension) and function name.

Python functions connected to output records should take at least one parameter which is the value from the record. Other function parameters should be made optional with default values. Depending on the record value type, PyDevice will convert record's value to native Python type before invoking Python function. No return code is expected for output records. If write operation fails, function should throw an exception which translates to record severity being raised.

Input records need to read value by invoking Python function without parameters and use return value as new record value. Return value must be of the type that is convertible to record type. If read operation fails, function should throw an exception which translates to record severity being raised.


