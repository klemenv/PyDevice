/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

/* Record Support Routines for pycalcRecord */
/*
 *      Author: Klemen Vodopivec
 *      Date:   12-18-2021
 */

#include "alarm.h"
#include "callback.h"
#include "cantProceed.h"
#include "dbAccess.h"
#include "dbConvertFast.h"
#include "dbEvent.h"
#include "devSup.h"
#include "epicsVersion.h"
#include "errlog.h"
#include "recSup.h"
#include "recGbl.h"

#include <string>
#include <cstring>

#include "asyncexec.h"
#include "pywrapper.h"
#include "util.h"

#define GEN_SIZE_OFFSET
#include "pycalcRecord.h"
#undef  GEN_SIZE_OFFSET
#include "epicsExport.h"

#ifdef VERSION_INT
#  if EPICS_VERSION_INT < VERSION_INT(3,16,0,2)
#    define dbLinkIsConstant(lnk) ((lnk)->type == CONSTANT)
#    define RECSUPFUN_CAST (RECSUPFUN)
#  else
#    define RECSUPFUN_CAST
#    define HAVE_EPICS_INT64
#  endif
#else
#  define dbLinkIsConstant(lnk) ((lnk)->type == CONSTANT)
#  define RECSUPFUN_CAST (RECSUPFUN)
#endif

static long initRecord(dbCommon *, int);
static long processRecord(dbCommon *);
//static long updateRecordField(DBADDR *addr, int after);
static long convertDbAddr(DBADDR *addr);
static long getArrayInfo(DBADDR *paddr, long *no_elements, long *offset);
static long fetchValues(pycalcRecord *rec);

struct PyCalcRecordContext {
    CALLBACK callback;
    int processCbStatus;
    std::string code;
    PyWrapper::ByteCode bytecode;
};

rset pycalcRSET = {
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
epicsExportAddress(rset, pycalcRSET);

static long initRecord(dbCommon *common, int pass)
{
    auto rec = reinterpret_cast<struct pycalcRecord *>(common);

    if (pass == 0) {
        // Allocate record context
        auto buffer = callocMustSucceed(1, sizeof(struct PyCalcRecordContext), "pycalcRecord::initRecord");
        rec->ctx = new (buffer) PyCalcRecordContext;

        // Allocate value fields
        for (int i = 0; i < PYCALCREC_NARGS; i++) {
            auto ft  = &rec->fta  + i;
            auto val = &rec->a    + i;
            auto siz = &rec->siza + i;
            auto me  = &rec->mea  + i;
            auto ne  = &rec->nea  + i;

            if (*ft > DBF_ENUM) {
                *ft = DBF_CHAR;
            }
            if (*me < 1) {
                *me = 1;
            }
            *ne = (*me == 1 ? 1 : 0);

            *siz = dbValueSize(*ft);
            *val = callocMustSucceed(*me, *siz, "pycalcRecord::initRecord");
        }

        // Allocate output VAL field for longest possible value
        if (rec->mevl < 1) {
            rec->mevl = 1;
        }
        rec->val = callocMustSucceed(rec->mevl, dbValueSize(rec->ftvl), "pycalcRecord::initRecord");
        reinterpret_cast<char*>(rec->val)[0] = 0;
        return 0;
    }

    // Initialize input links
    for (int i = 0; i < PYCALCREC_NARGS; i++) {
        auto inp = &rec->inpa + i;
        auto ft  = &rec->fta  + i;
        auto val = &rec->a    + i;

        // Only initialize constants, we fetch link values every time the record processes
        if (dbLinkIsConstant(inp)) {
            recGblInitConstantLink(inp, *ft, *val);
        }
    }

    return 0;
}

static void processRecordCb(pycalcRecord* rec)
{
    auto ctx = reinterpret_cast<PyCalcRecordContext*>(rec->ctx);
    std::string code = rec->calc;
    std::map<std::string, Variant> args;
    for (auto& macro: Util::getMacros(code)) {
        if      (macro == "NAME") { args["pydevNAME"] = Variant(rec->name); code = Util::replaceMacro(code, "NAME", "pydevNAME"); }
        else if (macro == "TPRO") { args["pydevTPRO"] = Variant(rec->tpro); code = Util::replaceMacro(code, "TPRO", "pydevTPRO"); }
        else {
            for (auto i = 0; i < PYCALCREC_NARGS; i++) {
                std::string field = std::string(1,'A'+i);
                if (macro == field) {
                    auto val = &rec->a   + i;
                    auto ft  = &rec->fta + i;
                    auto me  = &rec->mea + i;
                    auto ne  = &rec->nea + i;

                    code = Util::replaceMacro(code, field, "pydev"+field);
                    if (*me == 1) {
                        if      (*ft == DBR_CHAR)   { args["pydev"+field] = Variant(*reinterpret_cast<   epicsInt8*>(*val)); }
                        else if (*ft == DBR_UCHAR)  { args["pydev"+field] = Variant(*reinterpret_cast<  epicsUInt8*>(*val)); }
                        else if (*ft == DBR_SHORT)  { args["pydev"+field] = Variant(*reinterpret_cast<  epicsInt16*>(*val)); }
                        else if (*ft == DBR_USHORT) { args["pydev"+field] = Variant(*reinterpret_cast< epicsUInt16*>(*val)); }
                        else if (*ft == DBR_LONG)   { args["pydev"+field] = Variant(*reinterpret_cast<  epicsInt32*>(*val)); }
                        else if (*ft == DBR_ULONG)  { args["pydev"+field] = Variant(*reinterpret_cast< epicsUInt32*>(*val)); }
#ifdef HAVE_EPICS_INT64
                        else if (*ft == DBR_INT64)  { args["pydev"+field] = Variant(*reinterpret_cast<  epicsInt64*>(*val)); }
                        else if (*ft == DBR_UINT64) { args["pydev"+field] = Variant(*reinterpret_cast< epicsUInt64*>(*val)); }
#endif
                        else if (*ft == DBR_FLOAT)  { args["pydev"+field] = Variant(*reinterpret_cast<epicsFloat32*>(*val)); }
                        else if (*ft == DBR_DOUBLE) { args["pydev"+field] = Variant(*reinterpret_cast<epicsFloat64*>(*val)); }
                        else if (*ft == DBR_STRING) { args["pydev"+field] = Variant( reinterpret_cast<        char*>(*val)); }
                    } else {
                        if      (*ft == DBR_CHAR)   { auto a = reinterpret_cast<   epicsInt8*>(*val); args["pydev"+field] = Variant(a, *ne); }
                        else if (*ft == DBR_UCHAR)  { auto a = reinterpret_cast<  epicsUInt8*>(*val); args["pydev"+field] = Variant(a, *ne); }
                        else if (*ft == DBR_SHORT)  { auto a = reinterpret_cast<  epicsInt16*>(*val); args["pydev"+field] = Variant(a, *ne); }
                        else if (*ft == DBR_USHORT) { auto a = reinterpret_cast< epicsUInt16*>(*val); args["pydev"+field] = Variant(a, *ne); }
                        else if (*ft == DBR_LONG)   { auto a = reinterpret_cast<  epicsInt32*>(*val); args["pydev"+field] = Variant(a, *ne); }
                        else if (*ft == DBR_ULONG)  { auto a = reinterpret_cast< epicsUInt32*>(*val); args["pydev"+field] = Variant(a, *ne); }
#ifdef HAVE_EPICS_INT64
                        else if (*ft == DBR_INT64)  { auto a = reinterpret_cast<  epicsInt64*>(*val); args["pydev"+field] = Variant(a, *ne); }
                        else if (*ft == DBR_UINT64) { auto a = reinterpret_cast< epicsUInt64*>(*val); args["pydev"+field] = Variant(a, *ne); }
#endif
                        else if (*ft == DBR_FLOAT)  { auto a = reinterpret_cast<epicsFloat32*>(*val); args["pydev"+field] = Variant(a, *ne); }
                        else if (*ft == DBR_DOUBLE) { auto a = reinterpret_cast<epicsFloat64*>(*val); args["pydev"+field] = Variant(a, *ne); }
                        else if (*ft == DBR_STRING) {
                            std::vector<std::string> a;
                            const char* ptr = reinterpret_cast<const char*>(*val);
                            for (size_t i=0; i<*ne; i++) {
                                std::string element(ptr, dbValueSize(DBF_STRING));
                                element.resize(element.find('\0'));
                                a.push_back(element);
                                ptr += dbValueSize(DBF_STRING);
                            }
                            args["pydev"+field] = Variant(a);
                        }
                    }
                }
            }
        }
    }

    try {
        if (ctx->code != code) {
            PyWrapper::destroy(std::move(ctx->bytecode));
            ctx->bytecode = PyWrapper::compile(code, (rec->tpro == 1));
            ctx->code = code;
        }
        auto ret = PyWrapper::eval(ctx->bytecode, args, (rec->tpro == 1));

        rec->nevl = 0;
        typedef long (*convertRoutineCast)(const void*, void*, void*);
        if (ret.type == Variant::Type::BOOL) {
            epicsInt32 val = ret.get_bool();
            auto convert = reinterpret_cast<convertRoutineCast>(dbFastPutConvertRoutine[DBF_LONG][rec->ftvl]);
            if (convert(&val, rec->val, 0) != 0) {
                throw Variant::ConvertError();
            }
            rec->nevl = 1;
        } else if (ret.type == Variant::Type::LONG) {
            epicsInt32 val = ret.get_long();
            auto convert = reinterpret_cast<convertRoutineCast>(dbFastPutConvertRoutine[DBF_LONG][rec->ftvl]);
            if (convert(&val, rec->val, 0) != 0) {
                throw Variant::ConvertError();
            }
            rec->nevl = 1;
        } else if (ret.type == Variant::Type::UNSIGNED) {
            epicsInt32 val = ret.get_unsigned();
            auto convert = reinterpret_cast<convertRoutineCast>(dbFastPutConvertRoutine[DBF_ULONG][rec->ftvl]);
            if (convert(&val, rec->val, 0) != 0) {
                throw Variant::ConvertError();
            }
            rec->nevl = 1;
        } else if (ret.type == Variant::Type::DOUBLE) {
            epicsFloat64 val = ret.get_double();
            auto convert = reinterpret_cast<convertRoutineCast>(dbFastPutConvertRoutine[DBF_DOUBLE][rec->ftvl]);
            if (convert(&val, rec->val, 0) != 0) {
                throw Variant::ConvertError();
            }
            rec->nevl = 1;
        } else if (ret.type == Variant::Type::STRING) {
            char val[MAX_STRING_SIZE];
            strncpy(val, ret.get_string().c_str(), MAX_STRING_SIZE);
            val[MAX_STRING_SIZE-1] = 0;
            auto convert = reinterpret_cast<convertRoutineCast>(dbFastPutConvertRoutine[DBF_STRING][rec->ftvl]);
            if (convert(val, rec->val, 0) != 0) {
                throw Variant::ConvertError();
            }
            rec->nevl = 1;
        } else if (ret.type == Variant::Type::VECTOR_LONG) {
            auto convert = reinterpret_cast<convertRoutineCast>(dbFastPutConvertRoutine[DBF_LONG][rec->ftvl]);
            auto values = ret.get_long_array();
            for (size_t i=0; i<values.size() && i<rec->mevl; i++) {
                char* val = reinterpret_cast<char*>(rec->val) + i*dbValueSize(rec->ftvl);
                if (convert(&values[i], val, 0) != 0) {
                    throw Variant::ConvertError();
                }
                rec->nevl++;
            }
        } else if (ret.type == Variant::Type::VECTOR_UNSIGNED) {
            auto convert = reinterpret_cast<convertRoutineCast>(dbFastPutConvertRoutine[DBF_ULONG][rec->ftvl]);
            auto values = ret.get_unsigned_array();
            for (size_t i=0; i<values.size() && i<rec->mevl; i++) {
                char* val = reinterpret_cast<char*>(rec->val) + i*dbValueSize(rec->ftvl);
                if (convert(&values[i], val, 0) != 0) {
                    throw Variant::ConvertError();
                }
                rec->nevl++;
            }
        } else if (ret.type == Variant::Type::VECTOR_DOUBLE) {
            auto convert = reinterpret_cast<convertRoutineCast>(dbFastPutConvertRoutine[DBF_DOUBLE][rec->ftvl]);
            auto values = ret.get_double_array();
            for (size_t i=0; i<values.size() && i<rec->mevl; i++) {
                char* val = reinterpret_cast<char*>(rec->val) + i*dbValueSize(rec->ftvl);
                if (convert(&values[i], val, 0) != 0) {
                    throw Variant::ConvertError();
                }
                rec->nevl++;
            }
        } else if (ret.type == Variant::Type::VECTOR_STRING) {
            auto convert = reinterpret_cast<convertRoutineCast>(dbFastPutConvertRoutine[DBF_STRING][rec->ftvl]);
            auto values = ret.get_string_array();
            for (size_t i=0; i<values.size() && i<rec->mevl; i++) {
                char* val = reinterpret_cast<char*>(rec->val) + i*dbValueSize(rec->ftvl);
                if (convert(values[i].c_str(), val, 0) != 0) {
                    throw Variant::ConvertError();
                }
                rec->nevl++;
            }
        } else {
            throw std::exception();
        }
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

static long processRecord(dbCommon *common)
{
    auto rec = reinterpret_cast<struct pycalcRecord *>(common);

    if (rec->pact == 0) {
        rec->pact = 1;
        if (fetchValues(rec) != 0) {
            recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
            rec->pact = 0;
            return S_dev_badInpType;
        }

        auto scheduled = AsyncExec::schedule([rec]() {
            processRecordCb(rec);
        });
        return (scheduled ? 0 : -1);
    }

    if (rec->ctx->processCbStatus == -1) {
        recGblSetSevr(rec, epicsAlarmCalc, epicsSevInvalid);
    }

    recGblGetTimeStamp(rec);
    dbPutLink(&rec->out, rec->ftvl, rec->val, rec->nevl);

    auto monitor_mask = recGblResetAlarms(rec);
    if (rec->ctx->processCbStatus == -1) {
        monitor_mask |= DBE_ALARM;
    } else {
        monitor_mask |= DBE_VALUE | DBE_LOG;
    }
    db_post_events(rec, rec->val, monitor_mask);

    recGblFwdLink(rec);
    rec->pact = 0;

    return 0;
}

static long fetchValues(pycalcRecord *rec)
{
    for (auto i = 0; i < PYCALCREC_NARGS; i++) {
        auto inp = &rec->inpa + i;
        auto ft  = &rec->fta  + i;
        auto val = &rec->a    + i;
        auto me  = &rec->mea  + i;
        auto ne  = &rec->nea  + i;

        if (!dbLinkIsConstant(inp) && dbIsLinkConnected(inp)) {
            long nElements;
            auto ret = dbGetNelements(inp, &nElements);
            if (ret == 0 && nElements > 0) {
                if (nElements > *me) {
                    nElements = *me;
                }
                dbGetLink(inp, *ft, *val, 0, &nElements);
                *ne = nElements;
            }
        }
    }
    return 0;
}

static long convertDbAddr(DBADDR *paddr)
{
    auto rec = reinterpret_cast<pycalcRecord *>(paddr->precord);
    int field = dbGetFieldIndex(paddr);

    if (field >= pycalcRecordA && field < (pycalcRecordA + PYCALCREC_NARGS)) {
        int off = field - pycalcRecordA;
        paddr->pfield      = *(&rec->a   + off);
        paddr->no_elements = *(&rec->nea + off);
        paddr->field_type  = *(&rec->fta + off);
    } else if (field == pycalcRecordVAL) {
        paddr->pfield      = rec->val;
        paddr->no_elements = rec->nevl;
        paddr->field_type  = rec->ftvl;
    } else {
        errlogPrintf("pycalcRecord::convertDbAddr called for %s.%s\n", rec->name, paddr->pfldDes->name);
        return 0;
    }
    paddr->dbr_field_type = paddr->field_type;
    paddr->field_size     = dbValueSize(paddr->field_type);
    return 0;
}

static long getArrayInfo(DBADDR *paddr, long *no_elements, long *offset)
{
    auto rec = reinterpret_cast<pycalcRecord *>(paddr->precord);
    int field = dbGetFieldIndex(paddr);

    if (field >= pycalcRecordA && field < (pycalcRecordA + PYCALCREC_NARGS)) {
        int off = field - pycalcRecordA;
        *no_elements = *(&rec->nea + off);
    } else if (field == pycalcRecordVAL) {
        *no_elements = rec->nevl;
    } else {
        errlogPrintf("pycalcRecord::getArrayInfo called for %s.%s\n", rec->name, paddr->pfldDes->name);
    }
    *offset = 0;

    return 0;
}
