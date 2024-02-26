/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <mbbiRecord.h>

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

static long initRecord(mbbiRecord* rec)
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

static long getIointInfo(int /*direction*/, mbbiRecord *rec, IOSCANPVT* io)
{
    auto ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);
    if (ctx != nullptr && ctx->scan != nullptr) {
        *io = ctx->scan;
    }
    return 0;
}

static void processRecordCb(mbbiRecord* rec)
{
    auto ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);

    std::string code = rec->inp.value.instio.string;
    std::map<std::string, Variant> args;
    for (auto& macro: Util::getMacros(code)) {
        if      (macro == "VAL")  { args["pydevVAL"]  = Variant(rec->val);  code = Util::replaceMacro(code, "VAL",  "pydevVAL");  }
        else if (macro == "RVAL") { args["pydevRVAL"] = Variant(rec->rval); code = Util::replaceMacro(code, "RVAL", "pydevRVAL"); }
        else if (macro == "NAME") { args["pydevNAME"] = Variant(rec->name); code = Util::replaceMacro(code, "NAME", "pydevNAME"); }
        else if (macro == "ZRVL") { args["pydevZRVL"] = Variant(rec->zrvl); code = Util::replaceMacro(code, "ZRVL", "pydevZRVL"); }
        else if (macro == "ONVL") { args["pydevONVL"] = Variant(rec->onvl); code = Util::replaceMacro(code, "ONVL", "pydevONVL"); }
        else if (macro == "TWVL") { args["pydevTWVL"] = Variant(rec->twvl); code = Util::replaceMacro(code, "TWVL", "pydevTWVL"); }
        else if (macro == "THVL") { args["pydevTHVL"] = Variant(rec->thvl); code = Util::replaceMacro(code, "THVL", "pydevTHVL"); }
        else if (macro == "FRVL") { args["pydevFRVL"] = Variant(rec->frvl); code = Util::replaceMacro(code, "FRVL", "pydevFRVL"); }
        else if (macro == "FVVL") { args["pydevFVVL"] = Variant(rec->fvvl); code = Util::replaceMacro(code, "FVVL", "pydevFVVL"); }
        else if (macro == "SXVL") { args["pydevSXVL"] = Variant(rec->sxvl); code = Util::replaceMacro(code, "SXVL", "pydevSXVL"); }
        else if (macro == "SVVL") { args["pydevSVVL"] = Variant(rec->svvl); code = Util::replaceMacro(code, "SVVL", "pydevSVVL"); }
        else if (macro == "EIVL") { args["pydevEIVL"] = Variant(rec->eivl); code = Util::replaceMacro(code, "EIVL", "pydevEIVL"); }
        else if (macro == "NIVL") { args["pydevNIVL"] = Variant(rec->nivl); code = Util::replaceMacro(code, "NIVL", "pydevNIVL"); }
        else if (macro == "TEVL") { args["pydevTEVL"] = Variant(rec->tevl); code = Util::replaceMacro(code, "TEVL", "pydevTEVL"); }
        else if (macro == "ELVL") { args["pydevELVL"] = Variant(rec->elvl); code = Util::replaceMacro(code, "ELVL", "pydevELVL"); }
        else if (macro == "TVVL") { args["pydevTVVL"] = Variant(rec->tvvl); code = Util::replaceMacro(code, "TVVL", "pydevTVVL"); }
        else if (macro == "TTVL") { args["pydevTTVL"] = Variant(rec->ttvl); code = Util::replaceMacro(code, "TTVL", "pydevTTVL"); }
        else if (macro == "FTVL") { args["pydevFTVL"] = Variant(rec->ftvl); code = Util::replaceMacro(code, "FTVL", "pydevFTVL"); }
        else if (macro == "FFVL") { args["pydevFFVL"] = Variant(rec->ffvl); code = Util::replaceMacro(code, "FFVL", "pydevFFVL"); }
        else if (macro == "ZRST") { args["pydevZRST"] = Variant(rec->zrst); code = Util::replaceMacro(code, "ZRST", "pydevZRST"); }
        else if (macro == "ONST") { args["pydevONST"] = Variant(rec->onst); code = Util::replaceMacro(code, "ONST", "pydevONST"); }
        else if (macro == "TWST") { args["pydevTWST"] = Variant(rec->twst); code = Util::replaceMacro(code, "TWST", "pydevTWST"); }
        else if (macro == "THST") { args["pydevTHST"] = Variant(rec->thst); code = Util::replaceMacro(code, "THST", "pydevTHST"); }
        else if (macro == "FRST") { args["pydevFRST"] = Variant(rec->frst); code = Util::replaceMacro(code, "FRST", "pydevFRST"); }
        else if (macro == "FVST") { args["pydevFVST"] = Variant(rec->fvst); code = Util::replaceMacro(code, "FVST", "pydevFVST"); }
        else if (macro == "SXST") { args["pydevSXST"] = Variant(rec->sxst); code = Util::replaceMacro(code, "SXST", "pydevSXST"); }
        else if (macro == "SVST") { args["pydevSVST"] = Variant(rec->svst); code = Util::replaceMacro(code, "SVST", "pydevSVST"); }
        else if (macro == "EIST") { args["pydevEIST"] = Variant(rec->eist); code = Util::replaceMacro(code, "EIST", "pydevEIST"); }
        else if (macro == "NIST") { args["pydevNIST"] = Variant(rec->nist); code = Util::replaceMacro(code, "NIST", "pydevNIST"); }
        else if (macro == "TEST") { args["pydevTEST"] = Variant(rec->test); code = Util::replaceMacro(code, "TEST", "pydevTEST"); }
        else if (macro == "ELST") { args["pydevELST"] = Variant(rec->elst); code = Util::replaceMacro(code, "ELST", "pydevELST"); }
        else if (macro == "TVST") { args["pydevTVST"] = Variant(rec->tvst); code = Util::replaceMacro(code, "TVST", "pydevTVST"); }
        else if (macro == "TTST") { args["pydevTTST"] = Variant(rec->ttst); code = Util::replaceMacro(code, "TTST", "pydevTTST"); }
        else if (macro == "FTST") { args["pydevFTST"] = Variant(rec->ftst); code = Util::replaceMacro(code, "FTST", "pydevFTST"); }
        else if (macro == "FFST") { args["pydevFFST"] = Variant(rec->ffst); code = Util::replaceMacro(code, "FFST", "pydevFFST"); }
        else if (macro == "TPRO") { args["pydevTPRO"] = Variant(rec->tpro); code = Util::replaceMacro(code, "TPRO", "pydevTPRO"); }
    }

    try {
        if (ctx->code != code) {
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

static long processRecord(mbbiRecord* rec)
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
    } devPyDevMbbi;
    epicsExportAddress(dset, devPyDevMbbi);

}; // extern "C"
