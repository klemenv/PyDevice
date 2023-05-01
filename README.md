
# PyDevice

PyDevice is an EPICS device support for Python interpreter. It allows to connect EPICS database records with Python code.

The goal of this project is to provide very easy interface for Python developers to integrate Python code into EPICS control system. This is achieved by allowing arbitrary Python code to be called from EPICS records, including but not limited to built-in functions, calculation expressions, custom functions etc. In addition, Python code can be executed from IOC shell which is useful for setting up resources or troubleshooting Python code. Since PyDevice simply calls Python code, this allows Python modules to be developed and tested in a standalone non-EPICS environment, and ultimately connected to EPICS PVs.

## Hello World example

```
record(longout, "PyDev:Test:HelloWorld") {
    field(DTYP, "pydev")
    field(OUT,  "@print('Hello world!')")
}
```
Whenever this record processes, the *Hello world!* text is printed to the IOC console.

## Quick Tutorial

This tutorial introduces main features of PyDevice through examples.

### Sample Python code

Let's start with defining a standalone Python class that provides an interface to our device, in a file called mydevice.py:

``` python
import socket

class MyDevice(object):

def __init__(self, ip, port=4011):
    self.ip = ip
    self.port = port
    self.socket = None
    self.sent = 0

def connect(self, timeout=1.0):
    """ Connects to device, throws exception if can't connect """
    if self.socket:
        self.socket.close()
    self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    self.socket.settimeout(timeout)
    self.socket.connect((self.ip, self.port))

def disconnect(self):
      if self.socket:
          self.socket.close()
          self.socket = None

def send(self, msg):
    if not self.socket:
        raise RuntimeError("not connected")

    # TODO: send() doesn't guarantee sending entire message
    sent = self.socket.send(msg)
    if sent == 0:
        self.disconnect()
        raise RuntimeError("socket connection broken")

    self.sent += sent
```

Such class can be easily instantiated and tested in any Python environment:

``` python
from mydevice import MyDevice
if __name__ == "__main__":
    device = MyDevice()
    device.connect("127.0.0.1")
    device.send("test")
```

### Instantiate Python object in the IOC context

Assuming mydevice.py is in Python's search path, we can instantiate a MyDevice object from the IOC st.cmd file. This will create a *device1* variable in the global Python context:

``` shell
pydev("from mydevice import MyDevice")
pydev("device1 = MyDevice('127.0.0.1')")
```

*Hint: `pydev()` function allows to execute arbitrary single-line Python code from IOC shell.*


### Trigger connect() from database

In this simple example of database record, we'll trigger establishing connection with the device whenever record processes:

``` shell
record(longout, "Device1:Connect")
{
    field(DTYP, "pydev")
    field(OUT,  "@device1.connect(timeout=2.0)
}
```

As noted from the Python code above, *connect()* function may throw an exception. In such case, PyDevice will intercept exception and reflect the failure through SEVR and STAT fields.

### Sending a message to device

This example will use `stringout` record to send simple ASCII text to the device whenever the record processes:

``` shell
record(stringout, "Device1:Send")
{
    field(DTYP, "pydev")
    field(OUT,  "@device1.send('VAL')
    field(VAL,  "127.0.0.1")
}
```

PyDevice will recognize common record fields and replace them with actual field value right before the Python code is executed. Field names must be stand-alone upper-case words, and need to be a valid record field. When field is part of the string, it should be enclosed by % sign, ie. RecordNameIs%NAME%.

*Hint: Arbitrary single-line Python code can be executed from any of the supported records.*

### Getting values from Python code to records

It is also possible to get value by evaluating Python expression. In this example we'll read value from a variable by simply referencing the variable name, but it is equally easy to call a function and use its return value:

``` shell
record(longin, "Device1:Sent") {
    field(DTYP, "pydev")
    field(INP,  "@device1.sent")
    field(SCAN, "1 second")
}
```
Whenever this record processes, it will read the value of a member variable *sent* and assign it to this record's VAL field.

*Hint: When code specified in the link is a Python expression (any section of the code that evaluates to a value), the returned value is assigned to the record automatically. For input records this is required. For output records the return value is optional, which allows them to execute arbitrary Python expressions or statements (section of code that performs some action).*

### Pushing values from Python to database

In the previous example we've seen how a record can pull a value from Python context. Sometimes we want Python code to push values directly to record when available and avoid periodic scanning. In such case the record must select *I/O Intr* scanning and register a interrupt name using the built-in *pydev.iointr()* function. Let's add a new record that will process automatically when a value of device1.sent changes:

```
record(longin, "Device1:SentCb") {
  field(DTYP, "pydev")
  field(OUT,  "@pydev.iointr('bytes_sent')")
  field(SCAN, "I/O Intr")
}
```

There's is no magic variable tracking implemented in PyDevice that would allow this record to just work. Instead, Python code needs to explicitly tell PyDevice about a new value. In a way, it needs to send an interrupt to PyDevice. In order to distinguish interrupts from different sources, database and Python code must agree on using the same name. In the above example, we simply used name *bytes_sent* but it could be any other unique string. Let's extend the Python sample and add the following line at the end of *send()* function:

``` python
pydev.iointr('bytes_sent', self.sent)
```

With this change, whenever a message is succesfully sent to device, the *Device1:SentCb* record will process and receive new value.


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

When record processes, PyDevice will search INP or OUT field for field macros and replace them with actual fields' values from record. Field macros are simply field names in all capital letters, for example *VAL*. They need to be standalone words and not surrounded by other alpha-numeric characters, in that case they should be enclosed in %-sign. Only a subset of fields is supported depending on the record. VAL field macro of waveform record is converted into a Python list.

Note: Depeding how string fields are used, you may need to surround them with quotes. Consider these two working examples:

```
record(stringout, "PyDev:Log") {
  field(DTYP, "pydev")
  field(OUT,  "@print('VAL')")
}
record(stringout, "PyDev:Exec") {
  field(DTYP, "pydev")
  field(OUT,  "@VAL")
  # caput PyDev:Exec "print('hello')"
}
```

Supported field macros per record:
* longin & longout
  * VAL, NAME, EGU, HOPR, LOPR, HIGH, HIHI, LOW, LOLO
* bi & bo
  * VAL, NAME, ZNAM, ONAM
* mbbi & mbbo
  * VAL, RVAL, NAME, ZRVL, ONVL, TWVL, THVL, FRVL, FVVL, SXVL, SVVL, EIVL, NIVL, TEVL, ELVL, TVVL, TTVL, FTVL, FFVL, ZRST, ONST, TWST, THST, FRST, FVST, SXST, SVST, EIST, NIST, TEST, ELST, TVST, TTST, FTST, FFST
* ai & ao
  * VAL, RVAL, ORAW, NAME, EGU, HOPR, LOPR, PREC
* stringin & stringout
  * VAL, NAME
* lsi & lso
  * VAL, NAME, SIZV, LEN
* waveform
  * VAL

### Support for concurrent record processing

PyDevice supports processing multiple pydev records at the same time. While
Python protects the interpreted code with GIL, many of the built-in functions
as well as external modules support releasing GIL when doing IO bound or CPU
intensive operations. This works well for parallel processing with Python
threads, and now PyDevice also supports processing multiple `pydev` records in
parallel.

When a record processes, the Python code from the record is pushed to the
queue, record's PACT field is set to 1 and handle is returned to the caller.
This allows requests without put/get callback to return immediately. When a
worker thread becomes available, it processes the oldest request from the queue
and executes the Python code referenced. Once the Python code is complete,
record's value is updated and callback is executed. This allows requests with
completion to properly get notified after Python code is done. By default there
are 3 worker threads, which can be changed with the `PYDEV_NUM_THREADS`
environment variable. 

## Building and adding to IOC

### Dependencies

* EPICS 3.14+ or 7.0+
* Python 2.x or 3.5+ headers and libraries
    * RHEL: yum install python-devel
    * Debian & Ubuntu:
       * For python2: apt install python-dev
       * For python3: apt install python3-dev

### Compiling PyDevice

In order to PyDevice, all its dependencies must be installed. In configure/RELEASE file change EPICS_BASE. In configure/CONFIG_SITE set PYTHON_CONFIG=python3-config if you want to use python3. Set Afterwards issue *make* command in the top level. The compilation will provide dynamic library to be included in the IOC as well as a testing IOC that can be executed from iocBoot/iocpydev folder.

Assuming all dependencies are satisfied, project should build linkable library and testing IOC binary. Running st.cmd from test iocBoot/iocpydev folder will start the demo IOC. At this point database and Python code can be modified without rebuilding the PyDevice source code.

### Adding PyDevice support to IOC

For the existing IOC to receive PyDevice support, a few things need to be added.

* Edit configure/RELEASE and add PYDEVICE variable to point to PyDevice source location
* Edit Makefile in IOC's App/src folder and towards the top add
```shell
include $(PYDEVICE)/configure/CONFIG.PyDevice
```
* In the same Makefile, add the next two lines at applicable position
```shell
<yourioc>_DBD += pydev.dbd
<yourioc>_LIB += pydev
```

After rebuilding, the IOC should have support for pydev.

## Python path and the use of virtual environments

The Python search path can be defined by PYTHONPATH before starting the IOC, or can be defined within the context of the IOC, like:
```
epicsEnvSet("PYTHONPATH","$(MODULE)/python")
epicsEnvSet("PYTHONPATH","$(PYTHONPATH):$(TOP)/python")
```
In the above case, Python source files can be searched for in ```$(MODULE)/python/``` and in a ```python/``` directory in the IOC ```$(TOP)```.


It is also possible to make use of Python virtual environments, which may contain user installed Python packages that are not available via the default system packages. This is simply a case of starting the IOC after activating the environment. For example, to use an environment installed in ```/home/controls/common/python_env/main/env_scan```:
```
source /home/controls/common/python_env/main/env_scan/bin/activate
./st.cmd
```
The packages installed in the virtual environment will then be available to the IOC applcation. 

