#pragma once

#include <SciQLopPlots/Python/PythonInterface.hpp>
#include <SciQLopPlots/Python/Validation.hpp>

#include <utility>

class MatchedXY
{
    PyBuffer _x;
    PyBuffer _y;

    MatchedXY(PyBuffer x, PyBuffer y) : _x(std::move(x)), _y(std::move(y)) { }

public:
    MatchedXY() = default;
    MatchedXY(const MatchedXY&) = default;
    MatchedXY(MatchedXY&&) = default;
    MatchedXY& operator=(const MatchedXY&) = default;
    MatchedXY& operator=(MatchedXY&&) = default;

    static MatchedXY from_py(PyBuffer x, PyBuffer y)
    {
        sqp::validation::validate_xy(x, y);
        return MatchedXY(std::move(x), std::move(y));
    }

    const PyBuffer& x() const { return _x; }
    const PyBuffer& y() const { return _y; }

    std::size_t rows() const { return _x.size(0); }
    std::size_t cols() const { return _y.ndim() == 1 ? 1 : _y.size(1); }
};

class MatchedXYZ
{
    PyBuffer _x;
    PyBuffer _y;
    PyBuffer _z;

    MatchedXYZ(PyBuffer x, PyBuffer y, PyBuffer z)
            : _x(std::move(x)), _y(std::move(y)), _z(std::move(z)) { }

public:
    MatchedXYZ() = default;
    MatchedXYZ(const MatchedXYZ&) = default;
    MatchedXYZ(MatchedXYZ&&) = default;
    MatchedXYZ& operator=(const MatchedXYZ&) = default;
    MatchedXYZ& operator=(MatchedXYZ&&) = default;

    static MatchedXYZ from_py(PyBuffer x, PyBuffer y, PyBuffer z)
    {
        sqp::validation::validate_xyz(x, y, z);
        return MatchedXYZ(std::move(x), std::move(y), std::move(z));
    }

    const PyBuffer& x() const { return _x; }
    const PyBuffer& y() const { return _y; }
    const PyBuffer& z() const { return _z; }

    std::size_t x_len() const { return _x.size(0); }
    std::size_t y_len() const { return _y.size(0); }
};
