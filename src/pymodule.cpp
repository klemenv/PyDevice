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

template <typename T>
static PyObject *convertLiteral(T val)
{
    return PyInt_FromLong(val);
}

template <>
PyObject *convertLiteral(double val)
{
    return PyFloat_FromDouble(val);
}

template <>
PyObject *convertLiteral(char* val)
{
    return PyString_FromString(val);
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
    epicsThreadCreate(threadName.c_str(), epicsThreadPriorityLow, epicsThreadStackMedium, (EPICSTHREADFUNC)&ThreadC, this);
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

template <typename T>
void PyModule::schedule(const std::string& func, T arg, Callback &cb)
{
    auto funcPy = PyObject_GetAttrString(m_module, func.c_str());
    if (!funcPy)
    {
        throw std::invalid_argument("No such function!");
    }
    if (!PyCallable_Check(funcPy))
    {
        Py_XDECREF(funcPy);
        throw std::invalid_argument("Python object not a function!");
    }

    auto argPy = convertLiteral<T>(arg);
    if (!argPy)
    {
        Py_XDECREF(funcPy);
        throw std::invalid_argument("Python object not a function!");
    }
    auto argsPy = PyTuple_Pack(1, argPy);

    auto task = std::bind(&PyModule::exec, this, funcPy, argsPy, cb);

    m_mutex.lock();
    m_queue.emplace_back(task);
    m_mutex.unlock();
    m_event.signal();
}

// Instantiate schedule() for popular EPICS DB types
template void PyModule::schedule(const std::string&, epicsFloat64, Callback&);
template void PyModule::schedule(const std::string&, epicsInt32,   Callback &);
template void PyModule::schedule(const std::string&, epicsUInt16,  Callback &);
template void PyModule::schedule(const std::string&, char*,        Callback &);

void PyModule::run()
{
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

        task();
    }

    m_stopped.signal();
}

void PyModule::exec(PyObject* func, PyObject* args, Callback& cb)
{
    bool success;
    std::stringstream error;
    auto ret = PyObject_CallObject(func, args);
    Py_DECREF(args);
    if (ret == nullptr)
    {
        success = false;
        Py_DECREF(func);

        error << "Function failed:" << std::endl;

        PyObject *excType, *excValue, *excTrace;
        PyErr_Fetch(&excType, &excValue, &excTrace);
        if (excType != NULL)
        {
            PyObject *str = PyObject_Repr(excType);
            error << " - Exception type: " << PyString_AsString(str) << std::endl;
            Py_DecRef(str);
            Py_DecRef(excType);
        }
        if (excValue != NULL)
        {
            PyObject *str = PyObject_Repr(excValue);
            error << " - Exception value: " << PyString_AsString(str) << std::endl;
            Py_DecRef(str);
            Py_DecRef(excValue);
        }
        if (excTrace != NULL)
        {
            // TODO: Found this post but it's using Python internals and doesn't compile
            //       https://stackoverflow.com/a/43984515
            Py_DecRef(excTrace);
        }
    }
    else
    {
        success = true;
        Py_DECREF(ret);
    }
    cb(success, error.str());
}

PyObject* PyModule::resolveFunction(const std::string& name)
{
    auto func = PyObject_GetAttrString(m_module, name.c_str());
    if (!PyCallable_Check(func))
    {
        Py_XDECREF(func);
        func = nullptr;
    }

    return func;
}
