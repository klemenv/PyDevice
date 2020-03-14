/* epicsdevice.cpp
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Oct 2018
 */

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

#include <alarm.h>
#include <callback.h>
#include <cantProceed.h>
#include <devSup.h>
#include <epicsExit.h>
#include <epicsExport.h>
#include <iocsh.h>
#include <recGbl.h>

#include "asyncexec.h"
#include "pyworker.h"

#ifndef LINK_VALUE_NEEDLE
#   define LINK_VALUE_NEEDLE "%value%"
#endif

struct PyDevContext {
    CALLBACK callback;
};

std::string linkToPyCode(const std::string& input, const std::string& recordVal)
{
    std::string output = input;
    const static std::string needle = LINK_VALUE_NEEDLE;
    size_t pos = output.find(needle);
    while (pos != std::string::npos) {
        output.replace(pos, needle.size(), recordVal);
        pos = output.find(needle);
    }
    return output;
}

long initRecord(dbCommon* rec, const char* addr)
{
    rec->dpvt = nullptr;

    std::string module;
    std::string code;

    void *buffer = callocMustSucceed(1, sizeof(PyDevContext), "PyDev::initOutRecord");
    rec->dpvt = new (buffer) PyDevContext;
    return 0;
}

template <typename T>
long initInpRecord(T *rec)
{
    return initRecord(reinterpret_cast<dbCommon*>(rec), rec->inp.value.instio.string);
}

template <typename T>
long initOutRecord(T *rec)
{
    return initRecord(reinterpret_cast<dbCommon*>(rec), rec->out.value.instio.string);
}


template <typename T>
static void processInpRecordCb(T* rec)
{
    std::string value = std::to_string(rec->val);
    std::string code = linkToPyCode(rec->inp.value.instio.string, value);
    try {
        if (PyWorker::exec(code, (rec->tpro == 1), &rec->val) == false) {
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

/*
template <>
void processInpRecordCb(stringinRecord* rec)
{
    std::string value = rec->val;
    std::string code = linkToPyCode(rec->inp.value.instio.string, value);
    try {
        PyWorker::eval(rec->val, code, (rec->tpro == 1));
    } catch (PyWorker::Exception& e) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
    }
    auto ctx = reinterpret_cast<PyDevContext *>(rec->dpvt);
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}
*/

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
    std::string value = std::to_string(rec->val);
    std::string code = linkToPyCode(rec->out.value.instio.string, value);
    try {
        // Ignore the possibility that value hasn't been changed
        (void)PyWorker::exec(code, (rec->tpro == 1), &rec->val);
    } catch (...) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
    }
    auto ctx = reinterpret_cast<PyDevContext *>(rec->dpvt);
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}

/*
template <>
void processOutRecordCb(longoutRecord* rec)
{
    std::string value = rec->val;
    std::string code = linkToPyCode(rec->out.value.instio.string, value);
    try {
        // Ignore the possibility that value hasn't been changed
        (void)PyWorker::exec(code, (rec->tpro == 1), rec->val);
    } catch (...) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
    }
    auto ctx = reinterpret_cast<PyDevContext *>(rec->dpvt);
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}
*/

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
        DEVSUPFUN get_ioint_info{nullptr};
        DEVSUPFUN process{(DEVSUPFUN)processInpRecord<longinRecord>};
    } devPyDevLongin;
    epicsExportAddress(dset, devPyDevLongin);
/*
    struct
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initInpRecord<mbbiRecord>};
        DEVSUPFUN get_ioint_info{nullptr};
        DEVSUPFUN write{(DEVSUPFUN)processInpRecord<mbbiRecord>};
    } devPyDevMbbi;
    epicsExportAddress(dset, devPyDevMbbi);

    struct
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initInpRecord<biRecord>};
        DEVSUPFUN get_ioint_info{nullptr};
        DEVSUPFUN write{(DEVSUPFUN)processInpRecord<biRecord>};
    } devPyDevBi;
    epicsExportAddress(dset, devPyDevBi);

    struct
    {
        long number{6};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initInpRecord<aiRecord>};
        DEVSUPFUN get_ioint_info{nullptr};
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
        DEVSUPFUN get_ioint_info{nullptr};
        DEVSUPFUN write{(DEVSUPFUN)processInpRecord<stringinRecord>};
    } devPyDevStringin;
    epicsExportAddress(dset, devPyDevStringin);
*/
    /*** OUTPUT RECORDS ***/
    struct
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initOutRecord<longoutRecord>};
        DEVSUPFUN get_ioint_info{nullptr};
        DEVSUPFUN process{(DEVSUPFUN)processOutRecord<longoutRecord>};
    } devPyDevLongout;
    epicsExportAddress(dset, devPyDevLongout);

/*
    struct
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initOutRecord<mbboRecord>};
        DEVSUPFUN get_ioint_info{nullptr};
        DEVSUPFUN write{(DEVSUPFUN)processOutRecord<mbboRecord>};
    } devPyDevMbbo;
    epicsExportAddress(dset, devPyDevMbbo);

    struct
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initOutRecord<boRecord>};
        DEVSUPFUN get_ioint_info{nullptr};
        DEVSUPFUN write{(DEVSUPFUN)processOutRecord<boRecord>};
    } devPyDevBo;
    epicsExportAddress(dset, devPyDevBo);

    struct
    {
        long number{6};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initOutRecord<aoRecord>};
        DEVSUPFUN get_ioint_info{nullptr};
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
        DEVSUPFUN get_ioint_info{nullptr};
        DEVSUPFUN write{(DEVSUPFUN)processOutRecord<stringoutRecord>};
    } devPyDevStringout;
    epicsExportAddress(dset, devPyDevStringout);
*/

epicsShareFunc int pydevExec(const char *line)
{
    try {
        PyWorker::exec(line, true);
    } catch (...) {
        // pass
    }
    return 0;
}

static const iocshArg pydevExecArg0 = { "line", iocshArgString };
static const iocshArg *const pydevExecArgs[] = { &pydevExecArg0 };
static const iocshFuncDef pydevExecDef = { "pydevExec", 1, pydevExecArgs };
static void pydevExecCall(const iocshArgBuf * args)
{
    pydevExec(args[0].sval);
}

static void pydevUnregister(void)
{
    AsyncExec::shutdown();
    PyWorker::shutdown();
}

static void pydevRegister(void)
{
    static bool firstTime = true;
    if (firstTime) {
        firstTime = false;
        PyWorker::init();
        AsyncExec::init();
        iocshRegister(&pydevExecDef, pydevExecCall);
        epicsAtExit(pydevUnregister, 0);
    }
}
epicsExportRegistrar(pydevRegister);

}; // extern "C"
