#pragma once

#include <SciQLopPlots/Python/PythonInterface.hpp>

#include <QList>

#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace sqp::validation {

inline void validate_buffer(const PyBuffer& b,
                            std::string_view name,
                            int expected_ndim = -1,
                            char expected_dtype = 0)
{
    if (!b.is_valid())
    {
        std::ostringstream os;
        os << name << ": invalid or null buffer";
        throw std::invalid_argument(os.str());
    }

    const char fmt = b.format_code();
    constexpr std::string_view numeric_formats = "bBhHiIlLqQfd";
    if (fmt == '\0' || numeric_formats.find(fmt) == std::string_view::npos)
    {
        std::ostringstream os;
        os << name << ": dtype must be numeric (got '" << fmt << "')";
        throw std::invalid_argument(os.str());
    }

    if (expected_ndim >= 0 && static_cast<int>(b.ndim()) != expected_ndim)
    {
        std::ostringstream os;
        os << name << ": ndim is " << b.ndim() << ", expected " << expected_ndim;
        throw std::invalid_argument(os.str());
    }

    if (expected_dtype != 0 && fmt != expected_dtype)
    {
        std::ostringstream os;
        os << name << ": dtype must be '" << expected_dtype << "' (got '" << fmt << "')";
        throw std::invalid_argument(os.str());
    }
}

inline void validate_index(long long i, long long size, std::string_view name)
{
    if (i < 0)
    {
        std::ostringstream os;
        os << name << ": index is negative (" << i << ")";
        throw std::out_of_range(os.str());
    }
    if (i >= size)
    {
        std::ostringstream os;
        os << name << ": index " << i << " out of range (size " << size << ")";
        throw std::out_of_range(os.str());
    }
}

inline void validate_same_length(const PyBuffer& a, std::string_view name_a,
                                 const PyBuffer& b, std::string_view name_b)
{
    if (a.size(0) != b.size(0))
    {
        std::ostringstream os;
        os << "length mismatch: " << name_a << " has " << a.size(0)
           << ", " << name_b << " has " << b.size(0);
        throw std::invalid_argument(os.str());
    }
}

inline void validate_xy(const PyBuffer& x, const PyBuffer& y)
{
    validate_buffer(x, "x", 1);
    if (y.ndim() != 1 && y.ndim() != 2)
    {
        std::ostringstream os;
        os << "y: ndim must be 1 or 2 (got " << y.ndim() << ")";
        throw std::invalid_argument(os.str());
    }
    validate_buffer(y, "y");  // numeric check; ndim handled above
    validate_same_length(x, "x", y, "y");
}

inline void validate_xyz(const PyBuffer& x, const PyBuffer& y, const PyBuffer& z)
{
    validate_buffer(x, "x", 1);
    validate_buffer(y, "y", 1);
    validate_buffer(z, "z", 2);
    if (z.size(0) != x.size(0))
    {
        std::ostringstream os;
        os << "z: rows (" << z.size(0) << ") must match x length ("
           << x.size(0) << ")";
        throw std::invalid_argument(os.str());
    }
    if (z.size(1) != y.size(0))
    {
        std::ostringstream os;
        os << "z: cols (" << z.size(1) << ") must match y length ("
           << y.size(0) << ")";
        throw std::invalid_argument(os.str());
    }
}

} // namespace sqp::validation
