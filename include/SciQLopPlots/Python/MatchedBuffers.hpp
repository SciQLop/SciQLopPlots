#pragma once

#include <SciQLopPlots/Python/PythonInterface.hpp>
#include <SciQLopPlots/Python/Validation.hpp>

#include <utility>

class MatchedXY
{
    SciQLopPyBuffer _x;
    SciQLopPyBuffer _y;

    MatchedXY(SciQLopPyBuffer x, SciQLopPyBuffer y) : _x(std::move(x)), _y(std::move(y)) { }

public:
    MatchedXY() = default;
    MatchedXY(const MatchedXY&) = default;
    MatchedXY(MatchedXY&&) = default;
    MatchedXY& operator=(const MatchedXY&) = default;
    MatchedXY& operator=(MatchedXY&&) = default;

    static MatchedXY from_py(SciQLopPyBuffer x, SciQLopPyBuffer y)
    {
        sqp::validation::validate_xy(x, y);
        return MatchedXY(std::move(x), std::move(y));
    }

    const SciQLopPyBuffer& x() const { return _x; }
    const SciQLopPyBuffer& y() const { return _y; }

    std::size_t rows() const { return _x.size(0); }
    std::size_t cols() const { return _y.ndim() == 1 ? 1 : _y.size(1); }
};

class MatchedXYZ
{
    SciQLopPyBuffer _x;
    SciQLopPyBuffer _y;
    SciQLopPyBuffer _z;

    MatchedXYZ(SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z)
            : _x(std::move(x)), _y(std::move(y)), _z(std::move(z)) { }

public:
    MatchedXYZ() = default;
    MatchedXYZ(const MatchedXYZ&) = default;
    MatchedXYZ(MatchedXYZ&&) = default;
    MatchedXYZ& operator=(const MatchedXYZ&) = default;
    MatchedXYZ& operator=(MatchedXYZ&&) = default;

    static MatchedXYZ from_py(SciQLopPyBuffer x, SciQLopPyBuffer y, SciQLopPyBuffer z)
    {
        sqp::validation::validate_xyz(x, y, z);
        return MatchedXYZ(std::move(x), std::move(y), std::move(z));
    }

    const SciQLopPyBuffer& x() const { return _x; }
    const SciQLopPyBuffer& y() const { return _y; }
    const SciQLopPyBuffer& z() const { return _z; }

    std::size_t x_len() const { return _x.size(0); }
    std::size_t y_len() const { return _y.size(0); }
};
