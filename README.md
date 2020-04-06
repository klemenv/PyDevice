# PyDevice

PyDevice is an EPICS device support for Python interpreter. It allows to connect EPICS database records with Python code.

The goal of this project is to provide very easy interface for Python developers to integrate Python code into EPICS control system. This is achieved by allowing arbitrary Python code to be called from EPICS records, including but not limited to built-in functions, calculation expressions, custom functions etc. In addition, Python code can be executed from IOC shell which is useful for setting up resources or troubleshooting Python code. Since PyDevice simply calls Python code, this allows Python modules to be developed and tested in a standalone non-EPICS environment, and ultimately connected to EPICS PVs.

## Quick introduction

After selecting *pydev* as device type for a particular record, the INP/OUT fields of the record can specify any valid Python code as long as it is prefixed with *@* sign (this limitation comes from EPICS base).

### Hello world example
Any supported record type can be used for this simple example:
```
record(longout, "PyDev:Test:HelloWorld") {
    field(DTYP, "pydev")
    field(OUT,  "@print('Hello world!')")
}
```
Whenever this record processes, the *Hello world!* text is printed to the IOC console.

### Passing values from records to Python code
Record's link can use field modifiers that are replaced with actual values when record processes. Not all record fields can be used this way. This example simply assigns record's value to the Python variable in global dictionary:
```
record(longout, "PyDev:Test:Assign") {
    field(DTYP, "pydev")
    field(OUT,  "@myvar=%VAL%")
}
```

Note: Check <TODO> for valid field modifiers for supported records.

### Getting values from Python code to records
It is also possible to get value by evaluating Python expression.
```
record(longin, "PyDev:Test:Read") {
    field(DTYP, "pydev")
    field(INP,  "@myvar")
    field(SCAN, "1 second")
}
```
Whenever this record processes, it will read Python variable *myvar* and assign it to this record's VAL field.

When code specified in the link is a Python expression (any section of the code that evaluates to a value), the returned value is assigned to the record automatically. For input records this is required. For output records the return value is optional, which allows them to execute arbitrary Python expressions or statements (section of code that performs some action).

### Pushing values from Python to record
In the previous example we've seen how record can pull value from Python context. Sometimes we want Python code to push values directly to record when available. An example would be when a Python thread has new value and needs to update the record. PyDevice supports I/O Intr scanning in all supported records. In such case the record must register a parameter name using the built-in pydev.iointr() function.

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
    field(OUT,  "@abs(%VAL%)")
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
record(ai, "PyDev:Test:Log2") {
    field(DTYP, "pydev")
    field(INP,  "@math.log(%VAL%,2)")
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

In this example we'll instantiate a new object when IOC boots (and before iocInit()) so that any records can use it right away:

```
pydev("import pydevtest")
pydev("google = pydevtest.HttpClient('www.google.com', 80)")
```

Newly created objects live in the global dictionary of Python interpreter, so they can be referenced from records:

```
record(stringin, "PyDev:Google:Refresh") {
  field(DTYP, "pydev")
  field(INP,  "@google.get()")
}
```

## Record support

Several records from EPICS base are supported by PyDevice: longin, longout, ai, ao, bi, bo, mbbi, mbbo, stringin, stringout and waveform. For the supported record to use PyDevice, record must specify DTYP as *pydev*. Next, record's INP or OUT field must specify Python code to be executed, prefixed with *@* character. Upon processing, record will execute Python code from the link. If Python code is an expression, returned value is assigned to record - this is required for all input records. Returned value is casted to record's value, in case conversion fails record's SEVR is set to INVALID. All Python exceptions from the executed code are intercepted and printed to the IOC console. In addition, record STAT field becomes INVALID.

### Pushing value from Python to record (aka Interrupt scanning)

In order to support *I/O Intr* scaning, PyDevice provides built-in function called *pydev.iointr(param, value=None)*. Record's INP or OUT specification should call *pydev.iointr(param)* function with only parameter name specified and record's SCAN field should be specified as *I/O Intr*. When Python code wants to push new value to record(s), it must invoke same function with parameter name AND value. Doing so will immediately trigger processing of all records that use the same parameter name. Parameter name can be any valid string and is not coupled with any record name or Python variable.

Note: SCAN with *I/O Intr* can only be selected when IOC loads the database, not at runtime.

PyDevice defines *pydev.iointr()* function internally and registers it as built-in function. No module needs to be imported from Python code. However, when testing custom Python code outside PyDevice environment, *pydev.iointr()* will not be available. This can be easily fixed by defining dummy *pydev* class, for example as part of script startup mechanism which allows same script to be executed as a standalone script or as part of PyDevice:

```
if __name__ == "__main__":
  class pydev(object):
    @staticmethod
    def iointr(param, value=None):
      pass
  # rest of your code
```

### Field macros

When record processes, PyDevice will search INP or OUT field for field macros and replace them with actual fields' values from record. Field macros are simply field names surrounded with *%* character, for example *%VAL%*. Only a subset of fields is supported. Field macro try to preserve value types. %VAL% field macro of waveform record is converted into a Python list.

Note: Depeding how string fields are used, you may need to surround them with quotes. Consider these two working examples:

```
record(stringout, "PyDev:Log") {
  field(DTYP, "pydev")
  field(OUT,  "@print('%VAL%')")
}
record(stringout, "PyDev:Exec") {
  field(DTYP, "pydev")
  field(OUT,  "@%VAL%")
  # caput PyDev:Exec "print('hello')"
}
```

Supported field macros per record:
* longin & longout
  * %VAL%
  * %NAME%
  * %EGU%
  * %HOPR%
  * %LOPR%
  * %HIGH%
  * %HIHI%
  * %LOW%
  * %LOLO%
* bi & bo
  * %VAL%
  * %NAME%
  * %ZNAM%
  * %ONAM%
* mbbi & mbbo
  * %VAL%
  * %RVAL%
  * %NAME%
  * %ZRVL%
  * %ONVL%
  * %TWVL%
  * %THVL%
  * %FRVL%
  * %FVVL%
  * %SXVL%
  * %SVVL%
  * %EIVL%
  * %NIVL%
  * %TEVL%
  * %ELVL%
  * %TVVL%
  * %TTVL%
  * %FTVL%
  * %FFVL%
  * %ZRST%
  * %ONST%
  * %TWST%
  * %THST%
  * %FRST%
  * %FVST%
  * %SXST%
  * %SVST%
  * %EIST%
  * %NIST%
  * %TEST%
  * %ELST%
  * %TVST%
  * %TTST%
  * %FTST%
  * %FFST%
* ai & ao
  * %VAL%
  * %RVAL%
  * %ORAW%
  * %NAME%
  * %EGU%
  * %HOPR%
  * %LOPR%
  * %PREC%
* stringin & stringout
  * %VAL%
  * %NAME%
* waveform
  * %VAL%

### Serialized record processing

PyDevice uses a single thread to execute any Python code from records. It is assumed that Python's Global Interpreter Lock (GIL) would serialize requests anyway, although support for multiple processing threads might be added in the future. When record processes, the requested Python code to be executed is put into FIFO queue, record's PACT field is set to 1 and handle is returned to callee. This allows requests without callback to return immediately. When its turn, the request is dequeued and Python code is executed. Once the Python code is complete, record's value is updated and callback is executed. This allows requests with completion to properly get notified after Python code is done.

Single thread policy doesn't apply to any threads spawned from Python code.

## Building and adding to IOC

### Dependencies

* EPICS 3.14+ or 7.0+
* Python 2.x or 3.5+ headers and libraries
    * RHEL: yum install python-devel
    * Debian & Ubuntu: apt install python-dev
    
### Compiling PyDevice

In order to PyDevice, all its dependencies must be installed. In configure/RELEASE file change EPICS_BASE. Afterwards issue *make* command in the top level. The compilation will provide dynamic library to be included in the IOC as well as a testing IOC that can be executed from iocBoot/iocpydev folder.

Assuming all dependencies are satisfied, project should build linkable library and testing IOC binary. Running st.cmd from test iocBoot/iocpydev folder will start the demo IOC. At this point database and Python code can be modified without rebuilding the PyDevice source code.

### Adding PyDevice support to IOC

For the existing IOC to receive PyDevice support, a few things need to be added. 

* Edit configure/RELEASE and add PYDEVICE variable to point to PyDevice source location
* Edit Makefile in IOC's App/src folder and add
```
SYS_PROD_LIBS += $(shell python-config --ldflags | sed 's/-[^l][^ ]*//g' | sed 's/-l//g')
<yourioc>_DBD += pydev.dbd
<yourioc>_LIB += pydev
```

Check example IOC provided with PyDevice source. After rebuilding, the IOC should have support for pydev.
