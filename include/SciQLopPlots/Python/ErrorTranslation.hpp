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

//! A list of colours: each name in it is checked like a single colour argument.
inline void check_color_list_arg(PyObject* arg, const char* function)
{
    if (arg == nullptr || PyErr_Occurred() || !(PyList_Check(arg) || PyTuple_Check(arg)))
        return;
    const Py_ssize_t size = PySequence_Size(arg);
    for (Py_ssize_t i = 0; i < size && !PyErr_Occurred(); ++i)
    {
        PyObject* item = PySequence_GetItem(arg, i);
        check_color_arg(item, function);
        Py_XDECREF(item);
    }
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

//! The name of \a type, for messages.
inline std::string type_name_of(PyTypeObject* type)
{
    PyObject* name = PyType_GetName(type);
    const char* text = name != nullptr ? PyUnicode_AsUTF8AndSize(name, nullptr) : nullptr;
    std::string result = text != nullptr ? text : "?";
    Py_XDECREF(name);
    return result;
}

//! The name of \a object's type, for messages.
inline std::string type_name(PyObject* object)
{
    PyObject* name = PyType_GetName(Py_TYPE(object));
    const char* text = name != nullptr ? PyUnicode_AsUTF8AndSize(name, nullptr) : nullptr;
    std::string result = text != nullptr ? text : "?";
    Py_XDECREF(name);
    return result;
}

//! "int, str, bins=int": a call's argument types. \a args is the tuple of positional arguments when
//! \a args_is_tuple, else the only one.
inline std::string argument_types(PyObject* args, bool args_is_tuple, PyObject* kwds)
{
    std::string listed;
    const auto add = [&listed](const std::string& item) { listed += (listed.empty() ? "" : ", ") + item; };
    if (args != nullptr && args_is_tuple && PyTuple_Check(args))
        for (Py_ssize_t i = 0; i < PyTuple_Size(args); ++i)
            add(type_name(PyTuple_GetItem(args, i)));
    else if (args != nullptr && !args_is_tuple)
        add(type_name(args));
    PyObject *key, *value;
    Py_ssize_t pos = 0;
    while (kwds != nullptr && PyDict_Check(kwds) && PyDict_Next(kwds, &pos, &key, &value))
    {
        const char* k = PyUnicode_AsUTF8AndSize(key, nullptr);
        add(std::string(k != nullptr ? k : "?") + "=" + type_name(value));
    }
    PyErr_Clear(); // a failed lookup above must not replace the error being reported
    return listed;
}

//! A Python override of a data provider's get_data raised. Shiboken would print the traceback and
//! return an empty list, so the provider marked the range as loaded and reported nothing. Print it
//! once, as shiboken would, then throw: the provider emits fetch_failed and fetches the range again.
[[noreturn]] inline void throw_provider_override_error(const char* owner, const char* function)
{
    PyObject *type, *value, *traceback;
    PyErr_Fetch(&type, &value, &traceback);
    std::string text = std::string(owner) + "." + function + "() raised ";
    text += type != nullptr ? type_name_of(reinterpret_cast<PyTypeObject*>(type)) : std::string("?");
    if (PyObject* message = value != nullptr ? PyObject_Str(value) : nullptr)
    {
        if (const char* raw = PyUnicode_AsUTF8AndSize(message, nullptr))
            text += std::string(": ") + raw;
        Py_DECREF(message);
    }
    PyErr_Clear();
    PyErr_Restore(type, value, traceback);
    PyErr_Print();
    throw std::runtime_error(text);
}

//! Same, for an override that returned something the provider can't use.
[[noreturn]] inline void throw_provider_override_result(const char* owner, const char* function,
                                                        const char* expected, const char* got)
{
    throw type_error(std::string(owner) + "." + function + "() must return a " + expected + ", got "
                     + got);
}

//! Shiboken's argument errors that don't say why, made into TypeErrors that do. \a result is what
//! returnWrongArguments returned; \a args and \a kwds are the call's arguments.
//! - An unknown keyword is an AttributeError ("unsupported keyword"); Python says TypeError.
//! - With keywords given, shiboken can't print the signatures: "f((args,)) is wrong (missing
//!   signature)", or for a constructor a ValueError "called with wrong argument values" when a
//!   keyword doesn't convert. Name the types that were given instead.
template <typename Result>
Result wrong_arguments(Result result, PyObject* args, bool args_is_tuple, PyObject* kwds)
{
    const bool attribute = PyErr_Occurred() && PyErr_ExceptionMatches(PyExc_AttributeError);
    const bool type = PyErr_Occurred() && PyErr_ExceptionMatches(PyExc_TypeError);
    const bool value = PyErr_Occurred() && PyErr_ExceptionMatches(PyExc_ValueError);
    const bool has_keywords = kwds != nullptr && PyDict_Check(kwds) && PyDict_Size(kwds) > 0;
    if (!attribute && !type && !(value && has_keywords))
        return result;
    PyObject *exc_type, *exc_value, *traceback;
    PyErr_Fetch(&exc_type, &exc_value, &traceback);
    PyObject* text = exc_value != nullptr ? PyObject_Str(exc_value) : nullptr;
    const char* raw = text != nullptr ? PyUnicode_AsUTF8AndSize(text, nullptr) : nullptr;
    const std::string_view message = raw != nullptr ? raw : "";
    const auto quoted_function = [&message]
    {
        // "'pkg.Class.__init__' called with ...": the name between the first two quotes.
        const auto first = message.find('\'');
        return std::string(message.substr(first + 1, message.find('\'', first + 1) - first - 1));
    };
    if (attribute && message.find("unsupported keyword") != std::string_view::npos)
        PyErr_SetString(PyExc_TypeError, raw);
    else if ((type && message.find("(missing signature)") != std::string_view::npos)
             || (value && message.find("called with wrong argument values") != std::string_view::npos))
    {
        const std::string function = value ? quoted_function() : std::string(message.substr(0, message.find('(')));
        const std::string given = argument_types(args, args_is_tuple, kwds);
        PyErr_Format(PyExc_TypeError, "%s: no signature takes (%s); see help(%s)", function.c_str(),
                     given.c_str(), function.c_str());
    }
    else
    {
        PyErr_Restore(exc_type, exc_value, traceback);
        Py_XDECREF(text);
        return result;
    }
    Py_XDECREF(exc_type);
    Py_XDECREF(exc_value);
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
