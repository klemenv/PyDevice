/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Record Support Routines for pyRecord */
/*
 *      Author: Klemen Vodopivec
 *      Date:   12-18-2021
 */

#include "alarm.h"
// #include "alarmString.h"
#include "callback.h"
#include "cantProceed.h"
#include "dbAccess.h"
// #include "dbEvent.h"
// #include "devSup.h"
// #include "epicsTime.h"
#include "epicsVersion.h"
#include "errlog.h"
// #include "menuYesNo.h"
#include "recSup.h"
#include "recGbl.h"

#include <string>
#include <cstring>

#include "asyncexec.h"
#include "pywrapper.h"
#include "util.h"

#define GEN_SIZE_OFFSET
#include "pyRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

#ifdef VERSION_INT
#  if EPICS_VERSION_INT < VERSION_INT(3,16,0,2)
#    define dbLinkIsConstant(lnk) ((lnk)->type == CONSTANT)
#    define RECSUPFUN_CAST (RECSUPFUN)
#  else
#    define RECSUPFUN_CAST
#  endif
#else
#  define dbLinkIsConstant(lnk) ((lnk)->type == CONSTANT)
#  define RECSUPFUN_CAST (RECSUPFUN)
#endif

static long initRecord(dbCommon *, int);
static long processRecord(dbCommon *);
static long updateRecordField(DBADDR *addr, int after);
static long convertDbAddr(DBADDR *addr);
static long getArrayInfo(DBADDR *paddr, long *no_elements, long *offset);
//static void checkLinksCallback(epicsCallback *arg);
//static void checkLinks(pyRecord *rec);
static long fetchValues(pyRecord *rec);

struct PyRecordContext {
    CALLBACK callback;
    int processCbStatus;
};

rset pyRSET = {
    .number = RSETNUMBER,
    .report = NULL,
    .init = NULL,
    .init_record = RECSUPFUN_CAST initRecord,
    .process = RECSUPFUN_CAST processRecord,
//    .special = RECSUPFUN_CAST updateRecordField,
    .special = NULL,
    .get_value = NULL,
    .cvt_dbaddr = RECSUPFUN_CAST convertDbAddr,
    .get_array_info = RECSUPFUN_CAST getArrayInfo,
    .put_array_info = NULL,
    .get_units = NULL,
    .get_precision = NULL,
    .get_enum_str = NULL,
    .get_enum_strs = NULL,
    .put_enum_str = NULL,
    .get_graphic_double = NULL,
    .get_control_double = NULL,
    .get_alarm_double = NULL,
};
epicsExportAddress(rset, pyRSET);

static long initRecord(dbCommon *common, int pass)
{
    auto rec = reinterpret_cast<struct pyRecord *>(common);

    if (pass == 0) {
        // Allocate record context
        auto buffer = callocMustSucceed(1, sizeof(struct PyRecordContext), "pyRecord::initRecord");
        rec->ctx = new (buffer) PyRecordContext;

        // Allocate value fields
        for (int i = 0; i < PYREC_NARGS; i++) {
            auto ft = &rec->fta + i;
            auto no = &rec->noa + i;
            auto val = &rec->a +  i;
            auto ne = &rec->nea + i;

            if (*ft >= DBF_ENUM) {
                *ft = DBF_CHAR;
            }
            if (*no == 0) {
                *no = 1;
            }

            auto size = dbValueSize(*ft);
            *val = callocMustSucceed(*no, size, "pyRecord::initRecord");
            *ne = *no;
        }

        // Allocate output VAL field
        if (rec->ftvl >= DBF_ENUM) {
            rec->ftvl = DBF_CHAR;
        }
        auto size = dbValueSize(rec->ftvl);
        if (rec->ftvl == DBF_STRING) {
            rec->val = callocMustSucceed(PYREC_RETSTRMAXLEN, size, "pyRecord::initRecord");
            reinterpret_cast<char*>(rec->val)[0] = 0;
        } else {
            rec->val = callocMustSucceed(1, size, "pyRecord::initRecord");
        }
        return 0;
    }

    // Initialize input links
    for (int i = 0; i < PYREC_NARGS; i++) {
        auto inp = &rec->inpa + i;
        auto dbr = &rec->fta  + i;
        auto val = &rec->a    + i;
        long no  = *(&rec->noa  + i);

        dbLoadLinkArray(inp, *dbr, *val, &no);
        if (no > 0) {
            auto ne = &rec->nea + i;
            *ne = no;
        }
    }

    return 0;
}

static void processRecordCb(pyRecord* rec)
{
    auto fields = Util::getReplacables(rec->exec);
    for (auto& keyval: fields) {
        if      (keyval.first == "%NAME%") keyval.second = rec->name;
        else {
            for (auto i = 0; i < PYREC_NARGS; i++) {
                std::string field = "%" + std::string(1,'A'+i) + "%";
                if (keyval.first == field) {
                    auto val = &rec->a + i;
                    auto ft  = &rec->fta + i;
                    auto no  = &rec->noa + i;
                    if (*no == 1) {
                        if      (*ft == DBR_CHAR)   keyval.second = std::to_string((reinterpret_cast<   epicsInt8*>(*val))[0]);
                        else if (*ft == DBR_UCHAR)  keyval.second = std::to_string((reinterpret_cast<  epicsUInt8*>(*val))[0]);
                        else if (*ft == DBR_SHORT)  keyval.second = std::to_string((reinterpret_cast<  epicsInt16*>(*val))[0]);
                        else if (*ft == DBR_USHORT) keyval.second = std::to_string((reinterpret_cast< epicsUInt16*>(*val))[0]);
                        else if (*ft == DBR_LONG)   keyval.second = std::to_string((reinterpret_cast<  epicsInt32*>(*val))[0]);
                        else if (*ft == DBR_ULONG)  keyval.second = std::to_string((reinterpret_cast< epicsUInt32*>(*val))[0]);
                        else if (*ft == DBR_INT64)  keyval.second = std::to_string((reinterpret_cast<  epicsInt64*>(*val))[0]);
                        else if (*ft == DBR_UINT64) keyval.second = std::to_string((reinterpret_cast< epicsUInt64*>(*val))[0]);
                        else if (*ft == DBR_FLOAT)  keyval.second = std::to_string((reinterpret_cast<epicsFloat32*>(*val))[0]);
                        else if (*ft == DBR_DOUBLE) keyval.second = std::to_string((reinterpret_cast<epicsFloat64*>(*val))[0]);
                        else if (*ft == DBR_STRING) keyval.second = reinterpret_cast<char*>(val)[0];
                    }
                }
            }
        }
    }
    std::string code = Util::replace(rec->exec, fields);

    try {
        bool ret = false;
        if      (rec->ftvl == DBR_CHAR)   ret = PyWrapper::exec(code, (rec->tpro == 1), reinterpret_cast<   epicsInt8*>(rec->val));
        else if (rec->ftvl == DBR_UCHAR)  ret = PyWrapper::exec(code, (rec->tpro == 1), reinterpret_cast<  epicsUInt8*>(rec->val));
        else if (rec->ftvl == DBR_SHORT)  ret = PyWrapper::exec(code, (rec->tpro == 1), reinterpret_cast<  epicsInt16*>(rec->val));
        else if (rec->ftvl == DBR_USHORT) ret = PyWrapper::exec(code, (rec->tpro == 1), reinterpret_cast< epicsUInt16*>(rec->val));
        else if (rec->ftvl == DBR_LONG)   ret = PyWrapper::exec(code, (rec->tpro == 1), reinterpret_cast<  epicsInt32*>(rec->val));
        else if (rec->ftvl == DBR_ULONG)  ret = PyWrapper::exec(code, (rec->tpro == 1), reinterpret_cast< epicsUInt32*>(rec->val));
        else if (rec->ftvl == DBR_INT64)  ret = PyWrapper::exec(code, (rec->tpro == 1), reinterpret_cast<  epicsInt64*>(rec->val));
        else if (rec->ftvl == DBR_UINT64) ret = PyWrapper::exec(code, (rec->tpro == 1), reinterpret_cast< epicsUInt64*>(rec->val));
        else if (rec->ftvl == DBR_FLOAT)  ret = PyWrapper::exec(code, (rec->tpro == 1), reinterpret_cast<epicsFloat32*>(rec->val));
        else if (rec->ftvl == DBR_DOUBLE) ret = PyWrapper::exec(code, (rec->tpro == 1), reinterpret_cast<epicsFloat64*>(rec->val));
        else if (rec->ftvl == DBR_STRING) {
            std::string ret;
            PyWrapper::exec(code, (rec->tpro == 1), ret);
            strncpy(reinterpret_cast<char*>(rec->val), ret.c_str(), PYREC_RETSTRMAXLEN);
            reinterpret_cast<char*>(rec->val)[PYREC_RETSTRMAXLEN-1] = 0;
        }
        rec->ctx->processCbStatus = (ret ? 0 : -1);
    } catch (...) {
        rec->ctx->processCbStatus = -1;
    }
    callbackRequestProcessCallback(&rec->ctx->callback, rec->prio, rec);
}

static long processRecord(dbCommon *common)
{
    auto rec = reinterpret_cast<struct pyRecord *>(common);

    if (rec->pact == 0) {
        rec->pact = 1;
        if (fetchValues(rec) != 0) {
            recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
            rec->pact = 0;
            return -1;
        }

        auto scheduled = AsyncExec::schedule([rec]() {
            processRecordCb(rec);
        });
        return (scheduled ? 0 : -1);
    }

    if (rec->ctx->processCbStatus == -1) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
        rec->pact = 0;
        return -1;
    }

    recGblGetTimeStamp(rec);

    auto monitor_mask = recGblResetAlarms(rec) | DBE_VALUE | DBE_LOG;
    db_post_events(rec, rec->val, monitor_mask);

    recGblFwdLink(rec);
    rec->pact = 0;

    return 0;
}

static long fetchValues(pyRecord *rec)
{
    for (auto i = 0; i < PYREC_NARGS; i++) {
        auto inp = &rec->inpa + i;
        auto fta = &rec->fta  + i;
        auto no  = &rec->noa + i;
        auto ne  = &rec->nea + i;
        auto val = &rec->a + i;
        long n = *no;
        if (!dbLinkIsConstant(inp)) {
            auto status = dbGetLink(inp, *fta, *val, 0, &n);
            if (status) {
                return status;
            }
        }
        *ne = n;
    }
    return 0;
}

static long convertDbAddr(DBADDR *paddr)
{
    auto rec = reinterpret_cast<pyRecord *>(paddr->precord);
    int field = dbGetFieldIndex(paddr);

    if (field >= pyRecordA && field < (pyRecordA + PYREC_NARGS)) {
        int offset = field - pyRecordA;

        paddr->pfield      = *(&rec->a   + offset);
        paddr->no_elements = *(&rec->noa + offset);
        paddr->field_type  = *(&rec->fta + offset);
    } else if (field == pyRecordVAL) {
        paddr->pfield = rec->val;
        paddr->no_elements = (rec->ftvl == DBR_STRING ? strlen(reinterpret_cast<char*>(rec->val))+1 : 1);
        paddr->field_type = rec->ftvl;
    } else {
        errlogPrintf("pyRecord::convertDbAddr called for %s.%s\n", rec->name, paddr->pfldDes->name);
        return 0;
    }
    paddr->dbr_field_type = paddr->field_type;
    paddr->field_size     = dbValueSize(paddr->field_type);
    return 0;
}

static long getArrayInfo(DBADDR *paddr, long *no_elements, long *offset)
{
    auto rec = reinterpret_cast<pyRecord *>(paddr->precord);
    int field = dbGetFieldIndex(paddr);

    if (field >= pyRecordA && field < (pyRecordA + PYREC_NARGS)) {
        int off = field - pyRecordA;
        *no_elements = *(&rec->nea + off);
    } else if (field == pyRecordVAL) {
        *no_elements = (rec->ftvl == DBR_STRING ? strlen(reinterpret_cast<char*>(rec->val))+1 : 1);
    } else {
        errlogPrintf("pyRecord::getArrayInfo called for %s.%s\n", rec->name, paddr->pfldDes->name);
    }
    *offset = 0;

    return 0;
}