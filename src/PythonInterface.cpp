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
    PyObject* _py_obj = nullptr;


    inline void take(PyObjectWrapper& other)
    {
        this->set_obj(other.py_object());
        other.release_obj();
    }

    inline void borrow(const PyObjectWrapper& other) { this->set_obj(other.py_object()); }

public:
    inline PyObjectWrapper() : _py_obj { nullptr } { }
    inline PyObjectWrapper(const PyObjectWrapper& other) : _py_obj { nullptr }
    {
        this->borrow(other);
    }
    inline PyObjectWrapper(PyObjectWrapper&& other) : _py_obj { nullptr } { this->take(other); }
    explicit PyObjectWrapper(PyObject* obj) : _py_obj { nullptr } { this->set_obj(obj); }
    ~PyObjectWrapper() { this->release_obj(); }
    inline PyObjectWrapper& operator=(PyObjectWrapper&& other)
    {
        this->take(other);
        return *this;
    }
    inline PyObjectWrapper& operator=(const PyObjectWrapper& other)
    {
        this->borrow(other);
        return *this;
    }

    inline void set_obj(PyObject* py_obj)
    {
        this->release_obj();
        _py_obj = py_obj;
        if (_py_obj != nullptr)
        {
            _inc_ref(_py_obj);
        }
    }

    inline void release_obj()
    {
        if (_py_obj != nullptr)
        {
            _dec_ref(_py_obj);
            _py_obj = nullptr;
        }
    }

    inline PyObject* py_object() const { return _py_obj; }
    inline bool is_null() const { return _py_obj == nullptr; }
};


struct _Array_view_impl
{
    Py_buffer buffer = { 0 };
    PyObjectWrapper py_obj;
    bool is_valid = false;
    std::vector<std::size_t> shape;

    _Array_view_impl() = default;
    explicit _Array_view_impl(PyObject* obj) { this->init_buffer(obj); }
    _Array_view_impl(const _Array_view_impl& other) { this->init_buffer(other.py_obj.py_object()); }
    _Array_view_impl(_Array_view_impl&& other) { this->init_buffer(other.py_obj.py_object()); }
    ~_Array_view_impl() { this->release(); }

    _Array_view_impl& operator=(const _Array_view_impl& other)
    {
        this->init_buffer(other.py_obj.py_object());
        return *this;
    }

    _Array_view_impl& operator=(_Array_view_impl&& other)
    {
        this->init_buffer(other.py_obj.py_object());
        return *this;
    }

    inline void init_buffer(PyObject* obj)
    {
        this->py_obj.set_obj(obj);
        {
            auto scoped_gil = PyAutoScopedGIL();
            this->is_valid = PyObject_GetBuffer(
                                 obj, &this->buffer, PyBUF_SIMPLE | PyBUF_READ | PyBUF_C_CONTIGUOUS)
                == 0;
        }
        assert(this->is_valid);
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
        this->py_obj.release_obj();
    }
};


void Array_view::steal(Array_view&& other)
{
    if (is_valid())
        this->release();
    if (other.is_valid())
    {
        this->_impl = new _Array_view_impl(other.py_object());
        other.release();
    }
}

void Array_view::share(const Array_view& other)
{
    if (is_valid())
        this->release();
    if (other.is_valid())
    {
        this->_impl = new _Array_view_impl(other.py_object());
    }
}

Array_view::Array_view() { }

Array_view::Array_view(const Array_view& other)
{
    this->share(other);
}

Array_view::Array_view(Array_view&& other)
{
    this->steal(std::move(other));
}

Array_view::Array_view(PyObject* obj)
{
    this->_impl = new _Array_view_impl(obj);
}

Array_view::~Array_view()
{
    if (this->_impl)
    {
        delete this->_impl;
    }
}

Array_view& Array_view::operator=(const Array_view& other)
{
    if (this != &other)
        this->share(other);
    return *this;
}

Array_view& Array_view::operator=(Array_view&& other)
{
    if (this != &other)
        this->steal(std::move(other));
    return *this;
}

bool Array_view::is_valid() const
{
    if (this->_impl)
        return this->_impl->is_valid;
    return false;
}

const std::vector<std::size_t>& Array_view::shape() const
{
    if (this->_impl)
        return this->_impl->shape;
    static std::vector<std::size_t> empty;
    return empty;
}

std::size_t Array_view::ndim()
{
    if (is_valid())
    {
        return std::size(this->_impl->shape);
    }
    return 0;
}

std::size_t Array_view::size(std::size_t index)
{
    if (is_valid())
    {
        if (static_cast<int>(index) < std::size(this->_impl->shape))
        {
            return this->_impl->shape[index];
        }
    }
    return 0;
}

double* Array_view::data() const
{
    if (is_valid())
    {
        return reinterpret_cast<double*>(this->_impl->buffer.buf);
    }
    return nullptr;
}

std::vector<double> Array_view::to_std_vect()
{
    assert(this->_impl->is_valid);
    auto sz = flat_size();
    std::vector<double> v(sz);
    auto d_ptr = this->data();
    std::copy(d_ptr, d_ptr + sz, std::begin(v));
    return v;
}

PyObject* Array_view::py_object() const
{
    if (_impl)
        return this->_impl->py_obj.py_object();
    return nullptr;
}


void Array_view::release()
{
    if (this->_impl)
    {
        delete this->_impl;
        this->_impl = nullptr;
    }
}

std::size_t Array_view::flat_size() const
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

    inline std::vector<Array_view> get_data(double lower, double upper)
    {
        std::vector<Array_view> data;
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

std::vector<Array_view> GetDataPyCallable::get_data(double lower, double upper)
{
    if (this->_impl)
        return this->_impl->get_data(lower, upper);
    return {};
}
