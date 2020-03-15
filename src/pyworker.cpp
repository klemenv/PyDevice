#include "pyworker.h"

#include <Python.h>

#include <map>
#include <stdexcept>

static PyObject* globDict = nullptr;
static PyObject* locDict = nullptr;
static std::map<std::string, std::pair<PyWorker::Callback, PyObject*>> params;

/**
 * Function for caching parameter value or notifying record of new value.
 * 
 * This function has two use-cases and are combined into a single function
 * for the simplicity to the end user:
 * 1) when Python code wants to send new parameter value to the record,
 *    it specifies two arguments, parameter name and its value.
 *    The function will cache the value and notify record that is has new
 *    value for the given parameter.
 * 2) Record(s) associated with that parameter name will then process and
 *    invoke this function with a single argument, in which case the cached
 *    parameter value is returned.
 */
static PyObject* pydev_iointr(PyObject* self, PyObject* args)
{
    PyObject* param = nullptr;
    PyObject* value = nullptr;
    if (!PyArg_UnpackTuple(args, "pydev.iointr", 1, 2, &param, &value)) {
        Py_RETURN_FALSE;
    }

    std::string name = PyString_AsString(param);
    Py_DecRef(param);

    if (value) {
        try {
            params.at(name).second = value;
            params.at(name).first();
        } catch (...) {
            // pass
        }
        Py_RETURN_TRUE;
    }

    try {
        value = params.at(name).second;
        if (value) {
            return value;
        }
    } catch (...) {
        // pass
    }
    Py_RETURN_NONE;
}

static struct PyMethodDef methods[] = {
    { "iointr", pydev_iointr, METH_VARARGS, "PyDevice interface for parameters exchange"},
    { NULL, NULL, 0, NULL }
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moddef = {
    PyModuleDef_HEAD_INIT, "pydev", NULL, -1, methods, NULL, NULL, NULL, NULL
};
#endif

static PyObject* PyInit_pydev(void)
{
#if PY_MAJOR_VERSION >= 3
    return PyModule_Create(&moddef);
#else
    return Py_InitModule("pydev", methods);
#endif
}

bool PyWorker::init()
{
    // Initialize and register `pydev' Python module which serves as
    // communication channel for I/O Intr value exchange
    PyImport_AppendInittab("pydev", &PyInit_pydev);

    Py_Initialize();
    PyEval_InitThreads();

    globDict = PyDict_New();
    locDict = PyDict_New();
    PyDict_SetItemString(globDict, "__builtins__", PyEval_GetBuiltins());

    PyEval_ReleaseLock();

    // Make `pydev' module appear as built-in module
    exec("import pydev", true);

    return true;
}

void PyWorker::shutdown()
{
    Py_DecRef(globDict);
    Py_DecRef(locDict);
    Py_Finalize();
}

void PyWorker::registerIoIntr(const std::string& name, const Callback& cb)
{
    params[name].first = cb;
    params[name].second = nullptr;
}

void PyWorker::exec(const std::string& line, bool debug)
{
    PyObject* r = PyRun_String(line.c_str(), Py_file_input, globDict, locDict);
    if (r == nullptr) {
        if (debug && PyErr_Occurred()) {
            PyErr_Print();
        }
        PyErr_Clear();
        throw std::runtime_error("Failed to execute Python code");
    }
    Py_DecRef(r);
}

bool PyWorker::exec(const std::string& line, bool debug, int32_t* val)
{
    if (val != nullptr) {
        PyObject* r = PyRun_String(line.c_str(), Py_eval_input, globDict, locDict);
        if (r != nullptr) {
            long value = PyInt_AsLong(r);
            Py_DecRef(r);

            if (value == -1 && PyErr_Occurred()) {
                if (debug) {
                    PyErr_Print();
                }
                PyErr_Clear();
                return false;
            }
            *val = value;
            return true;
        }
        PyErr_Clear();
    }

    // Either *val == nullptr or eval failed, let's try executing code instead
    PyObject* r = PyRun_String(line.c_str(), Py_single_input, globDict, locDict);
    if (r == nullptr) {
        if (debug && PyErr_Occurred()) {
            PyErr_Print();
        }
        PyErr_Clear();
        throw std::runtime_error("Failed to execute Python code");
    }
    Py_DecRef(r);
    return false;
}
