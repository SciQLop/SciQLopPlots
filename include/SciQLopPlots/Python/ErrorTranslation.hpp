#pragma once

#include <SciQLopPlots/Debug.hpp>

// shiboken's Python.h wrapper: Python.h after Qt headers breaks on the "slots" macro.
#include <sbkpython.h>

#include <new>
#include <stdexcept>

namespace sqp
{
//! The Python exception class for a C++ exception escaping a wrapped call. Shiboken's
//! exception-handling hard-codes RuntimeError; shiboken-gen.py rewrites its catch blocks to call this.
inline PyObject* python_error_type(const std::exception& e)
{
    if (dynamic_cast<const unsupported_call*>(&e))
        return PyExc_NotImplementedError;
    if (dynamic_cast<const type_error*>(&e))
        return PyExc_TypeError;
    if (dynamic_cast<const std::invalid_argument*>(&e))
        return PyExc_ValueError;
    if (dynamic_cast<const std::out_of_range*>(&e))
        return PyExc_IndexError;
    if (dynamic_cast<const std::bad_alloc*>(&e))
        return PyExc_MemoryError;
    return PyExc_RuntimeError;
}
}
