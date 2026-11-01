#pragma once

#include <SciQLopPlots/Debug.hpp>

// shiboken's Python.h wrapper: Python.h after Qt headers breaks on the "slots" macro.
#include <sbkpython.h>

#include <QColor>
#include <QString>

#include <new>
#include <stdexcept>
#include <string>
#include <string_view>

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

//! A numpy array of complex numbers: its elements convert to double only by dropping the imaginary
//! part (numpy warns and goes on), so a list-of-numbers argument refuses it as the wrong type.
inline bool is_complex_array(PyObject* object)
{
    PyObject* dtype = PyObject_GetAttrString(object, "dtype");
    if (dtype == nullptr)
    {
        PyErr_Clear();
        return false;
    }
    PyObject* kind = PyObject_GetAttrString(dtype, "kind");
    Py_DECREF(dtype);
    if (kind == nullptr)
    {
        PyErr_Clear();
        return false;
    }
    const bool complex = PyUnicode_Check(kind) && PyUnicode_CompareWithASCIIString(kind, "c") == 0;
    Py_DECREF(kind);
    return complex;
}

//! A colour argument given as a str: PySide turns a name it can't parse into an invalid QColor, which
//! several setters read as "automatic". Raise instead. A QColor() passed or defaulted is not a str.
inline void check_color_arg(PyObject* arg, const char* function)
{
    if (arg == nullptr || PyErr_Occurred() || !PyUnicode_Check(arg))
        return;
    Py_ssize_t size = 0;
    const char* text = PyUnicode_AsUTF8AndSize(arg, &size);
    if (text != nullptr && !QColor::isValidColorName(QString::fromUtf8(text, size)))
        PyErr_Format(PyExc_ValueError, "%s: '%s' is not a colour name", function, text);
}

//! A font argument given as a list of family names: PySide prints an error for each non-str item and
//! goes on. Raise instead.
inline void check_font_arg(PyObject* arg, const char* function)
{
    if (arg == nullptr || PyErr_Occurred() || PyUnicode_Check(arg) || !(PyList_Check(arg) || PyTuple_Check(arg)))
        return;
    const Py_ssize_t size = PySequence_Size(arg);
    for (Py_ssize_t i = 0; i < size; ++i)
    {
        PyObject* item = PySequence_GetItem(arg, i);
        const bool is_str = item != nullptr && PyUnicode_Check(item);
        Py_XDECREF(item);
        if (!is_str)
        {
            PyErr_Format(PyExc_TypeError, "%s: font family names must be strs", function);
            return;
        }
    }
}

//! "bins=str, name=int": each keyword argument with its type's name.
inline std::string keyword_types(PyObject* kwds)
{
    std::string listed;
    PyObject *key, *value;
    Py_ssize_t pos = 0;
    while (PyDict_Next(kwds, &pos, &key, &value))
    {
        PyObject* type_name = PyType_GetName(Py_TYPE(value));
        const char* k = PyUnicode_AsUTF8AndSize(key, nullptr);
        const char* t = type_name != nullptr ? PyUnicode_AsUTF8AndSize(type_name, nullptr) : nullptr;
        listed += (listed.empty() ? "" : ", ") + std::string(k ? k : "?") + "=" + (t ? t : "?");
        Py_XDECREF(type_name);
    }
    PyErr_Clear(); // a failed name lookup above must not replace the error being reported
    return listed;
}

//! Two shiboken argument errors that don't say why, made into TypeErrors that do. \a result is what
//! returnWrongArguments returned, \a kwds the call's keyword arguments.
//! - An unknown keyword is an AttributeError ("unsupported keyword"); Python says TypeError.
//! - A keyword of the wrong type says "f((args,)) is wrong (missing signature)": the positional
//!   arguments matched, so the culprit is a keyword. Name them with their types.
inline PyObject* keyword_error_as_type_error(PyObject* result, PyObject* kwds = nullptr)
{
    const bool attribute = PyErr_Occurred() && PyErr_ExceptionMatches(PyExc_AttributeError);
    const bool type = PyErr_Occurred() && PyErr_ExceptionMatches(PyExc_TypeError);
    if (!attribute && !type)
        return result;
    PyObject *exc_type, *value, *traceback;
    PyErr_Fetch(&exc_type, &value, &traceback);
    PyObject* text = value != nullptr ? PyObject_Str(value) : nullptr;
    const char* raw = text != nullptr ? PyUnicode_AsUTF8AndSize(text, nullptr) : nullptr;
    const std::string_view message = raw != nullptr ? raw : "";
    const bool has_keywords = kwds != nullptr && PyDict_Check(kwds) && PyDict_Size(kwds) > 0;
    if (attribute && message.find("unsupported keyword") != std::string_view::npos)
        PyErr_SetString(PyExc_TypeError, raw);
    else if (type && has_keywords && message.find("(missing signature)") != std::string_view::npos)
    {
        const std::string function(message.substr(0, message.find('(')));
        PyErr_Format(PyExc_TypeError, "%s: a keyword argument has the wrong type (%s); see help(%s)",
                     function.c_str(), keyword_types(kwds).c_str(), function.c_str());
    }
    else
    {
        PyErr_Restore(exc_type, value, traceback);
        Py_XDECREF(text);
        return result;
    }
    Py_XDECREF(exc_type);
    Py_XDECREF(value);
    Py_XDECREF(traceback);
    Py_XDECREF(text);
    return result;
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
