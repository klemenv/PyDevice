#!../../bin/linux-x86_64/pydevioc

< envPaths

cd ${TOP}

## Register all support components
dbLoadDatabase "${TOP}/dbd/pydevioc.dbd"
pydevioc_registerRecordDeviceDriver pdbbase

#ipmiConnect IPMI1 192.168.1.252 "" "" "none" "lan" "operator"

## Load record instances
dbLoadRecords("${TOP}/db/test.db")

cd ${TOP}/iocBoot/${IOC}
iocInit
