/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include <epicsExit.h>
#include <epicsExport.h>
#include <iocsh.h>

#include "asyncexec.h"
#include "pywrapper.h"
#include "util.h"

extern "C"
{

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
        auto numThreads = Util::getEnvConfig("PYDEV_NUM_THREADS", 3);
        if (numThreads < 1)
            numThreads = 3;

        PyWrapper::init();
        AsyncExec::init(numThreads);
        iocshRegister(&pydevDef, pydevCall);
        epicsAtExit(pydevUnregister, 0);
    }
}
epicsExportRegistrar(pydevRegister);

}; // extern "C"
