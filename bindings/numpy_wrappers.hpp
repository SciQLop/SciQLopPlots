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
#pragma once

#define NPY_NO_DEPRECATED_API NPY_1_7_API_VERSION
#if defined(slots) &&                                                          \
    (defined(__GNUC__) || defined(_MSC_VER) || defined(__clang__))
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
#include <numpy/arrayobject.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#endif
}
#else
#include <Python.h>
#include <numpy/arrayobject.h>
#endif
#include <algorithm>
#include <assert.h>
#include <cpp_utils/warnings.h>
#include <map>
#include <numeric>
#include <vector>

inline int init_numpy()
{
  DISABLE_WARNING_PUSH
  DISABLE_WARNING_CONVERSION_NULL
  import_array(); // PyError if not successful
  DISABLE_WARNING_POP
  return 0;
}
const static int numpy_initialized = init_numpy();
template<typename dest_type = PyObject> struct PyObjectWrapper
{
private:
  PyObject* _py_obj = nullptr;
  void inc_refcount() { Py_XINCREF(_py_obj); }
  void dec_refcount()
  {
    Py_XDECREF(_py_obj);
    _py_obj = nullptr;
  }

public:
  PyObjectWrapper() : _py_obj{nullptr} {}
  PyObjectWrapper(const PyObjectWrapper& other) : _py_obj{other._py_obj}
  {
    inc_refcount();
  }
  PyObjectWrapper(PyObjectWrapper&& other) : _py_obj{other._py_obj}
  {
    inc_refcount();
  }
  explicit PyObjectWrapper(PyObject* obj) : _py_obj{obj} { inc_refcount(); }
  ~PyObjectWrapper() { dec_refcount(); }
  PyObjectWrapper& operator=(PyObjectWrapper&& other)
  {
    dec_refcount();
    this->_py_obj = other._py_obj;
    inc_refcount();
    return *this;
  }
  PyObjectWrapper& operator=(const PyObjectWrapper& other)
  {
    dec_refcount();
    this->_py_obj = other._py_obj;
    inc_refcount();
    return *this;
  }

  PyObject* py_object() { return _py_obj; }
  inline dest_type* get() { return reinterpret_cast<dest_type*>(_py_obj); }
  inline bool is_null() { return _py_obj == nullptr; }
};

struct NpArray_view
{
private:
  PyObjectWrapper<PyArrayObject> _py_obj;
  NpArray_view(const NpArray_view&& other) = delete;

public:
  static bool isNpArray(PyObject* obj)
  {
    auto arr       = reinterpret_cast<PyArrayObject*>(obj);
    auto is_c_aray = obj && PyArray_Check(arr) && PyArray_ISCARRAY(arr);
    return is_c_aray;
  }
  NpArray_view() : _py_obj{nullptr} {}
  NpArray_view(const NpArray_view& other) : _py_obj{other._py_obj} {}
  NpArray_view(NpArray_view&& other) : _py_obj{other._py_obj} {}
  explicit NpArray_view(PyObject* obj) : _py_obj{obj}
  {
    assert(isNpArray(obj));
    assert(PyArray_ISFLOAT(_py_obj.get()));
  }

  NpArray_view& operator=(const NpArray_view& other)
  {
    this->_py_obj = other._py_obj;
    return *this;
  }

  NpArray_view& operator=(NpArray_view&& other)
  {
    this->_py_obj = other._py_obj;
    return *this;
  }

  std::vector<std::size_t> shape()
  {
    std::vector<std::size_t> shape;
    if(!_py_obj.is_null())
    {
      if(int ndim = PyArray_NDIM(_py_obj.get()); ndim > 0)
      {
        if(ndim < 10)
        {
          shape.resize(ndim);
          std::copy_n(PyArray_SHAPE(_py_obj.get()), ndim, std::begin(shape));
        }
      }
    }
    return shape;
  }

  std::size_t ndim()
  {
    if(!_py_obj.is_null())
    {
      return static_cast<std::size_t>(PyArray_NDIM(_py_obj.get()));
    }
    return 0;
  }

  std::size_t size(std::size_t index = 0)
  {
    if(!_py_obj.is_null())
    {
      if(index < static_cast<std::size_t>(PyArray_NDIM(_py_obj.get())))
      {
        return PyArray_SHAPE(_py_obj.get())[index];
      }
    }
    return 0;
  }

  std::size_t flat_size()
  {
    auto s = this->shape();
    return std::accumulate(std::cbegin(s), std::cend(s), 1,
                           [](const auto& a, const auto& b) { return a * b; });
  }

  double data(std::size_t pos)
  {
    if(!_py_obj.is_null())
    {
      return reinterpret_cast<double*>(PyArray_DATA(_py_obj.get()))[pos];
    }
    return nan("NAN");
  }

  std::vector<double> to_std_vect()
  {
    assert(!this->_py_obj.is_null());
    auto sz = flat_size();
    std::vector<double> v(sz);
    auto d_ptr = reinterpret_cast<double*>(PyArray_DATA(_py_obj.get()));
    std::copy(d_ptr, d_ptr + sz, std::begin(v));
    return v;
  }

  PyObject* py_object() { return _py_obj.py_object(); }
};

struct NpArray
{
  std::vector<std::size_t> shape;
  std::vector<double> data;
  static bool isNpArray(PyObject* obj) { return NpArray_view::isNpArray(obj); }
  NpArray() = default;
  explicit NpArray(PyObject* obj)
  {
    if(obj)
    {
      NpArray_view view{obj};
      shape = view.shape();
      data  = view.to_std_vect();
    }
  }

  inline std::size_t ndim() { return shape.size(); }

  std::size_t size(std::size_t index = 0)
  {
    if(index < shape.size()) return shape[index];
    return 0;
  }

  std::size_t flat_size()
  {
    return std::accumulate(std::cbegin(shape), std::cend(shape), 1,
                           [](const auto& a, const auto& b) { return a * b; });
  }

  // TODO maybe ;)
  PyObject* py_object() { return nullptr; }
};

