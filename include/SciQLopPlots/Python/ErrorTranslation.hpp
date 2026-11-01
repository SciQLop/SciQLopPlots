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

//! libshiboken raises a bare OverflowError (no message) when a number doesn't fit a C++ integer
//! argument, NaN and infinity included. Give it one that says where and why.
inline void name_conversion_error(const char* function)
{
    if (!PyErr_Occurred() || !PyErr_ExceptionMatches(PyExc_OverflowError))
        return;
    PyObject *type, *value, *traceback;
    PyErr_Fetch(&type, &value, &traceback);
    // Python >= 3.12 hands back a normalized instance even for PyErr_SetObject(type, nullptr): look at its text.
    PyObject* text = value != nullptr ? PyObject_Str(value) : nullptr;
    const bool has_message = text != nullptr && PyUnicode_GetLength(text) > 0;
    Py_XDECREF(text);
    if (has_message)
    {
        PyErr_Restore(type, value, traceback);
        return;
    }
    Py_XDECREF(type);
    Py_XDECREF(value);
    Py_XDECREF(traceback);
    PyErr_Format(PyExc_OverflowError,
                 "%s: an integer argument is out of range (NaN, infinite, or too large for a C int)",
                 function);
}
}
