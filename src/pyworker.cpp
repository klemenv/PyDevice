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
    PyObject* param;
    PyObject* value = nullptr;
    if (!PyArg_UnpackTuple(args, "pydev.iointr", 1, 2, &param, &value)) {
        PyErr_Clear();
        Py_RETURN_FALSE;
    }
    std::string name = PyString_AsString(param);

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
    { NULL, NULL, 0, NULL }
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef moddef = {
    PyModuleDef_HEAD_INIT, "pydev", NULL, -1, methods, NULL, NULL, NULL, NULL
};
#endif

#if PY_MAJOR_VERSION >= 3
static PyObject* PyInit_pydev(void)
{
    return PyModule_Create(&moddef);
}
#else
static void PyInit_pydev(void)
{
printf("PyInit_pydev\n");
    Py_InitModule("pydev", methods);
}
#endif

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

template <typename T>
bool PyWorker::exec(const std::string& line, bool debug, T* val)
{
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
            PyErr_Print();
        }
        PyErr_Clear();
        throw std::runtime_error("Failed to execute Python code");
    }
    Py_DecRef(r);
    return false;
}
template bool PyWorker::exec(const std::string&, bool, int32_t*);
template bool PyWorker::exec(const std::string&, bool, uint16_t*);
template bool PyWorker::exec(const std::string&, bool, double*);

bool PyWorker::exec(const std::string& line, bool debug, std::string& val)
{
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

bool PyWorker::convert(void* in_, int32_t& out)
{
    PyObject* in = reinterpret_cast<PyObject*>(in_);
    if (PyInt_Check(in)) {
        out = PyInt_AsLong(in);
        if (out == -1 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        return true;
    }
    if (PyLong_Check(in)) {
        out = PyLong_AsLong(in);
        if (out == -1 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        return true;
    }
    if (PyBool_Check(in)) {
        out = (PyObject_IsTrue(in) ? 1 : 0);
        return true;
    }
    if (PyFloat_Check(in)) {
        double o = PyFloat_AsDouble(in);
        out = o;
        if (o == -1.0 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        return true;
    }
    return false;
}

bool PyWorker::convert(void* in_, uint16_t& out)
{
    PyObject* in = reinterpret_cast<PyObject*>(in_);
    if (PyInt_Check(in)) {
        long o = PyInt_AsLong(in);
        out = o;
        if (o == -1 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        return true;
    }
    if (PyLong_Check(in)) {
        long o = PyLong_AsLong(in);
        out = o;
        if (o == -1 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        return true;
    }
    if (PyBool_Check(in)) {
        out = (PyObject_IsTrue(in) ? 1 : 0);
        return true;
    }
    if (PyFloat_Check(in)) {
        double o = PyFloat_AsDouble(in);
        out = o;
        if (o == -1.0 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        return true;
    }
    return false;
}

bool PyWorker::convert(void* in_, double& out)
{
    PyObject* in = reinterpret_cast<PyObject*>(in_);
    if (PyInt_Check(in)) {
        out = PyInt_AsLong(in);
        if (out == -1.0 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        return true;
    }
    if (PyLong_Check(in)) {
        out = PyLong_AsLong(in);
        if (out == -1.0 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        return true;
    }
    if (PyBool_Check(in)) {
        out = (PyObject_IsTrue(in) ? 1.0 : 0.0);
        return true;
    }
    if (PyFloat_Check(in)) {
        out = PyFloat_AsDouble(in);
        if (out == -1.0 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        return true;
    }
    return false;
}

bool PyWorker::convert(void* in_, std::string& out)
{
    PyObject* in = reinterpret_cast<PyObject*>(in_);
    if (PyString_Check(in)) {
        const char* o = PyString_AsString(in);
        if (o == nullptr && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        out = o;
        return true;
    }
    if (PyInt_Check(in)) {
        long o = PyInt_AsLong(in);
        if (o == -1 && PyErr_Occurred()) {
            PyErr_Clear();
            return false;
        }
        out = std::to_string(o);
        return true;
    }
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
