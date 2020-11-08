/*************************************************************************\
* PyDevice is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/

#include "pywrapper.h"

extern "C" {
#include <Python.h>
};
#include <map>
#include <stdexcept>
#include <iostream>

static PyObject* globDict = nullptr;
static PyObject* locDict = nullptr;
static PyThreadState* mainThread = nullptr;
static std::map<std::string, std::pair<PyWrapper::Callback, PyObject*>> params;

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
    PyObject* param;
    PyObject* value = nullptr;
    if (!PyArg_UnpackTuple(args, "pydev.iointr", 1, 2, &param, &value)) {
        PyErr_Clear();
        Py_RETURN_FALSE;
    }
#if PY_MAJOR_VERSION < 3
    if (!PyString_Check(param)) {
        PyErr_SetString(PyExc_TypeError, "Parameter name is not a string");
        Py_RETURN_NONE;
    }
    std::string name = PyString_AsString(param);
#else
    PyObject* tmp = nullptr;
    if (!PyUnicode_Check(param) || ((tmp=PyUnicode_AsASCIIString(param)) == nullptr)) {
        PyErr_SetString(PyExc_TypeError, "Parameter name is not a string");
        Py_RETURN_NONE;
    }
    std::string name = PyByteArray_AsString(tmp);
    Py_XDECREF(tmp);
#endif

    auto it = params.find(name);
    if (value) {
        if (it != params.end()) {
            if (it->second.second) {
                Py_DecRef(it->second.second);
            }
            Py_IncRef(value);
            it->second.second = value;

            it->second.first();
        }
        Py_RETURN_TRUE;
    }

    if (it != params.end() && it->second.second != nullptr) {
        Py_IncRef(it->second.second);
        return it->second.second;
    }
    Py_RETURN_NONE;
}

static struct PyMethodDef methods[] = {
    { "iointr", pydev_iointr, METH_VARARGS, "PyDevice interface for parameters exchange"},
    /* sentinel */
    { NULL, NULL, 0, NULL }
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moddef = {
    PyModuleDef_HEAD_INIT, "pydev", NULL, -1, methods, NULL, NULL, NULL, NULL
};
#endif

#if PY_MAJOR_VERSION == 2
#define PYTHON_HAS_INT
#elif PY_MAJOR_VERSION == 3
/* has only longs */
#else
#warning "You are using unknown python version"
// #define PyInt_Check(n) PyLong_Check(n)
// #define PyInt_AsLong(n) PyLong_AsLong(n)
#endif


#if PY_MAJOR_VERSION >= 3
static PyObject* PyInit_pydev(void)
{
    return PyModule_Create(&moddef);
}
#else
static void PyInit_pydev(void)
{
    Py_InitModule("pydev", methods);
}
#endif

bool PyWrapper::init()
{
    // Initialize and register `pydev' Python module which serves as
    // communication channel for I/O Intr value exchange
    PyImport_AppendInittab("pydev", &PyInit_pydev);

    // Py_SetProgramName(L"python3.7");
    std::cerr << "Initalising python3" << std::endl;
    Py_Initialize();
    std::cerr << "Initialsing threads" << std::endl;
    PyEval_InitThreads();

    std::cerr << "Initialsing dics" << std::endl;

#if PY_MAJOR_VERSION == 3

    /*
     * IN Python 3 the
     */
    auto m = PyImport_AddModule("__main__");
    assert(m);
    globDict = PyModule_GetDict(m);
    assert(globDict);
    locDict = PyDict_New();
    assert(locDict);
    /*     */
#elif PY_MAJOR_VERSION == 2
    globDict = PyDict_New();
    locDict = PyDict_New();
    PyDict_SetItemString(globDict, "__builtins__", PyEval_GetBuiltins());
#else /* PY_MAJOR_VERSION */
#error "Unsupported python version"
#endif /* PY_MAJOR_VERSION */

    mainThread = PyEval_SaveThread();

    // Make `pydev' module appear as built-in module
    std::cerr << "Importing pydev" << std::endl;
    exec("import pydev", true);
    std::cerr << "Importing builtins" << std::endl;

#if PY_MAJOR_VERSION == 3
    exec("import builtins", true);
    exec("builtins.pydev=pydev", true);
    exec("import pydev", true);
#elif PY_MAJOR_VERSION == 2
    exec("import __builtin__", true);
    exec("__builtin__.pydev=pydev", true);
#else
#error "Unsupported python version"
#endif

    std::cerr << "python imported" << std::endl;

    return true;
}

void PyWrapper::shutdown()
{
    // PyEval_AcquireLock();
    PyEval_AcquireThread(mainThread);
    PyThreadState_Clear(mainThread);
    PyThreadState_Delete(mainThread);

    // The following will segfault IOC if there are Python-created threads
//    Py_DecRef(globDict);
//    Py_DecRef(locDict);
//    Py_Finalize();
}

void PyWrapper::registerIoIntr(const std::string& name, const Callback& cb)
{
    params[name].first = cb;
    params[name].second = nullptr;
}

struct PyGIL {
    PyGIL() {
        PyEval_RestoreThread(mainThread);
    }
    ~PyGIL() {
        PyEval_SaveThread();
    }
};

template <typename T>
bool PyWrapper::convert(void* in_, T& out)
{
    PyObject* in = reinterpret_cast<PyObject*>(in_);

#ifdef PYTHON_HAS_INT
    if (PyInt_Check(in)) {
        long o = PyInt_AsLong(in);
        if (o == -1 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        out = o;
        return true;
    }
#endif
    if (PyLong_Check(in)) {
        long o = PyLong_AsLong(in);
        if (o == -1 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        out = o;
        return true;
    }
    if (PyBool_Check(in)) {
        out = (PyObject_IsTrue(in) ? 1 : 0);
        return true;
    }
    if (PyFloat_Check(in)) {
        double o = PyFloat_AsDouble(in);
        if (o == -1.0 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        out = o;
        return true;
    }
    return false;
}
template bool PyWrapper::convert(void* in_, int32_t& out);
template bool PyWrapper::convert(void* in_, uint16_t& out);
template bool PyWrapper::convert(void* in_, uint32_t& out);
template bool PyWrapper::convert(void* in_, double& out);

template <>
bool PyWrapper::convert(void* in_, std::string& out)
{
    PyObject* in = reinterpret_cast<PyObject*>(in_);
#if PY_MAJOR_VERSION < 3
    if (PyString_Check(in)) {
        const char* o = PyString_AsString(in);
        if (o == nullptr && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        out = o;
        return true;
    }
#endif
    if (PyUnicode_Check(in)) {
        PyObject* tmp = PyUnicode_AsASCIIString(in);
        if (tmp == nullptr) {
            PyErr_Clear();
            return false;
        }
        const char* o = PyBytes_AsString(tmp);
        if (o == nullptr && PyErr_Occurred()) {
            Py_XDECREF(tmp);
            PyErr_Clear();
            return false;
        }
        out = o;
        Py_XDECREF(tmp);
        return true;
    }
#ifdef PYTHON_HAS_INT
    if (PyInt_Check(in)) {
        long o = PyInt_AsLong(in);
        if (o == -1 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        out = std::to_string(o);
        return true;
    }
#endif
    if (PyLong_Check(in)) {
        long o = PyLong_AsLong(in);
        if (o == -1 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        out = std::to_string(o);
        return true;
    }
    if (PyBool_Check(in)) {
        out = (PyObject_IsTrue(in) ? "true" : "false");
        return true;
    }
    if (PyFloat_Check(in)) {
        double o = PyFloat_AsDouble(in);
        if (o == -1.0 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        out = std::to_string(o);
        return true;
    }
    return false;
}

template <typename T>
bool PyWrapper::convert(void* in_, std::vector<T>& out)
{
    PyObject* in = reinterpret_cast<PyObject*>(in_);
    if (!PyList_Check(in)) {
        return false;
    }

    out.clear();
    for (Py_ssize_t i = 0; i < PyList_Size(in); i++) {
        PyObject* el = PyList_GetItem(in, i);

#ifdef PYTHON_HAS_INT
        if (PyInt_Check(el)) {
            T elval = PyInt_AsLong(el);
            if (elval == -1.0 && PyErr_Occurred()) {
                PyErr_Clear();
                return false;
            }
            out.push_back(elval);
        }
#endif
        if (PyLong_Check(el)) {
            T elval = PyLong_AsLong(el);
            if (elval == -1.0 && PyErr_Occurred()) {
                PyErr_Clear();
                return false;
            }
            out.push_back(elval);
        }
        if (PyBool_Check(el)) {
            T elval = (PyObject_IsTrue(el) ? 1 : 0);
            out.push_back(elval);
        }
        if (PyFloat_Check(el)) {
            T elval = PyFloat_AsDouble(el);
            if (elval == -1.0 && PyErr_Occurred()) {
                PyErr_Clear();
                return false;
            }
            out.push_back(elval);
        }
    }
    return true;
}
template bool PyWrapper::convert(void* in_, std::vector<long>& out);
template bool PyWrapper::convert(void* in_, std::vector<double>& out);

void PyWrapper::exec(const std::string& line, bool debug)
{
    PyGIL gil;

    PyObject* r = PyRun_String(line.c_str(), Py_file_input, globDict, locDict);
    if (r == nullptr) {
        if (debug && PyErr_Occurred()) {
            std::cerr << "Tried to execute string '" << line << "'" << std::endl;
            PyErr_Print();
        }
        PyErr_Clear();
        throw std::runtime_error("Failed to execute Python code");
    }
    Py_DecRef(r);
}

template <typename T>
bool PyWrapper::exec(const std::string& line, bool debug, T* val)
{
    PyGIL gil;
    if (val != nullptr) {
        PyObject* r = PyRun_String(line.c_str(), Py_eval_input, globDict, locDict);
        if (r != nullptr) {
            T value;
            bool converted = convert(r, value);
            Py_DecRef(r);

            if (!converted) {
                PyErr_Print();
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
        if (PyErr_Occurred()) {
            std::cerr << "Tried to execute string '" << line << "'" << std::endl;

            PyErr_Print();
        }
        PyErr_Clear();
        throw std::runtime_error("Failed to execute Python code");
    }
    Py_DecRef(r);
    return false;
}
template bool PyWrapper::exec(const std::string&, bool, int32_t*);
template bool PyWrapper::exec(const std::string&, bool, uint16_t*);
template bool PyWrapper::exec(const std::string&, bool, unsigned int*);
template bool PyWrapper::exec(const std::string&, bool, double*);

bool PyWrapper::exec(const std::string& line, bool debug, std::string& val)
{
    PyGIL gil;
    PyObject* r = PyRun_String(line.c_str(), Py_eval_input, globDict, locDict);
    if (r != nullptr) {
        std::string value;
        bool converted = convert(r, value);
        Py_DecRef(r);

        if (!converted) {
            PyErr_Print();
            PyErr_Clear();
            return false;
        }
        val = value;
        return true;
    }
    PyErr_Clear();

    // Still here, let's try executing code instead
    r = PyRun_String(line.c_str(), Py_single_input, globDict, locDict);
    if (r == nullptr) {
        if (PyErr_Occurred()) {
            PyErr_Print();
        }
        PyErr_Clear();
        throw std::runtime_error("Failed to execute Python code");
    }
    Py_DecRef(r);
    return false;
}

template <typename T>
bool PyWrapper::exec(const std::string& line, bool debug, std::vector<T>& arr)
{
    PyGIL gil;
    arr.clear();

    PyObject* r = PyRun_String(line.c_str(), Py_eval_input, globDict, locDict);
    if (r != nullptr) {
        bool converted = convert(r, arr);
        Py_DecRef(r);

        if (!converted) {
            PyErr_Print();
            PyErr_Clear();
            return false;
        }
        return true;
    }
    PyErr_Clear();

    // Still here, let's try executing code instead
    r = PyRun_String(line.c_str(), Py_single_input, globDict, locDict);
    if (r == nullptr) {
        if (PyErr_Occurred()) {
            PyErr_Print();
        }
        PyErr_Clear();
        throw std::runtime_error("Failed to execute Python code");
    }
    Py_DecRef(r);
    return false;
}
template bool PyWrapper::exec(const std::string&, bool, std::vector<long>&);
template bool PyWrapper::exec(const std::string&, bool, std::vector<double>&);
