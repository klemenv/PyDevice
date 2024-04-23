/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <longinRecord.h>

#include <alarm.h>
#include <callback.h>
#include <cantProceed.h>
#include <dbScan.h>
#include <devSup.h>
#include <epicsExport.h>
#include <recGbl.h>

#include <map>
#include <string.h>

#include "asyncexec.h"
#include "pywrapper.h"
#include "util.h"

struct PyDevContext {
    CALLBACK callback;
    IOSCANPVT scan;
    int processCbStatus;
    std::string code;
    PyWrapper::ByteCode bytecode;
};

static std::map<std::string, IOSCANPVT> ioScanPvts;

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

static long initRecord(longinRecord* rec)
{
    std::string addr = rec->inp.value.instio.string;
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

static long getIointInfo(int /*direction*/, longinRecord *rec, IOSCANPVT* io)
{
    auto ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);
    if (ctx != nullptr && ctx->scan != nullptr) {
        *io = ctx->scan;
    }
    return 0;
}

static void processRecordCb(longinRecord* rec)
{
    auto ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);

    std::string code = rec->inp.value.instio.string;
    std::map<std::string, Variant> args;
    for (auto& macro: Util::getMacros(code)) {
        if      (macro == "VAL")  { args["pydevVAL"]  = Variant(rec->val);  code = Util::replaceMacro(code, "VAL",  "pydevVAL");  }
        else if (macro == "NAME") { args["pydevNAME"] = Variant(rec->name); code = Util::replaceMacro(code, "NAME", "pydevNAME"); }
        else if (macro == "EGU")  { args["pydevEGU"]  = Variant(rec->egu);  code = Util::replaceMacro(code, "EGU",  "pydevEGU");  }
        else if (macro == "HOPR") { args["pydevHOPR"] = Variant(rec->hopr); code = Util::replaceMacro(code, "HOPR", "pydevHOPR"); }
        else if (macro == "LOPR") { args["pydevLOPR"] = Variant(rec->lopr); code = Util::replaceMacro(code, "LOPR", "pydevLOPR"); }
        else if (macro == "HIGH") { args["pydevHIGH"] = Variant(rec->high); code = Util::replaceMacro(code, "HIGH", "pydevHIGH"); }
        else if (macro == "HIHI") { args["pydevHIHI"] = Variant(rec->hihi); code = Util::replaceMacro(code, "HIHI", "pydevHIHI"); }
        else if (macro == "LOW")  { args["pydevLOW"]  = Variant(rec->low);  code = Util::replaceMacro(code, "LOW",  "pydevLOW"); }
        else if (macro == "LOLO") { args["pydevLOLO"] = Variant(rec->lolo); code = Util::replaceMacro(code, "LOLO", "pydevLOLO"); }
        else if (macro == "TPRO") { args["pydevTPRO"] = Variant(rec->tpro); code = Util::replaceMacro(code, "TPRO", "pydevTPRO"); }
    }

    try {
        if (ctx->code != code) {
            PyWrapper::destroy(std::move(ctx->bytecode));
            ctx->bytecode = PyWrapper::compile(code, (rec->tpro == 1));
            ctx->code = code;
        }
        rec->val = PyWrapper::eval(ctx->bytecode, args, (rec->tpro == 1)).get_long();
        rec->udf = 0;
        ctx->processCbStatus = 0;

    } catch (std::exception& e) {
        if (rec->tpro == 1) {
            printf("[%s] %s\n", rec->name, e.what());
        }
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
        ctx->processCbStatus = -1;
    }
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}

static long processRecord(longinRecord* rec)
{
    auto ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);
    if (ctx == nullptr) {
        // Keep PACT=1 to prevent further processing
        rec->pact = 1;
        recGblSetSevr(rec, epicsAlarmUDF, epicsSevInvalid);
        return -1;
    }

    if (rec->pact == 1) {
        rec->pact = 0;
        return ctx->processCbStatus;
    }
    rec->pact = 1;

    auto scheduled = AsyncExec::schedule([rec]() {
        processRecordCb(rec);
    });
    return (scheduled ? 0 : -1);
}

extern "C"
{
    struct
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initRecord};
        DEVSUPFUN get_ioint_info{(DEVSUPFUN)getIointInfo};
        DEVSUPFUN process{(DEVSUPFUN)processRecord};
    } devPyDevLongin;
    epicsExportAddress(dset, devPyDevLongin);

}; // extern "C"
