#!../../bin/linux-x86_64-debug/pydevioc

< envPaths

# PYTHONPATH points to folders where Python modules are.
epicsEnvSet("PYTHONPATH","$(TOP)/python")

cd ${TOP}

## Register all support components
dbLoadDatabase "${TOP}/dbd/pydevioc.dbd"
pydevioc_registerRecordDeviceDriver pdbbase

## Load record instances
dbLoadRecords("${TOP}/db/pydevtest.db")

cd ${TOP}/iocBoot/${IOC}

pydev("import pydevtest")
pydev("google = pydevtest.HttpClient('www.google.com', 80)")
pydev("example = pydevtest.HttpClient('www.example.com', 80)")

iocInit
