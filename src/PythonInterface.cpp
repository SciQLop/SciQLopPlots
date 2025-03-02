/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2022, Plasma Physics Laboratory - CNRS
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/

#include <algorithm>
#include <numeric>
#include <stdexcept>

#if defined(slots) && (defined(__GNUC__) || defined(_MSC_VER) || defined(__clang__))
#pragma push_macro("slots")
#undef slots
extern "C"
{
/*
 * Python 2 uses the "register" keyword, which is deprecated in C++ 11
 * and forbidden in C++17.
 */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-register"
#endif

#include <Python.h>
#include <pybuffer.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}
#else
extern "C"
{
#include <Python.h>
}
#endif

#ifdef _TRACE_REF_COUNT
#include <iostream>
#endif

#define SKIP_PYTHON_INTERFACE_CPP
#include "SciQLopPlots/Python/PythonInterface.hpp"

struct PyAutoScopedGIL
{
    PyGILState_STATE gstate;

    PyAutoScopedGIL() { gstate = PyGILState_Ensure(); }

    ~PyAutoScopedGIL() { PyGILState_Release(gstate); }
};

inline void _inc_ref(PyObject* obj)
{

#ifdef _TRACE_REF_COUNT
    std::cout << "Inc ref " << obj << " " << obj->ob_refcnt << std::endl;
#endif
    PyGILState_STATE state = PyGILState_Ensure();
    Py_INCREF(obj);
    PyGILState_Release(state);
}

inline void _dec_ref(PyObject* obj)
{
    auto scoped_gil = PyAutoScopedGIL();
#ifdef _TRACE_REF_COUNT
    std::cout << "Dec ref: " << obj << " " << obj->ob_refcnt << std::endl;
    if (obj->ob_refcnt == 1)
    {
        std::cout << "Dec ref, Last ref: " << obj << std::endl;
    }
    else if (obj->ob_refcnt > 100)
    {
        std::cout << "Dec ref, weird high refcount: " << obj << " " << obj->ob_refcnt << std::endl;
    }
#endif
    Py_DECREF(obj);
}

struct PyObjectWrapper
{
private:
    std::shared_ptr<PyObject> _py_obj = nullptr;

    inline void share(const PyObjectWrapper& other) { this->_py_obj = other._py_obj; }

public:
    inline PyObjectWrapper() : _py_obj { nullptr } { }

    inline PyObjectWrapper(const PyObjectWrapper& other) : _py_obj { nullptr }
    {
        this->share(other);
    }

    inline PyObjectWrapper(PyObjectWrapper&& other) : _py_obj { nullptr } { this->share(other); }

    explicit PyObjectWrapper(PyObject* obj) : _py_obj { nullptr } { this->set_obj(obj); }

    ~PyObjectWrapper() { }

    inline PyObjectWrapper& operator=(PyObjectWrapper&& other)
    {
        this->share(other);
        return *this;
    }

    inline PyObjectWrapper& operator=(const PyObjectWrapper& other)
    {
        this->share(other);
        return *this;
    }

    inline void set_obj(PyObject* py_obj)
    {
        _py_obj = std::shared_ptr<PyObject>(py_obj, [](PyObject* py_obj) { _dec_ref(py_obj); });
        if (_py_obj != nullptr)
        {
            _inc_ref(py_obj);
        }
    }

    inline PyObject* py_object() const { return _py_obj.get(); }

    inline bool is_null() const { return _py_obj == nullptr; }
};


// TODO: Add a function to create a memoryview from a pointer based on this code:
/*
template <typename T>
PyObject* create_memoryview_from_ptr(T* ptr, std::vector<std::size_t> shape)
{
    // Step 1: Create a capsule to manage the pointer's lifetime
    PyObject* capsule = PyCapsule_New(ptr, "cpp_buffer", nullptr);
    if (!capsule)
    {
        return NULL;
    }

    // Step 2: Fill the Py_buffer struct
    Py_buffer view;
    if (PyBuffer_FillInfo(
            &view, capsule, ptr,
            std::accumulate(std::begin(shape), std::end(shape), 1, std::multiplies<std::size_t>()),
            1, 1, PyBUF_READ)
        < 0)
    {
        Py_DECREF(capsule);
        return NULL;
    }


    view.ndim = 1;
    // view.shape = &size;
    view.strides = nullptr;
    view.suboffsets = nullptr;
    // view.format = "d"; //todo

    // Step 3: Create the memoryview
    PyObject* memoryview = PyMemoryView_FromBuffer(&view);
    if (!memoryview)
    {
        Py_DECREF(capsule);
        return NULL;
    }

    // Step 4: Decrement the capsule's refcount (memoryview holds a reference)
    Py_DECREF(capsule);

    return memoryview;
}
*/


struct _PyBuffer_impl : PyObjectWrapper
{
    Py_buffer buffer = { 0 };
    PyObjectWrapper py_obj;
    std::vector<std::size_t> shape;
    bool is_valid = false;
    bool is_row_major = true;

    _PyBuffer_impl() = default;

    explicit _PyBuffer_impl(PyObject* obj) { this->init_buffer(obj); }

    ~_PyBuffer_impl() { this->release(); }

    inline void init_buffer(PyObject* obj)
    {
        this->py_obj.set_obj(obj);
        {
            auto scoped_gil = PyAutoScopedGIL();
            this->is_valid = PyObject_GetBuffer(obj, &this->buffer,
                                                PyBUF_SIMPLE | PyBUF_READ | PyBUF_ANY_CONTIGUOUS
                                                    | PyBUF_FORMAT)
                == 0;
        }
        if (!this->is_valid)
            throw std::runtime_error("Failed to get buffer from object");
        if (this->buffer.format == nullptr || this->buffer.format[0] != 'd')
            throw std::runtime_error("Buffer must be double type");
        this->is_row_major = PyBuffer_IsContiguous(&this->buffer, 'C') == 1;
        if (this->buffer.ndim > 0)
        {
            this->shape.resize(this->buffer.ndim);
            std::copy_n(this->buffer.shape, std::size(this->shape), std::begin(this->shape));
        }
        else
        {
            this->shape.resize(1);
            this->shape[0] = this->buffer.len / this->buffer.itemsize;
        }
    }

    inline void release()
    {
        if (this->is_valid)
        {
            auto scoped_gil = PyAutoScopedGIL();
            PyBuffer_Release(&this->buffer);
            this->is_valid = false;
            this->buffer = { 0 };
        }
    }
};

void PyBuffer::share(const PyBuffer& other)
{
    this->_impl = other._impl;
}

PyBuffer::PyBuffer() { }

PyBuffer::PyBuffer(const PyBuffer& other)
{
    this->share(other);
}

PyBuffer::PyBuffer(PyBuffer&& other)
{
    this->share(std::move(other));
}

PyBuffer::PyBuffer(PyObject* obj)
{
    this->_impl = std::shared_ptr<_PyBuffer_impl>(new _PyBuffer_impl(obj));
}

PyBuffer::~PyBuffer() { }

PyBuffer& PyBuffer::operator=(const PyBuffer& other)
{
    if (this != &other)
        this->share(other);
    return *this;
}

PyBuffer& PyBuffer::operator=(PyBuffer&& other)
{
    if (this != &other)
        this->share(other);
    return *this;
}

bool PyBuffer::is_valid() const
{
    if (this->_impl)
        return this->_impl->is_valid;
    return false;
}

const std::vector<std::size_t>& PyBuffer::shape() const
{
    if (this->_impl)
        return this->_impl->shape;
    static std::vector<std::size_t> empty;
    return empty;
}

std::size_t PyBuffer::ndim() const
{
    if (is_valid())
    {
        return std::size(this->_impl->shape);
    }
    return 0;
}

std::size_t PyBuffer::size(std::size_t index) const
{
    if (is_valid())
    {
        if (static_cast<decltype(std::size(this->_impl->shape))>(index)
            < std::size(this->_impl->shape))
        {
            return this->_impl->shape[index];
        }
    }
    return 0;
}

double* PyBuffer::data() const
{
    if (is_valid())
    {
        return reinterpret_cast<double*>(this->_impl->buffer.buf);
    }
    return nullptr;
}

bool PyBuffer::row_major() const
{
    if (is_valid())
    {
        return this->_impl->is_row_major;
    }
    return false;
}

PyObject* PyBuffer::py_object() const
{
    if (_impl)
        return this->_impl->py_obj.py_object();
    return nullptr;
}

void PyBuffer::release() { }

std::size_t PyBuffer::flat_size() const
{
    if (is_valid())
    {
        return this->_impl->buffer.len / this->_impl->buffer.itemsize;
    }
    return 0;
}

struct _GetDataPyCallable_impl
{
    PyObjectWrapper _py_obj;
    bool _is_valid = false;

    _GetDataPyCallable_impl() = default;
    _GetDataPyCallable_impl(const _GetDataPyCallable_impl&) = default;
    _GetDataPyCallable_impl(_GetDataPyCallable_impl&&) = default;

    _GetDataPyCallable_impl(PyObject* obj) { this->_init_callable(obj); }

    ~_GetDataPyCallable_impl() { }

    _GetDataPyCallable_impl& operator=(const _GetDataPyCallable_impl&) = default;
    _GetDataPyCallable_impl& operator=(_GetDataPyCallable_impl&&) = default;

    inline void _init_callable(PyObject* obj)
    {
        this->_py_obj.set_obj(obj);
        auto scoped_gil = PyAutoScopedGIL();
        this->_is_valid = PyCallable_Check(obj);
    }

    inline std::vector<PyBuffer> get_data(double lower, double upper)
    {
        std::vector<PyBuffer> data;
        if (_is_valid)
        {
            auto scoped_gil = PyAutoScopedGIL();
            auto args = PyTuple_New(2);
            PyTuple_SetItem(args, 0, PyFloat_FromDouble(lower));
            PyTuple_SetItem(args, 1, PyFloat_FromDouble(upper));
            auto res = PyObject_CallObject(_py_obj.py_object(), args);
            Py_DECREF(args);
            if (res != nullptr)
            {
                if (PyList_Check(res))
                {
                    auto size = PyList_Size(res);
                    for (Py_ssize_t i = 0; i < size; ++i)
                    {
                        data.emplace_back(PyList_GetItem(res, i));
                    }
                }
                else if (PyTuple_Check(res))
                {
                    auto size = PyTuple_Size(res);
                    for (Py_ssize_t i = 0; i < size; ++i)
                    {
                        data.emplace_back(PyTuple_GetItem(res, i));
                    }
                }
                Py_DECREF(res);
            }
        }
        return data;
    }

    inline std::vector<PyBuffer> get_data(PyBuffer x, PyBuffer y)
    {
        std::vector<PyBuffer> data;
        if (_is_valid)
        {
            auto scoped_gil = PyAutoScopedGIL();
            auto args = PyTuple_New(2);
            Py_IncRef(x.py_object());
            Py_IncRef(y.py_object());
            PyTuple_SetItem(args, 0, x.py_object());
            PyTuple_SetItem(args, 1, y.py_object());
            auto res = PyObject_CallObject(_py_obj.py_object(), args);
            Py_DECREF(args);
            if (res != nullptr)
            {
                if (PyList_Check(res))
                {
                    auto size = PyList_Size(res);
                    for (Py_ssize_t i = 0; i < size; ++i)
                    {
                        data.emplace_back(PyList_GetItem(res, i));
                    }
                }
                else if (PyTuple_Check(res))
                {
                    auto size = PyTuple_Size(res);
                    for (Py_ssize_t i = 0; i < size; ++i)
                    {
                        data.emplace_back(PyTuple_GetItem(res, i));
                    }
                }
                Py_DECREF(res);
            }
        }
        return data;
    }

    inline std::vector<PyBuffer> get_data(PyBuffer x, PyBuffer y, PyBuffer z)
    {
        std::vector<PyBuffer> data;
        if (_is_valid)
        {
            auto scoped_gil = PyAutoScopedGIL();
            auto args = PyTuple_New(2);
            Py_IncRef(x.py_object());
            Py_IncRef(y.py_object());
            Py_IncRef(z.py_object());
            PyTuple_SetItem(args, 0, x.py_object());
            PyTuple_SetItem(args, 1, y.py_object());
            PyTuple_SetItem(args, 2, z.py_object());
            auto res = PyObject_CallObject(_py_obj.py_object(), args);
            Py_DECREF(args);
            if (res != nullptr)
            {
                if (PyList_Check(res))
                {
                    auto size = PyList_Size(res);
                    for (Py_ssize_t i = 0; i < size; ++i)
                    {
                        data.emplace_back(PyList_GetItem(res, i));
                    }
                }
                else if (PyTuple_Check(res))
                {
                    auto size = PyTuple_Size(res);
                    for (Py_ssize_t i = 0; i < size; ++i)
                    {
                        data.emplace_back(PyTuple_GetItem(res, i));
                    }
                }
                Py_DECREF(res);
            }
        }
        return data;
    }
};

void GetDataPyCallable::steal(GetDataPyCallable& other)
{
    if (is_valid())
        this->release();
    if (other.is_valid())
    {
        this->_impl = new _GetDataPyCallable_impl(other.py_object());
        other.release();
    }
}

void GetDataPyCallable::share(const GetDataPyCallable& other)
{
    if (is_valid())
        this->release();
    if (other.is_valid())
    {
        this->_impl = new _GetDataPyCallable_impl(other.py_object());
    }
}

GetDataPyCallable::GetDataPyCallable(PyObject* obj)
{
    this->_impl = new _GetDataPyCallable_impl(obj);
}

GetDataPyCallable::GetDataPyCallable() { }

GetDataPyCallable::GetDataPyCallable(const GetDataPyCallable& other)
{
    share(other);
}

GetDataPyCallable::GetDataPyCallable(GetDataPyCallable&& other)
{
    steal(other);
}

GetDataPyCallable& GetDataPyCallable::operator=(const GetDataPyCallable& other)
{
    if (this != &other)
    {
        share(other);
    }
    return *this;
}

GetDataPyCallable::~GetDataPyCallable()
{
    release();
}

GetDataPyCallable& GetDataPyCallable::operator=(GetDataPyCallable&& other)
{
    if (this != &other)
    {
        steal(other);
    }
    return *this;
}

PyObject* GetDataPyCallable::py_object() const
{
    if (this->_impl)
        return this->_impl->_py_obj.py_object();
    return nullptr;
}

bool GetDataPyCallable::is_valid() const
{
    return this->_impl && this->_impl->_is_valid;
}

void GetDataPyCallable::release()
{
    if (this->_impl)
    {
        delete this->_impl;
        this->_impl = nullptr;
    }
}

std::vector<PyBuffer> GetDataPyCallable::get_data(double lower, double upper)
{
    if (this->_impl)
        return this->_impl->get_data(lower, upper);
    return {};
}

std::vector<PyBuffer> GetDataPyCallable::get_data(PyBuffer x, PyBuffer y)
{
    if (this->_impl)
        return this->_impl->get_data(x, y);
    return {};
}

std::vector<PyBuffer> GetDataPyCallable::get_data(PyBuffer x, PyBuffer y, PyBuffer z)
{
    if (this->_impl)
        return this->_impl->get_data(x, y, z);
    return {};
}
