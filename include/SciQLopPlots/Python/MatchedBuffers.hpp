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
