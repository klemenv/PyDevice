#!../../bin/linux-x86_64-debug/pydevioc

< envPaths

# PYTHONPATH points to folders where Python modules are.
epicsEnvSet("PYTHONPATH","$(TOP)/python")

cd ${TOP}

## Register all support components
dbLoadDatabase "${TOP}/dbd/pydevioc.dbd"
pydevioc_registerRecordDeviceDriver pdbbase

## Load record instances
dbLoadRecords("${TOP}/db/test.db")

cd ${TOP}/iocBoot/${IOC}

pydevExec("import pydevtest")
pydevExec("google = pydevtest.HttpClient('www.google.com', 80)")
pydevExec("example = pydevtest.HttpClient('www.example.com', 80)")

iocInit
