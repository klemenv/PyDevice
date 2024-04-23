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
#include <sstream>
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

template <typename T>
static void toRecArrayVal(waveformRecord* rec, const std::vector<T>& arr)
{
    if (rec->ftvl == menuFtypeCHAR) {
        auto val = reinterpret_cast<epicsInt8*>(rec->bptr);
        rec->nord = std::min(arr.size(), (size_t)rec->nelm);
        std::copy(arr.begin(), arr.begin()+rec->nord, val);
    } else if (rec->ftvl == menuFtypeUCHAR) {
        auto val = reinterpret_cast<epicsUInt8*>(rec->bptr);
        rec->nord = std::min(arr.size(), (size_t)rec->nelm);
        std::copy(arr.begin(), arr.begin()+rec->nord, val);
    } else if (rec->ftvl == menuFtypeSHORT) {
        auto val = reinterpret_cast<epicsInt16*>(rec->bptr);
        rec->nord = std::min(arr.size(), (size_t)rec->nelm);
        std::copy(arr.begin(), arr.begin()+rec->nord, val);
    } else if (rec->ftvl == menuFtypeUSHORT) {
        auto val = reinterpret_cast<epicsUInt16*>(rec->bptr);
        rec->nord = std::min(arr.size(), (size_t)rec->nelm);
        std::copy(arr.begin(), arr.begin()+rec->nord, val);
    } else if (rec->ftvl == menuFtypeLONG) {
        auto val = reinterpret_cast<epicsInt32*>(rec->bptr);
        rec->nord = std::min(arr.size(), (size_t)rec->nelm);
        std::copy(arr.begin(), arr.begin()+rec->nord, val);
    } else if (rec->ftvl == menuFtypeULONG) {
        auto val = reinterpret_cast<epicsUInt32*>(rec->bptr);
        rec->nord = std::min(arr.size(), (size_t)rec->nelm);
        std::copy(arr.begin(), arr.begin()+rec->nord, val);
    } else if (rec->ftvl == menuFtypeFLOAT) {
        auto val = reinterpret_cast<epicsFloat32*>(rec->bptr);
        rec->nord = std::min(arr.size(), (size_t)rec->nelm);
        std::copy(arr.begin(), arr.begin()+rec->nord, val);
    } else if (rec->ftvl == menuFtypeDOUBLE) {
        auto val = reinterpret_cast<epicsFloat64*>(rec->bptr);
        rec->nord = std::min(arr.size(), (size_t)rec->nelm);
        std::copy(arr.begin(), arr.begin()+rec->nord, val);
    } else {
        throw Variant::ConvertError();
    }
}

template <>
void toRecArrayVal<std::string>(waveformRecord* rec, const std::vector<std::string>& arr)
{

    if (!rec->ftvl == menuFtypeSTRING) {
        throw Variant::ConvertError();
    }

    rec->nord = std::min(arr.size(), (size_t)rec->nelm);
    auto val = reinterpret_cast<char*>(rec->bptr);

    for (size_t i = 0; i < rec->nord; ++i) {
        char *cptr = &val[i * MAX_STRING_SIZE];
        std::string sval = arr[i];
        if (rec->tpro) {
            // need to foresee space for last '\0'
            if (sval.size() > MAX_STRING_SIZE - 1) {
                // Indicate on console which element will be truncated where
                std::stringstream strm;
                strm << rec->name << "[" << i << "]: '";
                const std::string info = strm.str();
                printf("%s%s' too long\n%s^\n", info.c_str(), sval.c_str(),
                       std::string(info.size() + MAX_STRING_SIZE - 1, ' ').c_str()
                );
            }
        }
        std::string cval = sval.substr(0, MAX_STRING_SIZE - 1);
        std::copy(cval.begin(), cval.end(), cptr);
        cptr[MAX_STRING_SIZE - 1] = '\0'; /* sentinel */
    }
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


std::string arrayOfStrToStr(const std::vector<std::string>& val)
{
    std::string value = "[";
    for (const auto v: val) {
        value += "b\"" +  Util::escape(v) + "\",";
    }
    if (value.back() == ',') {
        value.back() = ']';
    } else {
        value += "]";
    }
    return value;
}

static void processRecordCb(waveformRecord* rec)
{
    auto ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);

    std::string code = rec->inp.value.instio.string;
    std::map<std::string, Variant> args;
    for (auto& macro: Util::getMacros(code)) {
        if (macro == "VAL")  {
            std::vector<long long int> vl;
            std::vector<double> vd;
            std::vector<std::string> vs;
            if      (rec->ftvl == menuFtypeCHAR)   { auto a = reinterpret_cast<epicsInt8*>(rec->bptr);    vl.assign(a, a+rec->nelm); }
            else if (rec->ftvl == menuFtypeUCHAR)  { auto a = reinterpret_cast<epicsUInt8*>(rec->bptr);   vl.assign(a, a+rec->nelm); }
            else if (rec->ftvl == menuFtypeSHORT)  { auto a = reinterpret_cast<epicsInt16*>(rec->bptr);   vl.assign(a, a+rec->nelm); }
            else if (rec->ftvl == menuFtypeUSHORT) { auto a = reinterpret_cast<epicsUInt16*>(rec->bptr);  vl.assign(a, a+rec->nelm); }
            else if (rec->ftvl == menuFtypeLONG)   { auto a = reinterpret_cast<epicsInt32*>(rec->bptr);   vl.assign(a, a+rec->nelm); }
            else if (rec->ftvl == menuFtypeULONG)  { auto a = reinterpret_cast<epicsUInt32*>(rec->bptr);  vl.assign(a, a+rec->nelm); }
            else if (rec->ftvl == menuFtypeFLOAT)  { auto a = reinterpret_cast<epicsFloat32*>(rec->bptr); vd.assign(a, a+rec->nelm); }
            else if (rec->ftvl == menuFtypeDOUBLE) { auto a = reinterpret_cast<epicsFloat64*>(rec->bptr); vd.assign(a, a+rec->nelm); }
            else if (rec->ftvl == menuFtypeSTRING) {
                vs.resize(rec->nord);
                auto a = reinterpret_cast<char*>(rec->bptr);
                for(size_t i=0; i<rec->nord; ++i){
                    const char *cptr = &a[i * MAX_STRING_SIZE];
                    vs[i] = std::string(cptr, 0, MAX_STRING_SIZE);
                }
            }

            if (!vl.empty()) args["pydevVAL"] = Variant(vl);
            if (!vd.empty()) args["pydevVAL"] = Variant(vd);
            if (!vs.empty()) args["pydevVAL"] = Variant(vs);
            code = Util::replaceMacro(code, "VAL", "pydevVAL");
        } else if (macro == "TPRO") {
            args["pydevTPRO"] = Variant(rec->tpro);
            code = Util::replaceMacro(code, "TPRO", "pydevTPRO");
        }
    }

    try {
        if (ctx->code != code) {
            PyWrapper::destroy(std::move(ctx->bytecode));
            ctx->bytecode = PyWrapper::compile(code, (rec->tpro == 1));
            ctx->code = code;
        }
        auto val = PyWrapper::eval(ctx->bytecode, args, (rec->tpro == 1));

        if (rec->ftvl == menuFtypeFLOAT || rec->ftvl == menuFtypeDOUBLE) {
            std::vector<double> arr = val.get_double_array();
            toRecArrayVal(rec, arr);
        } else if (rec->ftvl == menuFtypeSTRING) {
            std::vector<std::string> arr = val.get_string_array();
            toRecArrayVal(rec, arr);
        } else {
            std::vector<long long int> arr = val.get_long_array();
            toRecArrayVal(rec, arr);
        }

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
