/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution. 
\*************************************************************************/

#include <aiRecord.h>
#include <aoRecord.h>
#include <biRecord.h>
#include <boRecord.h>
#include <dbCommon.h>
#include <longinRecord.h>
#include <longoutRecord.h>
#include <mbbiRecord.h>
#include <mbboRecord.h>
#include <stringinRecord.h>
#include <stringoutRecord.h>
#include <waveformRecord.h>

#include <alarm.h>
#include <callback.h>
#include <cantProceed.h>
#include <dbScan.h>
#include <devSup.h>
#include <epicsExit.h>
#include <epicsExport.h>
#include <iocsh.h>
#include <menuFtype.h>
#include <recGbl.h>

#include <algorithm>
#include <map>
#include <string.h>
#include <vector>

#include "asyncexec.h"
#include "pywrapper.h"

#ifndef LINK_VALUE_NEEDLE
#   define LINK_VALUE_NEEDLE "%value%"
#endif

struct PyDevContext {
    CALLBACK callback;
    IOSCANPVT scan;
};

static std::map<std::string, IOSCANPVT> ioScanPvts;

template <typename T>
static std::string valToStr(const T& val)
{
    return std::to_string(val);
}

template <>
std::string valToStr(const std::string& val)
{
    std::string value;
    // Ensure value string is in quotes
    if (val.size() < 2) {
        value = "'" + val + "'";
    } else {
        bool same = (val.front() == val.back());
        if (same && (val.front() == '\'' || val.front() == '"')) {
            value = val;
        } else {
            value = "'" + val + "'";
        }
    }
    return value;
}

template <typename T>
static std::string valToStr(const std::vector<T>& val)
{
    std::string value = "[";
    for (const auto v: val) {
        value += std::to_string(v) + ",";
    }
    if (value.back() == ',') {
        value.back() = ']';
    } else {
        value += "]";
    }
    return value;
}

template <typename T>
static std::string linkToPyCode(const std::string& input, const T& recordVal)
{
    std::string output = input;
    std::string value = valToStr(recordVal);
    const static std::string needle = LINK_VALUE_NEEDLE;
    size_t pos = output.find(needle);
    while (pos != std::string::npos) {
        output.replace(pos, needle.size(), value);
        pos = output.find(needle);
    }
    return output;
}

template <typename T>
static bool toRecArrayVal(waveformRecord* rec, const std::vector<T>& arr)
{
    if (rec->ftvl == menuFtypeCHAR) {
        auto val = reinterpret_cast<epicsInt8*>(rec->bptr);
        rec->nord = std::min(arr.size(), (size_t)rec->nelm);
        std::copy(arr.begin(), arr.begin()+rec->nord, val);
        return true;
    } else if (rec->ftvl == menuFtypeUCHAR) {
        auto val = reinterpret_cast<epicsUInt8*>(rec->bptr);
        rec->nord = std::min(arr.size(), (size_t)rec->nelm);
        std::copy(arr.begin(), arr.begin()+rec->nord, val);
        return true;
    } else if (rec->ftvl == menuFtypeSHORT) {
        auto val = reinterpret_cast<epicsInt16*>(rec->bptr);
        rec->nord = std::min(arr.size(), (size_t)rec->nelm);
        std::copy(arr.begin(), arr.begin()+rec->nord, val);
        return true;
    } else if (rec->ftvl == menuFtypeUSHORT) {
        auto val = reinterpret_cast<epicsUInt16*>(rec->bptr);
        rec->nord = std::min(arr.size(), (size_t)rec->nelm);
        std::copy(arr.begin(), arr.begin()+rec->nord, val);
        return true;
    } else if (rec->ftvl == menuFtypeLONG) {
        auto val = reinterpret_cast<epicsInt32*>(rec->bptr);
        rec->nord = std::min(arr.size(), (size_t)rec->nelm);
        std::copy(arr.begin(), arr.begin()+rec->nord, val);
        return true;
    } else if (rec->ftvl == menuFtypeULONG) {
        auto val = reinterpret_cast<epicsUInt32*>(rec->bptr);
        rec->nord = std::min(arr.size(), (size_t)rec->nelm);
        std::copy(arr.begin(), arr.begin()+rec->nord, val);
        return true;
    } else if (rec->ftvl == menuFtypeFLOAT) {
        auto val = reinterpret_cast<epicsFloat32*>(rec->bptr);
        rec->nord = std::min(arr.size(), (size_t)rec->nelm);
        std::copy(arr.begin(), arr.begin()+rec->nord, val);
        return true;
    } else if (rec->ftvl == menuFtypeDOUBLE) {
        auto val = reinterpret_cast<epicsFloat64*>(rec->bptr);
        rec->nord = std::min(arr.size(), (size_t)rec->nelm);
        std::copy(arr.begin(), arr.begin()+rec->nord, val);
        return true;
    }
    return false;
}

template <typename T>
static bool fromRecArrayVal(waveformRecord* rec, std::vector<T>& arr)
{
    arr.resize(rec->nelm);
    if (rec->ftvl == menuFtypeCHAR) {
        auto val = reinterpret_cast<epicsInt8*>(rec->bptr);
        arr.assign(val, val+rec->nelm);
        return true;
    } else if (rec->ftvl == menuFtypeUCHAR) {
        auto val = reinterpret_cast<epicsUInt8*>(rec->bptr);
        arr.assign(val, val+rec->nelm);
        return true;
    } else if (rec->ftvl == menuFtypeSHORT) {
        auto val = reinterpret_cast<epicsInt16*>(rec->bptr);
        arr.assign(val, val+rec->nelm);
        return true;
    } else if (rec->ftvl == menuFtypeUSHORT) {
        auto val = reinterpret_cast<epicsUInt16*>(rec->bptr);
        arr.assign(val, val+rec->nelm);
        return true;
    } else if (rec->ftvl == menuFtypeLONG) {
        auto val = reinterpret_cast<epicsInt32*>(rec->bptr);
        arr.assign(val, val+rec->nelm);
        return true;
    } else if (rec->ftvl == menuFtypeULONG) {
        auto val = reinterpret_cast<epicsUInt32*>(rec->bptr);
        arr.assign(val, val+rec->nelm);
        return true;
    } else if (rec->ftvl == menuFtypeFLOAT) {
        auto val = reinterpret_cast<epicsFloat32*>(rec->bptr);
        arr.assign(val, val+rec->nelm);
        return true;
    } else if (rec->ftvl == menuFtypeDOUBLE) {
        auto val = reinterpret_cast<epicsFloat64*>(rec->bptr);
        arr.assign(val, val+rec->nelm);
        return true;
    }
    return false;
}

static void scanCallback(IOSCANPVT scan)
{
#ifdef VERSION_INT
#  if EPICS_VERSION_INT < VERSION_INT(3,16,0,0)
    scanIoRequest(scan);
#  else
    scanIoImmediate(scan, priorityHigh);
    scanIoImmediate(scan, priorityMedium);
    scanIoImmediate(scan, priorityLow);
#  endif
#else
    scanIoRequest(scan);
#endif
}

long initRecord(dbCommon* rec, const std::string& addr)
{
    void *buffer = callocMustSucceed(1, sizeof(PyDevContext), "PyDev::initRecord");
    PyDevContext* ctx = new (buffer) PyDevContext;
    rec->dpvt = ctx;

    // This could be better checked with regex
    if (addr.find("pydev.iointr('") == 0 && addr.substr(addr.size()-2) == "')") {
        std::string param = addr.substr(14, addr.size()-16);
        auto it = ioScanPvts.find(param);
        if (it == ioScanPvts.end()) {
            scanIoInit( &ioScanPvts[param] );
            PyWrapper::Callback cb = std::bind(scanCallback, ioScanPvts[param]);
            PyWrapper::registerIoIntr(param, cb);
            it = ioScanPvts.find(param);
        }
        ctx->scan = it->second;
    } else {
        ctx->scan = nullptr;
    }

    return 0;
}

template <typename T>
static long initInpRecord(T* rec)
{
    return initRecord(reinterpret_cast<dbCommon*>(rec), rec->inp.value.instio.string);
}

template <>
long initInpRecord(waveformRecord* rec)
{
    if (rec->ftvl <= menuFtypeSTRING || rec->ftvl >= menuFtypeENUM) {
        printf("ERROR: Waveform only supports numeric types\n");
        return -1;
    }
    return initRecord(reinterpret_cast<dbCommon*>(rec), rec->inp.value.instio.string);
}

template <typename T>
static long initOutRecord(T* rec)
{
    return initRecord(reinterpret_cast<dbCommon*>(rec), rec->out.value.instio.string);
}

template <typename T>
static long getIointInfo(int dir, T *rec, IOSCANPVT* io)
{
    PyDevContext* ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);
    if (ctx != nullptr && ctx->scan != nullptr) {
        *io = ctx->scan;
    }
    return 0;
}


template <typename T>
static void processInpRecordCb(T* rec)
{
    std::string code = linkToPyCode(rec->inp.value.instio.string, rec->val);
    try {
        if (PyWrapper::exec(code, (rec->tpro == 1), &rec->val) == false) {
            if (rec->tpro == 1) {
                printf("ERROR: Can't convert Python type to record VAL field\n");
            }
            recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
        }
    } catch (...) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
    }
    auto ctx = reinterpret_cast<PyDevContext *>(rec->dpvt);
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}

template <>
void processInpRecordCb(mbbiRecord* rec)
{
    std::string code = linkToPyCode(rec->inp.value.instio.string, rec->rval);
    try {
        if (PyWrapper::exec(code, (rec->tpro == 1), &rec->rval) == false) {
            if (rec->tpro == 1) {
                printf("ERROR: Can't convert Python type to record VAL field\n");
            }
            recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
        }
    } catch (...) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
    }
    auto ctx = reinterpret_cast<PyDevContext *>(rec->dpvt);
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}

template <>
void processInpRecordCb(aiRecord* rec)
{
    std::string code = linkToPyCode(rec->inp.value.instio.string, rec->rval);
    try {
        if (PyWrapper::exec(code, (rec->tpro == 1), &rec->rval) == false) {
            if (rec->tpro == 1) {
                printf("ERROR: Can't convert Python type to record VAL field\n");
            }
            recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
        }
    } catch (...) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
    }
    auto ctx = reinterpret_cast<PyDevContext *>(rec->dpvt);
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}

template <>
void processInpRecordCb(stringinRecord* rec)
{
    std::string value = rec->val;
    std::string code = linkToPyCode(rec->inp.value.instio.string, value);
    try {
        if (PyWrapper::exec(code, (rec->tpro == 1), value) == true) {
            strncpy(rec->val, value.c_str(), sizeof(rec->val));
            rec->val[sizeof(rec->val)-1] = 0;
        } else {
            if (rec->tpro == 1) {
                printf("ERROR: Can't convert Python type to record VAL field\n");
            }
            recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
        }
    } catch (...) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
    }
    auto ctx = reinterpret_cast<PyDevContext *>(rec->dpvt);
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}

template <>
void processInpRecordCb(waveformRecord* rec)
{
    try {
        bool converted = false;
        if (rec->ftvl == menuFtypeFLOAT || rec->ftvl == menuFtypeDOUBLE) {
            std::vector<double> arr;
            if (fromRecArrayVal(rec, arr) == true) {
                std::string code = linkToPyCode(rec->inp.value.instio.string, arr);
                converted = (PyWrapper::exec(code, (rec->tpro == 1), arr) && toRecArrayVal(rec, arr));
            }
        } else {
            std::vector<long> arr;
            if (fromRecArrayVal(rec, arr) == true) {
                std::string code = linkToPyCode(rec->inp.value.instio.string, arr);
                converted = (PyWrapper::exec(code, (rec->tpro == 1), arr) && toRecArrayVal(rec, arr));
            }
        }
        
        if (!converted) {
            if (rec->tpro == 1) {
                printf("ERROR: Can't convert Python type to record VAL field\n");
            }
            recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
        }
    } catch (...) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
    }
    auto ctx = reinterpret_cast<PyDevContext *>(rec->dpvt);
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}

template <typename T>
static long processInpRecord(T* rec)
{
    PyDevContext* ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);
    if (ctx == nullptr) {
        // Keep PACT=1 to prevent further processing
        rec->pact = 1;
        recGblSetSevr(rec, epicsAlarmUDF, epicsSevInvalid);
        return -1;
    }

    if (rec->pact == 1) {
        rec->pact = 0;
        return 0;
    }
    rec->pact = 1;

    AsyncExec::Callback cb = std::bind(processInpRecordCb<T>, rec);
    AsyncExec::schedule(cb);
    return 0;
}

template <typename T>
static void processOutRecordCb(T* rec)
{
    std::string code = linkToPyCode(rec->out.value.instio.string, rec->val);
    try {
        // Ignore the possibility that value hasn't been changed
        (void)PyWrapper::exec(code, (rec->tpro == 1), &rec->val);
    } catch (...) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
    }
    auto ctx = reinterpret_cast<PyDevContext *>(rec->dpvt);
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}

template <>
void processOutRecordCb(mbboRecord* rec)
{
    std::string code = linkToPyCode(rec->out.value.instio.string, rec->rval);
    try {
        // Ignore the possibility that value hasn't been changed
        (void)PyWrapper::exec(code, (rec->tpro == 1), &rec->rval);
    } catch (...) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
    }
    auto ctx = reinterpret_cast<PyDevContext *>(rec->dpvt);
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}

template <>
void processOutRecordCb(aoRecord* rec)
{
    std::string code = linkToPyCode(rec->out.value.instio.string, rec->rval);
    try {
        // Ignore the possibility that value hasn't been changed
        (void)PyWrapper::exec(code, (rec->tpro == 1), &rec->rval);
    } catch (...) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
    }
    auto ctx = reinterpret_cast<PyDevContext *>(rec->dpvt);
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}

template <>
void processOutRecordCb(stringoutRecord* rec)
{
    std::string value = rec->val;
    std::string code = linkToPyCode(rec->out.value.instio.string, value);
    try {
        // Ignore the possibility that value hasn't been changed
        (void)PyWrapper::exec(code, (rec->tpro == 1), value);
        strncpy(rec->val, value.c_str(), sizeof(rec->val));
        rec->val[sizeof(rec->val)-1] = 0;
    } catch (...) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
    }
    auto ctx = reinterpret_cast<PyDevContext *>(rec->dpvt);
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}

template <typename T>
static long processOutRecord(T* rec)
{
    PyDevContext* ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);
    if (ctx == nullptr) {
        // Keep PACT=1 to prevent further processing
        rec->pact = 1;
        recGblSetSevr(rec, epicsAlarmUDF, epicsSevInvalid);
        return -1;
    }

    if (rec->pact == 1) {
        rec->pact = 0;
        return 0;
    }
    rec->pact = 1;

    AsyncExec::Callback cb = std::bind(processOutRecordCb<T>, rec);
    AsyncExec::schedule(cb);
    return 0;
}

extern "C"
{
    /*** INPUT RECORDS ***/
    struct
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initInpRecord<longinRecord>};
        DEVSUPFUN get_ioint_info{(DEVSUPFUN)getIointInfo<longinRecord>};
        DEVSUPFUN process{(DEVSUPFUN)processInpRecord<longinRecord>};
    } devPyDevLongin;
    epicsExportAddress(dset, devPyDevLongin);

    struct
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initInpRecord<mbbiRecord>};
        DEVSUPFUN get_ioint_info{(DEVSUPFUN)getIointInfo<mbbiRecord>};
        DEVSUPFUN write{(DEVSUPFUN)processInpRecord<mbbiRecord>};
    } devPyDevMbbi;
    epicsExportAddress(dset, devPyDevMbbi);

    struct
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initInpRecord<biRecord>};
        DEVSUPFUN get_ioint_info{(DEVSUPFUN)getIointInfo<biRecord>};
        DEVSUPFUN write{(DEVSUPFUN)processInpRecord<biRecord>};
    } devPyDevBi;
    epicsExportAddress(dset, devPyDevBi);

    struct
    {
        long number{6};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initInpRecord<aiRecord>};
        DEVSUPFUN get_ioint_info{(DEVSUPFUN)getIointInfo<aiRecord>};
        DEVSUPFUN write{(DEVSUPFUN)processInpRecord<aiRecord>};
        DEVSUPFUN special_linconv{nullptr};
    } devPyDevAi;
    epicsExportAddress(dset, devPyDevAi);

    struct
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initInpRecord<stringinRecord>};
        DEVSUPFUN get_ioint_info{(DEVSUPFUN)getIointInfo<stringinRecord>};
        DEVSUPFUN write{(DEVSUPFUN)processInpRecord<stringinRecord>};
    } devPyDevStringin;
    epicsExportAddress(dset, devPyDevStringin);

    struct
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initInpRecord<waveformRecord>};
        DEVSUPFUN get_ioint_info{(DEVSUPFUN)getIointInfo<waveformRecord>};
        DEVSUPFUN write{(DEVSUPFUN)processInpRecord<waveformRecord>};
    } devPyDevWaveform;
    epicsExportAddress(dset, devPyDevWaveform);

    /*** OUTPUT RECORDS ***/
    struct
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initOutRecord<longoutRecord>};
        DEVSUPFUN get_ioint_info{(DEVSUPFUN)getIointInfo<longoutRecord>};
        DEVSUPFUN process{(DEVSUPFUN)processOutRecord<longoutRecord>};
    } devPyDevLongout;
    epicsExportAddress(dset, devPyDevLongout);

    struct
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initOutRecord<mbboRecord>};
        DEVSUPFUN get_ioint_info{(DEVSUPFUN)getIointInfo<mbboRecord>};
        DEVSUPFUN write{(DEVSUPFUN)processOutRecord<mbboRecord>};
    } devPyDevMbbo;
    epicsExportAddress(dset, devPyDevMbbo);

    struct
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initOutRecord<boRecord>};
        DEVSUPFUN get_ioint_info{(DEVSUPFUN)getIointInfo<boRecord>};
        DEVSUPFUN write{(DEVSUPFUN)processOutRecord<boRecord>};
    } devPyDevBo;
    epicsExportAddress(dset, devPyDevBo);

    struct
    {
        long number{6};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initOutRecord<aoRecord>};
        DEVSUPFUN get_ioint_info{(DEVSUPFUN)getIointInfo<aoRecord>};
        DEVSUPFUN write{(DEVSUPFUN)processOutRecord<aoRecord>};
        DEVSUPFUN special_linconv{nullptr};
    } devPyDevAo;
    epicsExportAddress(dset, devPyDevAo);

    struct
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initOutRecord<stringoutRecord>};
        DEVSUPFUN get_ioint_info{(DEVSUPFUN)getIointInfo<stringoutRecord>};
        DEVSUPFUN write{(DEVSUPFUN)processOutRecord<stringoutRecord>};
    } devPyDevStringout;
    epicsExportAddress(dset, devPyDevStringout);

epicsShareFunc int pydev(const char *line)
{
    try {
        PyWrapper::exec(line, true);
    } catch (...) {
        // pass
    }
    return 0;
}

static const iocshArg pydevArg0 = { "line", iocshArgString };
static const iocshArg *const pydevArgs[] = { &pydevArg0 };
static const iocshFuncDef pydevDef = { "pydev", 1, pydevArgs };
static void pydevCall(const iocshArgBuf * args)
{
    pydev(args[0].sval);
}

static void pydevUnregister(void*)
{
    AsyncExec::shutdown();
    PyWrapper::shutdown();
}

static void pydevRegister(void)
{
    static bool firstTime = true;
    if (firstTime) {
        firstTime = false;
        PyWrapper::init();
        AsyncExec::init();
        iocshRegister(&pydevDef, pydevCall);
        epicsAtExit(pydevUnregister, 0);
    }
}
epicsExportRegistrar(pydevRegister);

}; // extern "C"
