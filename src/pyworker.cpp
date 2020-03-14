#include "pyworker.h"

#include <Python.h>

#include <stdexcept>

static PyObject* globDict = nullptr;
static PyObject* locDict = nullptr;

bool PyWorker::init()
{
    Py_Initialize();
    PyEval_InitThreads();

    globDict = PyDict_New();
    locDict = PyDict_New();
    PyDict_SetItemString(globDict, "__builtins__", PyEval_GetBuiltins());

    PyEval_ReleaseLock();
    return true;
}

void PyWorker::shutdown()
{
    Py_DecRef(globDict);
    Py_DecRef(locDict);
    Py_Finalize();
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
