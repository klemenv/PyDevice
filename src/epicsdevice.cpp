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
#include <epicsExport.h>
#include <epicsGuard.h>
#include <epicsMutex.h>
#include <iocsh.h>
#include <recGbl.h>

#include <dirent.h>
#include <fstream>
#include <map>
#include <sys/types.h>
#include <vector>

#include "pymodule.h"

typedef epicsGuard<epicsMutex> ScopedLock;

struct PyDevContext {
    CALLBACK callback;
};

static PyModule& getPyModule(const std::string& file, bool create=false)
{
    static std::map<std::string, PyModule> modules;
    static epicsMutex mutex;

    ScopedLock lock(mutex);
    auto it = modules.find(file);
    if (it == modules.end())
    {
        if (!create)
        {
            throw std::range_error("device doesn't exist");
        }
        bool inserted;
        std::tie(it, inserted) = modules.emplace(file, file);
    }

    return std::reference_wrapper<PyModule>(it->second);
}

static std::vector<std::string> split(const std::string &text, char delimiter=' ', unsigned maxSplits=0)
{
    std::vector<std::string> tokens;

    if (maxSplits == 0)
        maxSplits = -1;
    size_t start = 0;
    while (maxSplits-- > 0 && start != std::string::npos)
    {
        size_t end = text.find(delimiter, start);
        if (end == std::string::npos)
            break;
        tokens.push_back(std::move(text.substr(start, end - start)));
        start = text.find_first_not_of(delimiter, end);
    }
    if (start != std::string::npos)
        tokens.push_back(std::move(text.substr(start)));
    return tokens;
}

static std::pair<std::string, std::string> parseRecordAddress(const std::string &addr)
{
    // Device address is of the form @pydev MODULE FUNC
    auto tokens = split(addr, ' ', 2);
    if (tokens.size() < 3 || tokens[0] != "pydev")
    {
        throw std::invalid_argument("invalid device address");
    }

    return std::make_pair(tokens[1], tokens[2]);
}

long initRecord(dbCommon* rec, const char* addr)
{
    rec->dpvt = nullptr;

    std::string module;
    std::string func;
    try {
        std::tie(module, func) = parseRecordAddress(addr);
    }
    catch (...)
    {
        if (rec->tpro == 1)
        {
            printf("ERROR: invalid record link '%s'\n", addr);
        }
        return -1;
    }

    try
    {
        (void)getPyModule(module, true);
    }
    catch (...)
    {
        if (rec->tpro == 1)
        {
            printf("ERROR: Failed to initialize Python module!\n");
        }
        return -1;
    }

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
static void processInpRecordCb(T *rec, PyModule &module, const std::string &func)
{
    try
    {
        rec->val = module.exec<decltype(rec->val)>(func);
    }
    catch (std::exception &error)
    {
        recGblSetSevr(rec, epicsAlarmWrite, epicsSevInvalid);
        if (rec->tpro == 1)
        {
            printf("%s\n", error.what());
        }
    }

    auto ctx = reinterpret_cast<PyDevContext *>(rec->dpvt);
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}

template <>
void processInpRecordCb(stringinRecord *rec, PyModule &module, const std::string &func)
{
    try
    {
        std::string val = module.exec<char*>(func);
        strncpy(rec->val, val.c_str(), sizeof(rec->val) - 1);
        rec->val[sizeof(rec->val)-1] = '\0';
    }
    catch (std::exception &error)
    {
        recGblSetSevr(rec, epicsAlarmWrite, epicsSevInvalid);
        if (rec->tpro == 1)
        {
            printf("%s\n", error.what());
        }
    }

    auto ctx = reinterpret_cast<PyDevContext *>(rec->dpvt);
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}

template <typename T>
static long processInpRecord(T *rec)
{
    PyDevContext *ctx = reinterpret_cast<PyDevContext *>(rec->dpvt);
    if (ctx == nullptr)
    {
        // Keep PACT=1 to prevent further processing
        rec->pact = 1;
        recGblSetSevr(rec, epicsAlarmUDF, epicsSevInvalid);
        return -1;
    }

    if (rec->pact == 1)
    {
        rec->pact = 0;
        return 0;
    }

    try
    {
        std::string file;
        std::string func;
        std::tie(file, func) = parseRecordAddress(rec->inp.value.instio.string);
        PyModule::Task cb = std::bind(processInpRecordCb<T>, rec, std::placeholders::_1, func);

        PyModule &module = getPyModule(file);
        module.schedule(cb);

        rec->pact = 1;
        return 0;
    }
    catch (...)
    {
        recGblSetSevr(rec, epicsAlarmWrite, epicsSevInvalid);
        // TODO: if (rec->tpro == 1) { log error }
        return -1;
    }
}

template <typename T>
static void processOutRecordCb(T* rec, PyModule& module, const std::string& func)
{
    try
    {
        module.exec(func, rec->val);
    }
    catch (std::exception& error)
    {
        recGblSetSevr(rec, epicsAlarmWrite, epicsSevInvalid);
        if (rec->tpro == 1)
        {
            printf("%s\n", error.what());
        }
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

    try{
        std::string file;
        std::string func;
        std::tie(file, func) = parseRecordAddress(rec->out.value.instio.string);
        PyModule::Task cb = std::bind(processOutRecordCb<T>, rec, std::placeholders::_1, func);

        PyModule &module = getPyModule(file);
        module.schedule(cb);

        rec->pact = 1;
        return 0;
    } catch (...) {
        recGblSetSevr(rec, epicsAlarmWrite, epicsSevInvalid);
        // TODO: if (rec->tpro == 1) { log error }
        return -1;
    }
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
        DEVSUPFUN write{(DEVSUPFUN)processInpRecord<longinRecord>};
    } devPyDevLongin;
    epicsExportAddress(dset, devPyDevLongin);

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

    /*** OUTPUT RECORDS ***/
    struct
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initOutRecord<longoutRecord>};
        DEVSUPFUN get_ioint_info{nullptr};
        DEVSUPFUN write{(DEVSUPFUN)processOutRecord<longoutRecord>};
    } devPyDevLongout;
    epicsExportAddress(dset, devPyDevLongout);

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
}; // extern "C"
