

# PyDevice

PyDevice is an EPICS device support for Python interpreter. It allows to connect EPICS database with Python functions from a C-based soft IOC.

The goal of this project is to provide very easy interface for Python developers to integrate Python code into EPICS control system. This is achieved by allowing arbitrary Python code to be called from EPICS records, including but not limited to built-in functions, calculation expressions, custom functions etc. In addition, Python code can be executed from IOC shell which is useful for setting up resources or troubleshooting Python code. Since PyDevice simple provides hooks into any Python code, this allows Python modules to be developed and tested in a standalone non-EPICS environment, while finally connected to EPICS PVs.

## Quick introduction

After selecting *pydev* as device type for a particular record, the INP/OUT fields of the record can specify any valid Python code. Link's string must start with @ character followed by Python expression.

### Hello world example
Any record type can be used for this simple example:
```
record(longout, "PyDev:Test:HelloWorld") {
    field(DTYP, "pydev")
    field(OUT,  "@print('Hello world!')")
}
```
Whenever this record processes, the *Hello world!* text is printed to IOC console.

### Passing values between records and Python code
Record's link can use *%value%* modifier to replace it with current record's value at runtime. 
```
record(longin, "PyDev:Test:HelloWorld") {
    field(DTYP, "pydev")
    field(INP,  "@myvar=%value%")
}
```

Note: *%value% modifier can be changed through the PyDevice Makefile when project is built.*

Next example will demonstrate how to pass value to and from Python code using two records. 
```
record(longout, "PyDev:Test:SP") {
    field(DTYP, "pydev")
    field(OUT,  "@tmpVar=%value%")
    field(FLNK, "PyDev:Test:RB")
}
record(longout, "PyDev:Test:RB") {
    field(DTYP, "pydev")
    field(OUT,  "@tmpVar")
}
```
As an exercise you may want to remove forward link processing to confirm the read back record is reading tmpVar value from Python.

When code specified in the link is a Python statement which returns a value, returned value is assigned to the record automatically. For input records this is required, which means input records can only reference Python variables or call Python functions which return a value. For output records the return value is optional, which allows them to execute arbitrary Python statements and expressions.

### Invoking functions
Any Python function can be invoked from the record.
```
record(longout, "PyDev:Test:AbsVal") {
    field(DTYP, "pydev")
    field(OUT,  "@abs(%value%)")
}
```
Writing a new value to this record will cause record to process, pass new value to Python built-in abs() function and write its return value back to the record. This effectively enforces record to hold only absolute numerical values.

### Importing modules
Python modules can be imported from a record as well.
```
record(longout, "PyDev:Test:ImportMath") {
    field(DTYP, "pydev")
    field(OUT,  "@import math")
    field(PINI, "YES")
}
record(longout, "PyDev:Test:Log2") {
    field(DTYP, "pydev")
    field(OUT,  "@math.log(%value%,2)")
}
```
This approach might be useful to handle each Python module from a individual EPICS database file, which can be selected at boot time.

It is also possible to import Python modules (or call any other Python function for that matter) from the IOC console using **pydevExec()** command.

### Custom modules
Python modules are imported from standard system import locations. Refer to your Python distribution for details. In order to specify particular location for custom files, we need to define PYTHONPATH environment variable. This can easily be done at IOC startup, in this example we include python/ folder from PyDevice distribution folder.

```
epicsEnvSet("PYTHONPATH","$(TOP)/python")
```

### Combining IOC shell with records
The examples provided with PyDevice distribution implement a simple HTTP client class which can connect to any HTTP/1.1 web server.

```
import socket
class HttpClient(object):
    def __init__(self, host, port):
        self._socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._addr = (host, port)

    def get(self, url="/"):
        req = "GET %s HTTP/1.1\r\n\r\n" % url
        self._socket.connect(self._addr)
        self._socket.sendall(req)
        rsp = self._socket.recv(1024)
        self._socket.close()
        print rsp
        return rsp
```

The example IOC startup file then instantiates an object for connecting to Google web servers. Because our example record uses this object, we need to specify the next two lines before calling iocInit():

```
pydevExec("import pydevtest")
pydevExec("google = pydevtest.HttpClient('www.google.com', 80)")
```

Newly created objects live in the global scope of Python interpreter, so they can be referenced from records. The record will trigger simple GET to Google web server request whenever it is processed.

```
record(longout, "PyDev:Http:Google") {
  field(DTYP, "pydev")
  field(OUT,  "@google.get()")
  field(TPRO, "1")
}
```

## Building

In order to build the project, EPICS 3.14+ must be provided and defined in configure/RELEASE file. Also, Python development files must be installed, in particular Python.h must be available. Debian-based Linux distributions this means installing python-dev package.

Assuming all dependencies are satisfied, project should build linkable library and testing IOC binary. Running st.cmd from test iocBoot/iocpydev folder will start the demo IOC. At this point database and Python code can be modified without rebuilding the PyDevice source code.


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
