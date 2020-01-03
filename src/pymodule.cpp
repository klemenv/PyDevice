/* pywrapper.cpp
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Feb 2019
 */

#include <epicsThread.h>
#include <epicsTypes.h>

#include <atomic>
#include <sstream>

#include "pymodule.h"

static std::atomic_bool python_initialized{false};

extern "C"
{
    static void ThreadC(void *ctx)
    {
        reinterpret_cast<PyModule *>(ctx)->run();
    }
};

static void decodePyError(std::string &type, std::string &text, std::string &trace)
{
    PyObject *excType, *excValue, *excTrace;
    PyErr_Fetch(&excType, &excValue, &excTrace);
    if (excType != NULL)
    {
        PyObject *str = PyObject_Repr(excType);
        type = PyString_AsString(str);
        Py_DecRef(str);
        Py_DecRef(excType);
    }
    if (excValue != NULL)
    {
        PyObject *str = PyObject_Repr(excValue);
        text = PyString_AsString(str);
        Py_DecRef(str);
        Py_DecRef(excValue);
    }
    if (excTrace != NULL)
    {
        trace = "pydevice: not yet implemented";
        // TODO: Found this post but it's using Python internals and doesn't compile
        //       https://stackoverflow.com/a/43984515
        Py_DecRef(excTrace);
    }
}

template <typename T>
static PyObject *convertToNative(T val)
{
    PyErr_Clear();
    auto ret = PyInt_FromLong(val);
    if (PyErr_Occurred())
    {
        std::string type, text, trace;
        decodePyError(type, text, trace);
        throw std::runtime_error("Can not convert argument from 'long' type: " + text);
    }
    return ret;
}

template <>
PyObject *convertToNative(double val)
{
    PyErr_Clear();
    auto ret = PyFloat_FromDouble(val);
    if (PyErr_Occurred())
    {
        std::string type, text, trace;
        decodePyError(type, text, trace);
        throw std::runtime_error("Can not convert argument from 'double' type: " + text);
    }
    return ret;
}

template <>
PyObject *convertToNative(char *val)
{
    PyErr_Clear();
    auto ret = PyString_FromString(val);
    if (PyErr_Occurred())
    {
        std::string type, text, trace;
        decodePyError(type, text, trace);
        throw std::runtime_error("Can not convert argument from 'char*' type: " + text);
    }
    return ret;
}

template <typename T>
T convertFromNative(PyObject* val)
{
    PyErr_Clear();
    T ret = PyInt_AsLong(val);
    if (PyErr_Occurred())
    {
        std::string type, text, trace;
        decodePyError(type, text, trace);
        throw std::runtime_error("Can not convert return code to 'long' type: " + text);
    }
    return ret;
}

template <>
bool convertFromNative(PyObject *val)
{
    return false;
}

template <>
double convertFromNative(PyObject *val)
{
    PyErr_Clear();
    double ret = PyFloat_AsDouble(val);
    if (PyErr_Occurred())
    {
        std::string type, text, trace;
        decodePyError(type, text, trace);
        throw std::runtime_error("Can not convert return code to 'double' type: " + text);
    }
    return ret;
}

template <>
char* convertFromNative(PyObject* val)
{
    PyErr_Clear();
    char* ret = PyString_AsString(val);
    if (PyErr_Occurred())
    {
        std::string type, text, trace;
        decodePyError(type, text, trace);
        throw std::runtime_error("Can not convert return code to 'char*' type: " + text);
    }
    return ret;
}

PyModule::PyModule(const std::string &id)
{
    if (!python_initialized)
    {
        Py_Initialize();
        python_initialized = true;
    }

    auto name = PyUnicode_FromString(id.c_str());
    if (!name)
    {
        throw std::invalid_argument("Invalid Python module name");
    }
    auto module = PyImport_Import(name);
    Py_DECREF(name);
    if (!module)
    {
        PyErr_Print(); // Print a bit of extra information to the console
        throw std::invalid_argument("Failed to import Python module");
    }
    m_module = module;

    std::string threadName = "thread_" + id;
    epicsThreadCreate(threadName.c_str(),
                      epicsThreadPriorityLow,
                      epicsThreadGetStackSize(epicsThreadStackMedium),
                      (EPICSTHREADFUNC)&ThreadC, this);
}

PyModule::~PyModule()
{
    if (m_processing)
    {
        m_processing = false;
        m_event.signal();
        m_stopped.wait();
    }
}

void PyModule::schedule(Task& task)
{
    m_mutex.lock();
    m_queue.emplace_back(task);
    m_mutex.unlock();
    m_event.signal();
}

void PyModule::run()
{
    m_processing = true;
    while (m_processing)
    {
        m_mutex.lock();

        if (m_queue.empty())
        {
            m_mutex.unlock();
            m_event.wait();
            continue;
        }

        auto task = std::move(m_queue.front());
        m_queue.pop_front();
        m_mutex.unlock();

        task(*this);
    }

    m_stopped.signal();
}

template <typename R, typename A>
R PyModule::exec(const std::string& func, A arg)
{
    auto funcPy = PyObject_GetAttrString(m_module, func.c_str());
    if (!funcPy)
    {
        throw std::invalid_argument("No such function!");
    }
    if (!PyCallable_Check(funcPy))
    {
        Py_DECREF(funcPy);
        throw std::invalid_argument("Python object not a function!");
    }

    PyObject* argsPy;
    if (std::is_same<A, bool>::value)
    {
        argsPy = PyTuple_New(0);
    }
    else
    {
        PyObject *argPy;
        try
        {
            argPy = convertToNative<A>(arg);
        }
        catch (...)
        {
            Py_DECREF(funcPy);
            throw;
        }
        argsPy = PyTuple_Pack(1, argPy);
        Py_DECREF(argPy);
    }
    if (!argsPy)
    {
        Py_DECREF(funcPy);
        throw std::invalid_argument("Failed to initialize Python arguments object!");
    }

    auto ret = PyObject_CallObject(funcPy, argsPy);
    Py_DECREF(argsPy);

    if (ret != nullptr)
    {
        // Success
        R r = convertFromNative<R>(ret);
        Py_DECREF(ret);
        return r;
    }
    else
    {
        Py_DECREF(funcPy);

        std::string type;
        std::string text;
        std::string trace;
        decodePyError(type, text, trace);

        std::string error;
        error += "Function threw exception:\n";
        error += " - Exception type: " + type + "\n";
        error += " - Exception value: " + text + "\n";
        throw std::runtime_error(error);
    }
}

template <typename T>
T PyModule::exec(const std::string& func)
{
    return exec<T, bool>(func, false);
}

template <typename T>
void PyModule::exec(const std::string& func, T arg)
{
    (void)exec<bool, T>(func, arg);
}

// Instantiate schedule() for popular EPICS DB types
template void PyModule::exec(const std::string&, epicsFloat64);
template void PyModule::exec(const std::string&, epicsInt32);
template void PyModule::exec(const std::string&, epicsUInt16);
template void PyModule::exec(const std::string&, char*);

template epicsFloat64 PyModule::exec(const std::string&);
template epicsInt32   PyModule::exec(const std::string &);
template epicsUInt16  PyModule::exec(const std::string &);
template char*        PyModule::exec(const std::string &);
