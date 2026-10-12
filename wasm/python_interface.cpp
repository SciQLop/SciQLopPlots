// PythonInterface.hpp without Python, for the browser build: a buffer owns a copy of a
// JavaScript typed array, and there are no Python callables (JavaScript pushes data with
// set_data instead), so a GetDataPyCallable is never valid.
#include "SciQLopPlots/Python/PythonInterface.hpp"

#include <stdexcept>

struct _PyBuffer_impl
{
    std::vector<unsigned char> bytes;
    char format = '\0';
    std::size_t item_size = 0;
    std::vector<std::size_t> shape;
};

struct _GetDataPyCallable_impl
{
};

SciQLopPyBuffer::SciQLopPyBuffer() { }

SciQLopPyBuffer::SciQLopPyBuffer(const SciQLopPyBuffer& other) : _impl(other._impl) { }

SciQLopPyBuffer::SciQLopPyBuffer(SciQLopPyBuffer&& other) noexcept : _impl(other._impl) { }

SciQLopPyBuffer::SciQLopPyBuffer(PyObject*)
{
    throw std::logic_error("SciQLopPyBuffer: no Python objects in the browser build");
}

SciQLopPyBuffer::SciQLopPyBuffer(std::vector<unsigned char> bytes, char format,
                                 std::size_t item_size, std::vector<std::size_t> shape)
        : _impl(std::make_shared<_PyBuffer_impl>(
              _PyBuffer_impl { std::move(bytes), format, item_size, std::move(shape) }))
{
}

SciQLopPyBuffer::~SciQLopPyBuffer() { }

void SciQLopPyBuffer::share(const SciQLopPyBuffer& other)
{
    _impl = other._impl;
}

SciQLopPyBuffer& SciQLopPyBuffer::operator=(const SciQLopPyBuffer& other)
{
    _impl = other._impl;
    return *this;
}

SciQLopPyBuffer& SciQLopPyBuffer::operator=(SciQLopPyBuffer&& other) noexcept
{
    _impl = other._impl;
    return *this;
}

bool SciQLopPyBuffer::is_valid() const
{
    return _impl != nullptr;
}

const std::vector<std::size_t>& SciQLopPyBuffer::shape() const
{
    static const std::vector<std::size_t> empty;
    return _impl ? _impl->shape : empty;
}

std::size_t SciQLopPyBuffer::ndim() const
{
    return shape().size();
}

std::size_t SciQLopPyBuffer::size(std::size_t index) const
{
    return index < ndim() ? shape()[index] : 0;
}

std::size_t SciQLopPyBuffer::flat_size() const
{
    return _impl ? _impl->bytes.size() / _impl->item_size : 0;
}

double* SciQLopPyBuffer::data() const
{
    if (!_impl)
        throw std::runtime_error("SciQLopPyBuffer::data() called on invalid buffer");
    if (_impl->format != 'd')
        throw std::runtime_error(
            "SciQLopPyBuffer::data() called on non-double buffer; use raw_data() + format_code()");
    return reinterpret_cast<double*>(_impl->bytes.data());
}

bool SciQLopPyBuffer::row_major() const
{
    return is_valid();
}

char SciQLopPyBuffer::format_code() const
{
    return _impl ? _impl->format : '\0';
}

void* SciQLopPyBuffer::raw_data() const
{
    return _impl ? _impl->bytes.data() : nullptr;
}

std::size_t SciQLopPyBuffer::item_size() const
{
    return _impl ? _impl->item_size : 0;
}

PyObject* SciQLopPyBuffer::py_object() const
{
    return nullptr;
}

GetDataPyCallable::GetDataPyCallable(PyObject*)
{
    throw std::logic_error("GetDataPyCallable: no Python objects in the browser build");
}

GetDataPyCallable::GetDataPyCallable() { }

GetDataPyCallable::GetDataPyCallable(const GetDataPyCallable&) { }

GetDataPyCallable::GetDataPyCallable(GetDataPyCallable&&) { }

GetDataPyCallable& GetDataPyCallable::operator=(const GetDataPyCallable&)
{
    return *this;
}

GetDataPyCallable& GetDataPyCallable::operator=(GetDataPyCallable&&)
{
    return *this;
}

GetDataPyCallable::~GetDataPyCallable() { }

void GetDataPyCallable::steal(GetDataPyCallable&) { }

void GetDataPyCallable::share(const GetDataPyCallable&) { }

PyObject* GetDataPyCallable::py_object() const
{
    return nullptr;
}

bool GetDataPyCallable::is_valid() const
{
    return false;
}

void GetDataPyCallable::release() { }

SciQLopPyDataBatch GetDataPyCallable::get_data(double, double)
{
    return {};
}

std::vector<SciQLopPyBuffer> GetDataPyCallable::get_data(SciQLopPyBuffer, SciQLopPyBuffer)
{
    return {};
}

std::vector<SciQLopPyBuffer> GetDataPyCallable::get_data(SciQLopPyBuffer, SciQLopPyBuffer,
                                                         SciQLopPyBuffer)
{
    return {};
}
