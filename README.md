
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

Let's start with defining a standalone Python class that provides an interface to our device, and save it as *python/mydevice.py* in the IOC's folder:

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

    # Send an I/O Intr to any interested record
    pydev.iointr('bytes_sent', self.sent)
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

First we need to tell Python where to import script files from. Add this line close to the top of the IOC startup file, usually *st.cmd*:

``` shell
epicsEnvSet("PYTHONPATH", "$(TOP)/python")
```

We can now instantiate a new MyDevice object from the IOC's st.cmd file using the built-in pydev() function. The following snippet will create a *device1* variable in the global Python context:

``` shell
pydev("from mydevice import MyDevice")
pydev("device1 = MyDevice('127.0.0.1')")
```

*Hint: `pydev()` function allows to execute arbitrary single-line Python code from IOC shell.*


### Connect to device from a record

While setting up Python script, establishing connections, and other initialization tasks can be carried out at IOC startup, the power of PyDevice lies in triggering Python code from EPICS records.

In this simple example of database record, we'll establish a connection with the device whenever record processes:

``` shell
record(longout, "Device1:Connect")
{
    field(DTYP, "pydev")
    field(OUT,  "@device1.connect(timeout=2.0)")
    field(PINI, "YES")
}
```

As noted from the Python code above, *connect()* function may throw an exception. In such case, PyDevice will intercept exception and reflect the failure through record's SEVR and STAT fields.

### Sending a message to device

This example will use `stringout` record to send a simple ASCII text to the device whenever the record processes:

``` shell
record(stringout, "Device1:Send")
{
    field(DTYP, "pydev")
    field(OUT,  "@device1.send(VAL)")
    field(VAL,  "127.0.0.1")
}
```

PyDevice will recognize common record fields and replace them with actual field value right before the Python code is executed. Field names must be stand-alone upper-case words as defined in EPICS record. Field values are converted to native Python types. When field is part of the string, it should be enclosed by % sign, ie. RecordNameIs%NAME%.

*Hint: Arbitrary single-line Python code can be executed from any of the supported records.*

### Getting value from Python context to the record

It is also possible to get a value by evaluating Python expression. In this example we'll create a record that displays a Python variable, but it is equally easy to call a function and use its return value:

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

In the previous example we've seen how a record can pull a value from Python context when the record processes. Sometimes we want Python code to decide when to push values to record and avoid periodic scanning. In such case the record must select *I/O Intr* scanning and register a interrupt name using the built-in *pydev.iointr()* function. Let's add a new record that will process automatically when a value of device1.sent changes:

```
record(longin, "Device1:SentCb") {
  field(DTYP, "pydev")
  field(OUT,  "@pydev.iointr('bytes_sent')")
  field(SCAN, "I/O Intr")
}
```

There's is no automatic variable tracking implemented in PyDevice that would allow this record to just work. Instead, Python code needs to explicitly send a new value to
the record. Look for a line *pydev.iointr()* in the Python script above. In order to support multiple interrupts, database and Python code must agree on using the same name. In the above example, we simply used name *bytes_sent* but it could be any other unique string.


## Record support

Several records from EPICS base are supported by PyDevice: longin, longout, ai, ao, bi, bo, mbbi, mbbo, stringin, stringout and waveform. For the supported record to use PyDevice, record must specify DTYP as *pydev*. Next, record's INP or OUT field must specify Python code to be executed, and prefix it with *@* character. Upon processing, record will execute Python code from the link. If Python code is an expression, returned value is assigned to record - returning a value is required for all input records. Returned value is converted to record's value, in case conversion fails record's SEVR is set to INVALID and STAT is set to CALC alarm. All Python exceptions from the executed code are also printed to the IOC console.

### Record interrupt scanning

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

When record processes, PyDevice will scan INP or OUT field and search for sub-strings that match record fields. Any field found is then replaced them with actual fields' values from the record. Only a subset of fields is supported depending on the record. Another way to view this is to consider record fields as built-in Python variables that can be used in Python code specified in record's INP and OUT fields.

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

When replacing a field with its value, PyDevice tries to convert EPICS types to native Python types. For example, converting a *VAL* field of a *ao* record will generate a Python float number.

Prior to PyDevice 1.3.0, the record fields were replaced with their string representation before Python code was executed. This had a side effect of loosing precision of double/float types, converting types twice, and others. Unfortunately the change in 1.3.0 introduced compatibility issues when using fields in Python strings. Consider the following two examples:

With PyDevice before 1.3.0:
``` 
record(stringout, "PyDev:Log") {
  field(DTYP, "pydev")
  field(OUT,  "@print('VAL')")
}
```
With PyDevice 1.3.0 and later:
``` 
record(stringout, "PyDev:Log") {
  field(DTYP, "pydev")
  field(OUT,  "@print(VAL)")
}
```


### Support for concurrent record processing

PyDevice supports processing multiple pydev records at the same time. WhilePython protects the interpreted code with GIL, many of the built-in functions as well as external modules support releasing GIL when doing IO bound or CPU intensive operations. This works well for parallel processing with Python threads, and now PyDevice also supports processing multiple `pydev` records in
parallel.

When a record processes, the Python code from the record is pushed to the queue, record's PACT field is set to 1 and handle is returned to the caller. This allows requests without put/get callback to return immediately. When a worker thread becomes available, it processes the oldest request from the queue and executes the Python code referenced. Once the Python code is complete, record's value is updated and callback is executed. This allows requests with completion to properly get notified after Python code is done. By default there are 3 worker threads, which can be changed with the `PYDEV_NUM_THREADS` environment variable. 

## Building and adding to IOC

### Dependencies

* EPICS 3.14+ or 7.0+
* Python 2.x or 3.5+ headers and libraries
    * RHEL: yum install python-devel
    * Debian & Ubuntu:
       * For python2: apt install python-dev
       * For python3: apt install python3-dev

### Compiling PyDevice

PyDevice depends on EPICS base and Python. `EPICS_BASE` needs to be specified in either *configure/RELEASE* or *configure/RELEASE*.local files. Python libraries and header files must be installed, usually as system packages. PyDevice uses python-config tool to determine how to include Python libraries, on some systems this tool may be called python3-config depending on installed Python versions. In such case, `PYTHON_CONFIG` parameter can be overriden in *configure/CONFIG_SITE* or *configure/CONFIG_SITE.local* with desired python-config name or path.

After making those 2 changes in *configure/* folder, run the `make` command in the top folder of PyDevice. The compilation will provide dynamic library to be included in the IOC as well as a testing IOC that can be executed from iocBoot/iocpydev folder.

Assuming all dependencies are satisfied, project should build linkable library and testing IOC binary. Running st.cmd from test iocBoot/iocpydev folder will start the demo IOC. At this point database and Python code can be modified without rebuilding the PyDevice source code.

### Adding PyDevice support to the IOC

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

The Python search path can be defined by environment variable PYTHONPATH before starting the IOC, or can be defined in the IOC shell, ie:
```
epicsEnvSet("PYTHONPATH","$(MODULE)/python")
epicsEnvSet("PYTHONPATH","$(PYTHONPATH):$(TOP)/python")
```
In the above case, Python source files can be searched for in ```$(MODULE)/python/``` and in a ```python/``` directory of the IOC.


It is also possible to make use of Python virtual environments, which may contain user installed Python packages that are not available via the default system packages. This is simply a case of starting the IOC after activating the environment. For example, to use an environment installed in ```/home/controls/common/python_env/main/env_scan```:
```
source /home/controls/common/python_env/main/env_scan/bin/activate
./st.cmd
```
The packages installed in the virtual environment will then be available to the IOC applcation. 

