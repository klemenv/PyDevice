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
#include <lsoRecord.h>
#include <lsiRecord.h>
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
static long getIointInfo(int /*direction*/, T *rec, IOSCANPVT* io)
{
    auto ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);
    if (ctx != nullptr && ctx->scan != nullptr) {
        *io = ctx->scan;
    }
    return 0;
}

/**
 * @brief Common processing callback function for all records, executes Python code and updates record.
 * @param rec Record, only need (prio, dpvt) fields from it.
 * @param exec Function that executed Python code and optionally get new value.
 * @param needValue Input fields expect return value, but it's optional for output records.
 */
static void processCb(dbCommon* rec, std::function<bool()> exec, bool needValue)
{
    auto ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);
    try {
        if (exec() == false && needValue) {
            if (rec->tpro == 1) {
                printf("ERROR: Can't convert returned Python type to record type\n");
            }
            recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
            ctx->processCbStatus = -1;
        } else {
            ctx->processCbStatus = 0;
        }
    } catch (...) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
        ctx->processCbStatus = -1;
    }
    callbackRequestProcessCallback(&ctx->callback, rec->prio, rec);
}

/**
 * @brief Templated processCb for longin and longout records prepares Python code string and invokes common processCb().
 *
 * Updates record's link and replaces following strings with corresponding field values from record:
 * - %VAL%
 * - %NAME%
 * - %EGU%
 * - %HOPR%
 * - %LOPR%
 * - %HIGH%
 * - %HIHI%
 * - %LOW%
 * - %LOLO%
 */
template <typename T, typename std::enable_if<Util::is_any<T, longinRecord, longoutRecord>::value, T>::type* = nullptr>
void processCb(T* rec, const std::string& link, bool needValue)
{
    auto fields = Util::getReplacables(link);
    for (auto& keyval: fields) {
        if (keyval.first == "%VAL%")       keyval.second = std::to_string(rec->val);
        else if (keyval.first == "%NAME%") keyval.second = rec->name;
        else if (keyval.first == "%EGU%")  keyval.second = rec->egu;
        else if (keyval.first == "%HOPR%") keyval.second = std::to_string(rec->hopr);
        else if (keyval.first == "%LOPR%") keyval.second = std::to_string(rec->lopr);
        else if (keyval.first == "%HIGH%") keyval.second = std::to_string(rec->high);
        else if (keyval.first == "%HIHI%") keyval.second = std::to_string(rec->hihi);
        else if (keyval.first == "%LOW%")  keyval.second = std::to_string(rec->low);
        else if (keyval.first == "%LOLO%") keyval.second = std::to_string(rec->lolo);
    }
    std::string code = Util::replace(link, fields);
    auto worker = [code, rec]() { return PyWrapper::exec(code, (rec->tpro == 1), &rec->val); };
    processCb(reinterpret_cast<dbCommon*>(rec), worker, needValue);
}

/**
 * @brief Templated processCb for bi and bo records prepares Python code string and invokes common processCb().
 *
 * Updates record's link and replaces following strings with corresponding field values from record:
 * - %VAL%
 */
template <typename T, typename std::enable_if<Util::is_any<T, biRecord, boRecord>::value, T>::type* = nullptr>
void processCb(T* rec, const std::string& link, bool needValue)
{
    auto fields = Util::getReplacables(link);
    for (auto& keyval: fields) {
        if      (keyval.first == "%VAL%")  keyval.second = std::to_string(rec->val);
        else if (keyval.first == "%RVAL%") keyval.second = std::to_string(rec->rval);
        else if (keyval.first == "%NAME%") keyval.second = rec->name;
        else if (keyval.first == "%ZNAM%") keyval.second = rec->znam;
        else if (keyval.first == "%ONAM%") keyval.second = rec->onam;
    }
    std::string code = Util::replace(link, fields);
    auto worker = [code, rec]() { return PyWrapper::exec(code, (rec->tpro == 1), &rec->rval); };
    processCb(reinterpret_cast<dbCommon*>(rec), worker, needValue);
}

/**
 * @brief Templated processCb for mbbi and mbbo records prepares Python code string and invokes common processCb().
 *
 * Updates record's link and replaces following strings with corresponding field values from record:
 * - %VAL%
 */
template <typename T, typename std::enable_if<Util::is_any<T, mbbiRecord, mbboRecord>::value, T>::type* = nullptr>
void processCb(T* rec, const std::string& link, bool needValue)
{
    auto fields = Util::getReplacables(link);
    for (auto& keyval: fields) {
        if      (keyval.first == "%VAL%")  keyval.second = std::to_string(rec->val);
        else if (keyval.first == "%RVAL%") keyval.second = std::to_string(rec->rval);
        else if (keyval.first == "%NAME%") keyval.second = rec->name;
        else if (keyval.first == "%ZRVL%") keyval.second = std::to_string(rec->zrvl);
        else if (keyval.first == "%ONVL%") keyval.second = std::to_string(rec->onvl);
        else if (keyval.first == "%TWVL%") keyval.second = std::to_string(rec->twvl);
        else if (keyval.first == "%THVL%") keyval.second = std::to_string(rec->thvl);
        else if (keyval.first == "%FRVL%") keyval.second = std::to_string(rec->frvl);
        else if (keyval.first == "%FVVL%") keyval.second = std::to_string(rec->fvvl);
        else if (keyval.first == "%SXVL%") keyval.second = std::to_string(rec->sxvl);
        else if (keyval.first == "%SVVL%") keyval.second = std::to_string(rec->svvl);
        else if (keyval.first == "%EIVL%") keyval.second = std::to_string(rec->eivl);
        else if (keyval.first == "%NIVL%") keyval.second = std::to_string(rec->nivl);
        else if (keyval.first == "%TEVL%") keyval.second = std::to_string(rec->tevl);
        else if (keyval.first == "%ELVL%") keyval.second = std::to_string(rec->elvl);
        else if (keyval.first == "%TVVL%") keyval.second = std::to_string(rec->tvvl);
        else if (keyval.first == "%TTVL%") keyval.second = std::to_string(rec->ttvl);
        else if (keyval.first == "%FTVL%") keyval.second = std::to_string(rec->ftvl);
        else if (keyval.first == "%FFVL%") keyval.second = std::to_string(rec->ffvl);
        else if (keyval.first == "%ZRST%") keyval.second = rec->zrst;
        else if (keyval.first == "%ONST%") keyval.second = rec->onst;
        else if (keyval.first == "%TWST%") keyval.second = rec->twst;
        else if (keyval.first == "%THST%") keyval.second = rec->thst;
        else if (keyval.first == "%FRST%") keyval.second = rec->frst;
        else if (keyval.first == "%FVST%") keyval.second = rec->fvst;
        else if (keyval.first == "%SXST%") keyval.second = rec->sxst;
        else if (keyval.first == "%SVST%") keyval.second = rec->svst;
        else if (keyval.first == "%EIST%") keyval.second = rec->eist;
        else if (keyval.first == "%NIST%") keyval.second = rec->nist;
        else if (keyval.first == "%TEST%") keyval.second = rec->test;
        else if (keyval.first == "%ELST%") keyval.second = rec->elst;
        else if (keyval.first == "%TVST%") keyval.second = rec->tvst;
        else if (keyval.first == "%TTST%") keyval.second = rec->ttst;
        else if (keyval.first == "%FTST%") keyval.second = rec->ftst;
        else if (keyval.first == "%FFST%") keyval.second = rec->ffst;
    }
    std::string code = Util::replace(link, fields);
    auto worker = [code, rec]() { return PyWrapper::exec(code, (rec->tpro == 1), &rec->rval); };
    processCb(reinterpret_cast<dbCommon*>(rec), worker, needValue);
}

/**
 * @brief Templated processCb for ai records prepares Python code string and invokes common processCb().
 *
 * Updates record's link and replaces following strings with corresponding field values from record:
 * - %VAL%
 */
template <typename T, typename std::enable_if<Util::is_any<T, aiRecord>::value, T>::type* = nullptr>
void processCb(T* rec, const std::string& link, bool needValue)
{
    auto ctx = reinterpret_cast<PyDevContext*>(rec->dpvt);

    rec->val -= rec->aoff;
    if (rec->aslo != 0.0) rec->val /= rec->aslo;

    auto fields = Util::getReplacables(link);
    for (auto& keyval: fields) {
        if      (keyval.first == "%VAL%")  keyval.second = std::to_string(rec->val);
        else if (keyval.first == "%RVAL%") keyval.second = std::to_string(rec->rval);
        else if (keyval.first == "%ORAW%") keyval.second = std::to_string(rec->oraw);
        else if (keyval.first == "%NAME%") keyval.second = rec->name;
        else if (keyval.first == "%EGU%")  keyval.second = rec->egu;
        else if (keyval.first == "%HOPR%") keyval.second = std::to_string(rec->hopr);
        else if (keyval.first == "%LOPR%") keyval.second = std::to_string(rec->lopr);
        else if (keyval.first == "%PREC%") keyval.second = std::to_string(rec->prec);
    }
    std::string code = Util::replace(link, fields);
    auto worker = [code, rec]() {
        epicsFloat64 val;
        bool ret = PyWrapper::exec(code, (rec->tpro == 1), &val);
        if (ret == true) {
            val = (val * rec->aslo) + rec->aoff;
            if (rec->smoo == 0.0 || rec->udf)
                rec->val = val;
            else
                rec->val = (rec->val * rec->smoo) + (val * (1.0 - rec->smoo));
            rec->udf = 0;
        }
        return ret;
    };
    processCb(reinterpret_cast<dbCommon*>(rec), worker, needValue);
    if (ctx->processCbStatus == 0)
        ctx->processCbStatus = 2; // No convertion needed
}

/**
 * @brief Templated processCb for ai records prepares Python code string and invokes common processCb().
 *
 * Updates record's link and replaces following strings with corresponding field values from record:
 * - %VAL%
 */
template <typename T, typename std::enable_if<Util::is_any<T, aoRecord>::value, T>::type* = nullptr>
void processCb(T* rec, const std::string& link, bool needValue)
{
    rec->val = rec->oval - rec->aoff;
    if (rec->aslo != 0.0) rec->val /= rec->aslo;

    auto fields = Util::getReplacables(link);
    for (auto& keyval: fields) {
        if      (keyval.first == "%VAL%")  keyval.second = std::to_string(rec->val);
        else if (keyval.first == "%RVAL%") keyval.second = std::to_string(rec->rval);
        else if (keyval.first == "%ORAW%") keyval.second = std::to_string(rec->oraw);
        else if (keyval.first == "%NAME%") keyval.second = rec->name;
        else if (keyval.first == "%EGU%")  keyval.second = rec->egu;
        else if (keyval.first == "%HOPR%") keyval.second = std::to_string(rec->hopr);
        else if (keyval.first == "%LOPR%") keyval.second = std::to_string(rec->lopr);
        else if (keyval.first == "%PREC%") keyval.second = std::to_string(rec->prec);
    }
    std::string code = Util::replace(link, fields);
    auto worker = [code, rec]() {
        epicsFloat64 val;
        bool ret = PyWrapper::exec(code, (rec->tpro == 1), &val);
        if (ret == true) {
            rec->val = val;
            if (rec->aslo != 0.0) rec->val *= rec->aslo;
            rec->val += rec->aoff;
            rec->udf = 0;
        }
        return ret;
    };
    processCb(reinterpret_cast<dbCommon*>(rec), worker, needValue);
}

/**
 * @brief Templated processCb for stringin and stringout records prepares Python code string and invokes common processCb().
 *
 * Updates record's link and replaces following strings with corresponding field values from record:
 * - %VAL%
 */
template <typename T, typename std::enable_if<Util::is_any<T, stringinRecord, stringoutRecord>::value, T>::type* = nullptr>
void processCb(T* rec, const std::string& link, bool needValue)
{
    auto fields = Util::getReplacables(link);
    for (auto& keyval: fields) {
        if      (keyval.first == "%VAL%")  keyval.second = rec->val;
        else if (keyval.first == "%NAME%") keyval.second = rec->name;
    }
    std::string code = Util::replace(link, fields);
    auto worker = [code, rec]() {
        std::string val(rec->val);
        if (PyWrapper::exec(code, (rec->tpro == 1), val) == false) {
            return false;
        }
        strncpy(rec->val, val.c_str(), sizeof(rec->val)-1);
        rec->val[sizeof(rec->val)-1] = 0;
        return true;
    };
    processCb(reinterpret_cast<dbCommon*>(rec), worker, needValue);
}

/**
 */
template <typename T, typename std::enable_if<Util::is_any<T, lsiRecord, lsoRecord>::value, T>::type* = nullptr>
void processCb(T* rec, const std::string& link, bool needValue)
{
    auto fields = Util::getReplacables(link);
    for (auto& keyval: fields) {
        if (keyval.first == "%VAL%")       keyval.second = rec->val;
        else if (keyval.first == "%NAME%") keyval.second = rec->name;
        else if (keyval.first == "%SIZV%") keyval.second = std::to_string(rec->sizv);
        else if (keyval.first == "%LEN%")  keyval.second = std::to_string(rec->len);
    }
    std::string code = Util::replace(link, fields);
    auto worker = [code, rec]() {
        std::string val(rec->val);
        if (PyWrapper::exec(Util::escape_control_characters(code), (rec->tpro == 1), val) == false) {
            return false;
        }
        strncpy(rec->val, val.c_str(), rec->sizv - 1);
        rec->val[rec->sizv - 1] = 0;
        rec->len = strlen(rec->val) + 1;
        return true;
    };
    processCb(reinterpret_cast<dbCommon*>(rec), worker, needValue);
}

/**
 * @brief Templated processCb for ai and ao records prepares Python code string and invokes common processCb().
 *
 * Updates record's link and replaces following strings with corresponding field values from record:
 * - %VAL%
 */
template <typename T, typename std::enable_if<std::is_same<T, waveformRecord>::value, T>::type* = nullptr>
void processCb(T* rec, const std::string& link, bool /*needValue*/)
{
    auto fields = Util::getReplacables(link);
    for (auto& keyval: fields) {
        if (keyval.first == "%VAL%") {
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
    std::string code = Util::replace(link, fields);
    auto worker = [code, rec]() {
        if (rec->ftvl == menuFtypeFLOAT || rec->ftvl == menuFtypeDOUBLE) {
            std::vector<double> arr;
            return (PyWrapper::exec(code, (rec->tpro == 1), arr) && toRecArrayVal(rec, arr));
        } else {
            std::vector<long> arr;
            return (PyWrapper::exec(code, (rec->tpro == 1), arr) && toRecArrayVal(rec, arr));
        }
    };
    processCb(reinterpret_cast<dbCommon*>(rec), worker, false);
}

template <typename T>
static long processInpRecord(T* rec)
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
        processCb<T>(rec, rec->inp.value.instio.string, true);
    });
    return (scheduled ? 0 : -1);
}

template <typename T>
static long processOutRecord(T* rec)
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
        processCb<T>(rec, rec->out.value.instio.string, false);
    });
    return (scheduled ? 0 : -1);
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
        DEVSUPFUN init_record{(DEVSUPFUN)initInpRecord<lsiRecord>};
        DEVSUPFUN get_ioint_info{(DEVSUPFUN)getIointInfo<lsiRecord>};
        DEVSUPFUN write{(DEVSUPFUN)processInpRecord<lsiRecord>};
    } devPyDevLsi;
    epicsExportAddress(dset, devPyDevLsi);

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

    struct 
    {
        long number{5};
        DEVSUPFUN report{nullptr};
        DEVSUPFUN init{nullptr};
        DEVSUPFUN init_record{(DEVSUPFUN)initOutRecord<lsoRecord>};
        DEVSUPFUN get_ioint_info{(DEVSUPFUN)getIointInfo<lsoRecord>};
        DEVSUPFUN write{(DEVSUPFUN)processOutRecord<lsoRecord>};
    } devPyDevLso;
    epicsExportAddress(dset, devPyDevLso);
    

epicsShareFunc int pydev(const char *line)
{
    try {
        PyWrapper::exec(line, true);
    } catch (...) {
        // pass
    }
    return 0;
}

static const iocshArg pydevArg0 = { "pycode", iocshArgString };
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
