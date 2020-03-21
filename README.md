

# PyDevice

PyDevice is an EPICS device support for Python interpreter. It allows to connect EPICS database with Python functions from a C-based soft IOC.

The goal of this project is to provide very easy interface for Python developers to integrate Python code into EPICS control system. This is achieved by allowing arbitrary Python code to be called from EPICS records, including but not limited to built-in functions, calculation expressions, custom functions etc. In addition, Python code can be executed from IOC shell which is useful for setting up resources or troubleshooting Python code. Since PyDevice simple provides hooks into any Python code, this allows Python modules to be developed and tested in a standalone non-EPICS environment, while finally connected to EPICS PVs.

## Quick introduction

After selecting *pydev* as device type for a particular record, the INP/OUT fields of the record can specify any valid Python code as long as it is prefixed with *@* sign (due to how EPICS parses record links).

### Hello world example
Any record type can be used for this simple example:
```
record(longout, "PyDev:Test:HelloWorld") {
    field(DTYP, "pydev")
    field(OUT,  "@print('Hello world!')")
}
```
Whenever this record processes, the *Hello world!* text is printed to IOC console.

### Passing values from records to Python code
Record's link can use *%value%* modifier to replace it with current record's value at runtime. Use your preferred way to store new value in Python context, usually in variable or passing it to a function.
```
record(longin, "PyDev:Test:HelloWorld") {
    field(DTYP, "pydev")
    field(INP,  "@myvar=%value%")
}
```

Note: *%value% modifier can be changed through the PyDevice Makefile when project is built.*

### Getting values from Python code to records
It is also possible to get value by evaluating arbitrary Python expression.
```
record(longout, "PyDev:Test:RB") {
    field(DTYP, "pydev")
    field(OUT,  "@myvar")
    field(SCAN, "1 second")
}
```
Whenever this record processes, it will read Python variable *tmpVar* and put its value to this record. 

When code specified in the link is a Python expression (any section of the code that evaluates to a value), the returned value is assigned to the record automatically. For input records this is required. For output records the return value is optional, which allows them to execute arbitrary Python expressions or statements (section of code that performs some action).

### Pushing values from Python to record
In the previous example we've seen how record can pull value from Python context. Sometimes we will want the opposite to happen, Python code that will update EPICS record directly. An example would be when message is received asynchronously and decoded value needs to propagate to record. PyDevice supports I/O Intr scanning in all supported records. In such case the record must register a parameter name using the built-in pydev.iointr() function.

```
record(longout, "PyDev:IoIntr") {
  field(DTYP, "pydev")
  field(OUT,  "@pydev.iointr('asyncvar')")
  field(SCAN, "I/O Intr")
}
```

And Python code needs to call pydev.iointr() function passing new value as the second argument. This can be simply exercised from IOC shell by executing following command:

```
pydev("pydev.iointr('asyncvar', 7)")
```

which will immediately push value of 7 to *asyncvar* parameter and in turn process PyDev:IoIntr record.

### Invoking functions
Any Python function can be invoked from the record, including built-in functions as well as functions imported from modules.
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
This approach might be useful to handle each Python module from an individual EPICS database file, which can be selected at boot time.

It is also possible to import Python modules (or call any other Python function for that matter) from the IOC console using **pydev()** command.

### Custom modules
Python modules are imported from standard system import locations. Refer to your Python distribution for details. In order to specify particular location for custom files, we need to define PYTHONPATH environment variable. This can easily be done at IOC startup, in this example we include python/ sub-folder from PyDevice top location.

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
        return rsp
```

The example IOC startup file then instantiates an object for connecting to Google web servers. Because our example record uses this object, we need to specify the next two lines before calling iocInit():

```
pydev("import pydevtest")
pydev("google = pydevtest.HttpClient('www.google.com', 80)")
```

Newly created objects live in the global scope of Python interpreter, so they can be referenced from records. The record will trigger simple GET to Google web server request whenever it is processed.

```
record(stringin, "PyDev:Http:Google") {
  field(DTYP, "pydev")
  field(OUT,  "@google.get()")
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
