# PyRecord

PyRecord is a custom record that allows passing multiple parameters to
Python code and returning result of executed code as record value.
Most common use case is to invoke Python function and using it's return
value. But any Python expression can be evaluated the same way, including
numerical expressions. While PyDevice itself provides similar functionality
through EPICS built-in records, the EPICS built-in records only allow to
pass a single parameter to Python code at a time.

## Parameter fields

### Processing
PyRecord has the standard fields for specifying under what 
curcuimstances the record will process. Refer to EPICS record commond 
[Scan Fields](https://wiki-ext.aps.anl.gov/epics/index.php?title=RRM_3-14_dbCommon#Scan_Fields) 
documentation for their description, ie. SCAN, PINI and other 
processing fields. When INPx field contains a link and uses CP or CPP 
subscription, the PyRecord will process when link's monitor is posted 
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
Setting a corresponding FTx field is required for constant
values, but optional for links as they automatically determine the
scalar type from the connected link.

### Output fields
The result of executed Python code is pushed to VAL field every time
record processes. Record should specify the returned value type in
FTVL field and should match the return type from the executed Python
code. For example, result of `17+3` is a long integer and the FTVL
should specify LONG. Supported types include: CHAR, UCHAR, SHORT,
USHORT, LONG, ULONG, INT64, UINT64, FLOAT, DOUBLE, STRING.

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

## Examples

### Simple expression

```
record(py, "PyRecTest:MathExp") {
    field(INPA, "17")
    field(INPB, "3")
    field(CALC, "%A%*%B%")
}
```

Processing this record will result in its value being 20. This is by no means
a surprise. This example can be as simple because the default parameter type
is LONG, including the returned value of VAL field.

### Floating point expression

```
record(py, "PyRecTest:FloatExp") {
    field(INPA, "Test:Input1 CP")
    field(INPB, "Test:Input2 CP")
    field(FTA,  "FLOAT")
    field(FTB,  "FLOAT")
    field(CALC, "pow(max([%A%, %B%]), 2)")
    field(FTVL, "LONG")
}
```

Here we're passing 2 parameters to Python code and using Python's built-in
functions pow() and max() to calculate square of the largest of the 2
parameters. Both parameters are floating point numbers to support any
numerical value from input PVs, but we only need integer precision for the
return value.

### Mixing parameter types and automatic casting

```
record(py, "PyRecTest:CastToStr") {
    field(INPA, "Test:Input1")
    field(FTA,  "FLOAT")
    field(CALC, "'invalid value' if %A% < 0 else %A%")
    field(FTVL, "STRING")
}
```

This example is interesting because it shows the use of Python conditional
statement that returns 2 different types. When parameter A is negative, the
record value is a string 'invalid value'. But when parameter A is positive,
the parameter is converted to string because of FTVL field.

### Trigger record alarm

```
record(py, "PyRecTest:InvalidAlarm") {
    field(CALC, "unknown_function()")
    field(FTVL, "STRING")
}
```

When Python code fails to execute, the records' alarm is set to CALC. Python
code can fail due to a syntax error, a module or a function not defined,
or a Python exception is raised. Set TPRO field to 1 to print error details
to the IOC console.