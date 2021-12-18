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
// #include "cantProceed.h"
#include "dbAccess.h"
// #include "dbEvent.h"
// #include "devSup.h"
// #include "epicsTime.h"
#include "epicsVersion.h"
// #include "errlog.h"
// #include "menuYesNo.h"
#include "recSup.h"
#include "recGbl.h"

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
//static void checkLinksCallback(epicsCallback *arg);
//static void checkLinks(pyRecord *rec);
static long fetchValues(pyRecord *rec);

struct PyRecordContext {
    CALLBACK callback;
    IOSCANPVT scan;
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
    .cvt_dbaddr = NULL,
    .get_array_info = NULL,
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

            if (*ft > DBF_ENUM) {
                *ft = DBF_CHAR;
            }
            if (*no == 0) {
                *no = 1;
            }

            auto size = dbValueSize(*ft);
            *val = callocMustSucceed(*no, size, "pyRecord::initRecord");
            *ne = *no;
        }
        return 0;
    }

    // Initialize input links
    for (int i = 0; i < PYREC_NARGS; i++) {
        auto inp = &rec->inpa + i;
        auto dbr = &rec->fta  + i;
        auto val = &rec->a    + i;
        long no  = *(&rec->noa  + i);

        dbLoadLinkArray(inp, *dbr, val, &no);
        if (no > 0) {
            auto ne = &rec->nea + i;
            *ne = no;
        }
    }

    return 0;
}

static void processRecordCb(pyRecord* rec)
{
//    auto fields = Util::getReplacables(rec->out.value.instio.string);
//    for (auto& keyval: fields) {
//        if      (keyval.first == "%VAL%")  keyval.second = std::to_string(rec->val);
//        else if (keyval.first == "%RVAL%") keyval.second = std::to_string(rec->rval);
//    }
//    std::string code = Util::replace(rec->out.value.instio.string, fields);
    std::string code = "print('hello world pyRecord')";

    try {
        PyWrapper::exec(code, (rec->tpro == 1));
        rec->ctx->processCbStatus = 0;
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

    // TODO: post monitors
    recGblGetTimeStamp(rec);
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
        auto status = dbGetLink(inp, *fta, val, 0, &n);
        if (status) {
            return status;
        }
        *ne = n;
    }
    return 0;
}