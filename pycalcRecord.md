# pycalcRecord

pycalcRecord is a custom record that allows passing multiple parameters to
Python code and returning result of executed code as record value.
Most common use case is to invoke Python function and using it's return
value. But any Python expression can be evaluated the same way, including
numerical expressions. While PyDevice itself provides similar functionality
through EPICS built-in records, the EPICS built-in records only allow to
pass a single parameter to Python code at a time.

## Parameter fields

### Processing
pycalcRecord has the standard fields for specifying under what 
curcuimstances the record will process. Refer to EPICS record commond 
[Scan Fields](https://wiki-ext.aps.anl.gov/epics/index.php?title=RRM_3-14_dbCommon#Scan_Fields) 
documentation for their description, ie. SCAN, PINI and other 
processing fields. When INPx field contains a link and uses CP or CPP 
subscription, the pycalcRecord will process when link's monitor is posted 
according to [Channel Access Links](https://wiki-ext.aps.anl.gov/epics/index.php?title=RRM_3-14_Concepts#Channel_Access_Links) specification.

### Input fields
INPx fields are used to pass parameters to Python code to be executed.
The fields can be database links, channel access links or constants.
If they're links, they must specify another record's field. If they're
constants, they will be initialized with the value they are configured
with and can be changed vid dbPuts.

Each INPx field has a corresponding FTx field that specifies the type
of the field. Only scalar type are supported as of now: CHAR, UCHAR,
SHORT, USHOR, LONG, ULONG, INT64, UINT64, FLOAT, DOUBLE, STRING.
Default value is DOUBLE.

### Output fields
The result of executed Python code is pushed to VAL field every time
record processes unless Python code could not be executed. The result
value is converted to the type specified in FTVL which can be one of the
following: LONG, DOUBLE, STRING. 

If an output link is specified in OUT field, the same value from VAL is
sent to the output link.

### Python expression
The purpose of this record is to be able to execute arbitrary Python code
by passing multiple parameters from EPICS PVs. The actual code to be executed
is specified in CALC field. The field is evaluated every time the record
processes, and so it can be changed at runtime. 

When record processes, it first obtains latest values for the the INPx fields.
Input parameters can be specified as %A%, %B%, etc. Input paramaters values
are then replaced in the CALC string and the string is then executed through
Python interpreter. The result (expression, function return value, etc.) is
transfered to VAL field and the monitor is posted. In case Python code can
not be executed or throws an exception, the record alarm is set to
epicsAlarmCalc and severity is epicsSevInvalid.

### Including pycalcRecord support in the IOC

In addition to linking against pydev library as explained in 
[README.md](README.md#adding-pydevice-support-to-ioc), one must also include
pycalcRecord.dbd in the IOC as

```
<yourioc>_DBD += pycalcRecord.dbd
```

## Examples

### Simple expression

```
record(py, "PyCalcTest:MathExpr") {
    field(INPA, "17")
    field(INPB, "3")
    field(CALC, "A*B")
}
```

Processing this record will result in its value being 51.0. This may come as
a bit of surprise since both parameters are integers. The constants needs the
FTx field populated which defaults to DOUBLE. When the above record executes,
the 2 integer values are converted to double values before passing to Python
code. The result of the expression is DOUBLE value which happens to be default
value for FTVL.

### Adaptive parameter and return value types

```
record(py, "PyCalcTest:AdaptiveTypes") {
    field(INPA, "PyCalcTest:Input1 CP")
    field(INPB, "PyCalcTest:Input2 CP")
    field(FTA,  "LONG")
    field(FTB,  "DOUBLE")
    field(CALC, "pow(max([A, B]), 2)")
    field(FTVL, "LONG")
}
```

The FTx fields define the parameter type regarless of what the link's type
might be. When types don't match, values are converted and precision may be
lost. In the example above, the PyCalcTest:Input1 PV is ai but the pycalc
definition forces conversion to LONG. Similarly, the result of Python code
is a double value, but we're only intersted in integer precision.

### Trigger record alarm

```
record(py, "PyCalcTest:InvalidAlarm") {
    field(CALC, "unknown_function()")
}
```

When Python code fails to execute, the records' alarm is set to CALC and the
severity is set to INVALID. Python code can fail due to a syntax error,
a module or a function not defined, or a Python exception is raised.
Set TPRO field to 1 to print error details to the IOC console.
