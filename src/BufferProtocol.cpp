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

#include "SciQLopPlots/Python/BufferProtocol.hpp"

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


void Array_view::_init_buffer(PyObject* obj)
{
    this->_py_obj = obj;
    PyGILState_STATE state = PyGILState_Ensure();
    this->_is_valid
        = PyObject_GetBuffer(obj, &this->_buffer, PyBUF_SIMPLE | PyBUF_READ | PyBUF_C_CONTIGUOUS)
        == 0;
    PyGILState_Release(state);
    // std::cout << "Array_view PyObject_GetBuffer" << std::endl;
    assert(this->_is_valid);
    if (this->_buffer.ndim > 0)
    {
        this->_shape.resize(this->_buffer.ndim);
        std::copy_n(this->_buffer.shape, std::size(this->_shape), std::begin(this->_shape));
    }
    else
    {
        this->_shape.resize(1);
        this->_shape[0] = this->_buffer.len / this->_buffer.itemsize;
    }
}

Array_view::Array_view(PyObject* obj)
{
    this->_init_buffer(obj);
}

Array_view::~Array_view()
{
    this->release();
}

std::vector<std::size_t> Array_view::shape() const
{
    std::vector<std::size_t> shape;
    if (this->_is_valid)
    {
        shape.resize(std::size(this->_shape));
        std::copy_n(std::cbegin(this->_shape), std::size(this->_shape), std::begin(shape));
    }
    return shape;
}

std::size_t Array_view::ndim()
{
    if (this->_is_valid)
    {
        return std::size(this->_shape);
    }
    return 0;
}

std::size_t Array_view::size(std::size_t index)
{
    if (this->_is_valid)
    {
        if (static_cast<int>(index) < this->_buffer.ndim)
        {
            return this->_shape[index];
        }
    }
    return 0;
}

double* Array_view::data() const
{
    if (this->_is_valid)
    {
        return reinterpret_cast<double*>(this->_buffer.buf);
    }
    return nullptr;
}

std::vector<double> Array_view::to_std_vect()
{
    assert(this->_is_valid);
    auto sz = flat_size();
    std::vector<double> v(sz);
    auto d_ptr = this->data();
    std::copy(d_ptr, d_ptr + sz, std::begin(v));
    return v;
}


PyObjectWrapper::~PyObjectWrapper()
{
    this->release_obj();
}
