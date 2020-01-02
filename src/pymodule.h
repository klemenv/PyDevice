/* provider.h
 *
 * Copyright (c) 2018 Oak Ridge National Laboratory.
 * All rights reserved.
 * See file LICENSE that is included with this distribution.
 *
 * @author Klemen Vodopivec
 * @date Feb 2019
 */

#pragma once

#include <epicsEvent.h>
#include <epicsMutex.h>

#include <Python.h>

#include <functional>
#include <list>
#include <string>

class PyModule
{
    public:
        using Callback = std::function<void(bool, std::string)>;
        using Task = std::function<void(void)>;

    private:
        bool m_processing{true};
        std::list<Task> m_queue;
        epicsMutex m_mutex;
        epicsEvent m_event;
        epicsEvent m_stopped;
        PyObject* m_module{nullptr};

    public:
        PyModule(const std::string& id);
        ~PyModule();

        template <typename T>
        void schedule(const std::string& func, T arg, Callback& cb);
        void run();

        PyObject* resolveFunction(const std::string& name);

    private:
        void exec(PyObject *func, PyObject *args, Callback &cb);
};