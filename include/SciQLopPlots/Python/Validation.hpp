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

} // namespace sqp::validation
