/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <waveformRecord.h>

#include <alarm.h>
#include <callback.h>
#include <cantProceed.h>
#include <dbScan.h>
#include <devSup.h>
#include <epicsExport.h>
#include <menuFtype.h>
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

static long initRecord(waveformRecord* rec)
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

static long getIointInfo(int /*direction*/, waveformRecord *rec, IOSCANPVT* io)
{
    auto ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);
    if (ctx != nullptr && ctx->scan != nullptr) {
        *io = ctx->scan;
    }
    return 0;
}

static void processRecordCb(waveformRecord* rec)
{
    auto ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);

    auto fields = Util::getFields(rec->inp.value.instio.string);
    for (auto& keyval: fields) {
        if (keyval.first == "VAL") {
            if (rec->ftvl == menuFtypeFLOAT || rec->ftvl == menuFtypeDOUBLE) {
                std::vector<double> arr;
                if (fromRecArrayVal(rec, arr) == true) {
                    keyval.second = Util::arrayToStr(arr);
                }
            } else {
                std::vector<long> arr;
                if (fromRecArrayVal(rec, arr) == true) {
                    keyval.second = Util::arrayToStr(arr);
                }
            }


        }
    }
    std::string code = Util::replaceFields(rec->inp.value.instio.string, fields);

    try {
        bool ret;
        if (rec->ftvl == menuFtypeFLOAT || rec->ftvl == menuFtypeDOUBLE) {
            std::vector<double> arr;
            ret = (PyWrapper::exec(code, (rec->tpro == 1), arr) && toRecArrayVal(rec, arr));
        } else {
            std::vector<long> arr;
            ret = (PyWrapper::exec(code, (rec->tpro == 1), arr) && toRecArrayVal(rec, arr));
        }

        if (ret == true) {
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

static long processRecord(waveformRecord* rec)
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
    } devPyDevWaveform;
    epicsExportAddress(dset, devPyDevWaveform);

}; // extern "C"
