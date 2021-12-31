/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <mbboRecord.h>

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

static long initRecord(mbboRecord* rec)
{
    std::string addr = rec->out.value.instio.string;
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

static long getIointInfo(int /*direction*/, mbboRecord *rec, IOSCANPVT* io)
{
    auto ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);
    if (ctx != nullptr && ctx->scan != nullptr) {
        *io = ctx->scan;
    }
    return 0;
}

static void processRecordCb(mbboRecord* rec)
{
    auto ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);

    auto fields = Util::getFields(rec->out.value.instio.string);
    for (auto& keyval: fields) {
        if      (keyval.first == "VAL")  keyval.second = std::to_string(rec->val);
        else if (keyval.first == "RVAL") keyval.second = std::to_string(rec->rval);
        else if (keyval.first == "NAME") keyval.second = rec->name;
        else if (keyval.first == "ZRVL") keyval.second = std::to_string(rec->zrvl);
        else if (keyval.first == "ONVL") keyval.second = std::to_string(rec->onvl);
        else if (keyval.first == "TWVL") keyval.second = std::to_string(rec->twvl);
        else if (keyval.first == "THVL") keyval.second = std::to_string(rec->thvl);
        else if (keyval.first == "FRVL") keyval.second = std::to_string(rec->frvl);
        else if (keyval.first == "FVVL") keyval.second = std::to_string(rec->fvvl);
        else if (keyval.first == "SXVL") keyval.second = std::to_string(rec->sxvl);
        else if (keyval.first == "SVVL") keyval.second = std::to_string(rec->svvl);
        else if (keyval.first == "EIVL") keyval.second = std::to_string(rec->eivl);
        else if (keyval.first == "NIVL") keyval.second = std::to_string(rec->nivl);
        else if (keyval.first == "TEVL") keyval.second = std::to_string(rec->tevl);
        else if (keyval.first == "ELVL") keyval.second = std::to_string(rec->elvl);
        else if (keyval.first == "TVVL") keyval.second = std::to_string(rec->tvvl);
        else if (keyval.first == "TTVL") keyval.second = std::to_string(rec->ttvl);
        else if (keyval.first == "FTVL") keyval.second = std::to_string(rec->ftvl);
        else if (keyval.first == "FFVL") keyval.second = std::to_string(rec->ffvl);
        else if (keyval.first == "ZRST") keyval.second = rec->zrst;
        else if (keyval.first == "ONST") keyval.second = rec->onst;
        else if (keyval.first == "TWST") keyval.second = rec->twst;
        else if (keyval.first == "THST") keyval.second = rec->thst;
        else if (keyval.first == "FRST") keyval.second = rec->frst;
        else if (keyval.first == "FVST") keyval.second = rec->fvst;
        else if (keyval.first == "SXST") keyval.second = rec->sxst;
        else if (keyval.first == "SVST") keyval.second = rec->svst;
        else if (keyval.first == "EIST") keyval.second = rec->eist;
        else if (keyval.first == "NIST") keyval.second = rec->nist;
        else if (keyval.first == "TEST") keyval.second = rec->test;
        else if (keyval.first == "ELST") keyval.second = rec->elst;
        else if (keyval.first == "TVST") keyval.second = rec->tvst;
        else if (keyval.first == "TTST") keyval.second = rec->ttst;
        else if (keyval.first == "FTST") keyval.second = rec->ftst;
        else if (keyval.first == "FFST") keyval.second = rec->ffst;
        else if (keyval.first == "TPRO") keyval.second = std::to_string(rec->tpro);
    }
    std::string code = Util::replaceFields(rec->out.value.instio.string, fields);

    try {
        PyWrapper::exec(code, (rec->tpro == 1), &rec->rval);
        ctx->processCbStatus = 0;
    } catch (...) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
        ctx->processCbStatus = -1;
    }
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}

static long processRecord(mbboRecord* rec)
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
    } devPyDevMbbo;
    epicsExportAddress(dset, devPyDevMbbo);

}; // extern "C"
