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
        using Task = std::function<void(PyModule &)>;

    private:
        bool m_processing{false};
        std::list<Task> m_queue;
        epicsMutex m_mutex;
        epicsEvent m_event;
        epicsEvent m_stopped;
        PyObject* m_module{nullptr};

    public:
        PyModule(const std::string& id);
        PyModule(const PyModule &other) = delete;
        PyModule &operator=(const PyModule &other) = delete;

        ~PyModule();

        void schedule(Task& cb);

        /**
         * Main thread function, should not be called directly.
         *
         * It needs to be public due to C linkage.
         */
        void run();

        template <typename T>
        void exec(const std::string& func, T arg);

        template <typename T>
        T exec(const std::string& func);

    private:
        template <typename R, typename A>
        R exec(const std::string& func, A arg);
};
