/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <biRecord.h>

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

static long initRecord(biRecord* rec)
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

static long getIointInfo(int /*direction*/, biRecord *rec, IOSCANPVT* io)
{
    auto ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);
    if (ctx != nullptr && ctx->scan != nullptr) {
        *io = ctx->scan;
    }
    return 0;
}

static void processRecordCb(biRecord* rec)
{
    auto ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);

    auto fields = Util::getFields(rec->inp.value.instio.string);
    for (auto& keyval: fields) {
        if      (keyval.first == "VAL")  keyval.second = Util::to_string(rec->val);
        else if (keyval.first == "RVAL") keyval.second = Util::to_string(rec->rval);
        else if (keyval.first == "NAME") keyval.second = rec->name;
        else if (keyval.first == "ZNAM") keyval.second = rec->znam;
        else if (keyval.first == "ONAM") keyval.second = rec->onam;
        else if (keyval.first == "TPRO") keyval.second = Util::to_string(rec->tpro);
    }
    std::string code = Util::replaceFields(rec->inp.value.instio.string, fields);

    try {
        if (PyWrapper::exec(code, (rec->tpro == 1), &rec->rval) == true) {
            ctx->processCbStatus = 0;
        } else {
            if (rec->tpro == 1) {
                printf("ERROR: Can't convert returned Python type to record type\n");
            }
            recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
            ctx->processCbStatus = -1;
        }
    } catch (...) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
        ctx->processCbStatus = -1;
    }
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}

static long processRecord(biRecord* rec)
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
        DEVSUPFUN write{(DEVSUPFUN)processRecord};
    } devPyDevBi;
    epicsExportAddress(dset, devPyDevBi);

}; // extern "C"
