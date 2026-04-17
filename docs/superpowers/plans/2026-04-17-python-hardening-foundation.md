# Python Hardening — Sub-effort 1 (Foundation) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the plumbing (validators, typed wrappers, safe-slot helper, assertion macro, exception-translation audit) that later sub-efforts apply across the C++/Python boundary. No user-visible behavior changes in callers yet.

**Architecture:** New header-only utilities under `include/SciQLopPlots/Python/`. Free-function validators throw `std::invalid_argument` / `std::out_of_range`. Shiboken typesystem translates to Python `ValueError` / `IndexError`. Validators are also exposed as callable Python free functions so they can be tested directly without wiring them into every `set_data` method yet (that is Sub-effort 2).

**Tech Stack:** C++20, Qt6, Shiboken6 bindings, Meson, pytest, numpy.

**Reference:** Design spec at `docs/superpowers/specs/2026-04-17-python-hardening-design.md`.

---

## Build and test reference

All tasks use the same dev cycle.

**Build (preferred — no reinstall):**
```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
meson compile -C build
cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
   /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/
```

**Run Python tests (from outside project dir, using SciQLop venv):**
```bash
cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest SciQLopPlots/tests/integration/test_hardening_foundation.py -v
```

**Full re-setup (only if build dir missing or meson.build changed):**
```bash
PATH="/home/jeandet/Qt/6.10.2/gcc_64/bin:/var/home/jeandet/Documents/prog/SciQLop/.venv/bin:/home/jeandet/.local/bin:/usr/bin:/usr/local/bin:$PATH" \
  meson setup build --buildtype=debugoptimized --reconfigure
```

**Git remote:** always push to `origin` (the fork), never `upstream`.

---

## File structure

**Created:**
- `include/SciQLopPlots/Assert.hpp` — `SQP_ASSERT` macro
- `include/SciQLopPlots/Python/Validation.hpp` — free-function validators
- `include/SciQLopPlots/Python/MatchedBuffers.hpp` — `MatchedXY` / `MatchedXYZ` typed wrappers
- `include/SciQLopPlots/Python/SafeSlot.hpp` — `SQP_SAFE_SLOT_*` macros + `safe_slot` template
- `tests/integration/test_hardening_foundation.py` — tests for validators and typed wrappers via Python bindings

**Modified:**
- `include/SciQLopPlots/Python/DtypeDispatch.hpp` — tighten exception type and message
- `src/PythonInterface.cpp` — make `PyBuffer::data()` throw when the buffer is invalid
- `SciQLopPlots/bindings/bindings.xml` — expose validators as Python free functions; audit exception-handling
- `SciQLopPlots/bindings/bindings.h` — include new headers
- `SciQLopPlots/meson.build` — register new headers

---

## Task 1: Add `SQP_ASSERT` macro

**Files:**
- Create: `include/SciQLopPlots/Assert.hpp`

- [ ] **Step 1: Create the header**

Write `include/SciQLopPlots/Assert.hpp`:

```cpp
#pragma once

#include <cstdio>
#include <cstdlib>

// SQP_ASSERT — debug-only invariant check.
//
// In debug builds (NDEBUG not defined): prints diagnostic and calls std::abort().
// In release builds: the expression is not evaluated, no runtime cost.
//
// Use for *internal* invariants that should never fail if the boundary-layer
// validation is correct. Do NOT use for user-input validation — throw an
// exception from Validation.hpp instead.

#ifndef NDEBUG
#define SQP_ASSERT(expr)                                                       \
    do {                                                                       \
        if (!(expr)) {                                                         \
            std::fprintf(stderr, "SQP_ASSERT failed: %s\n  at %s:%d in %s\n",  \
                         #expr, __FILE__, __LINE__, __func__);                 \
            std::abort();                                                      \
        }                                                                      \
    } while (0)
#else
#define SQP_ASSERT(expr) ((void)0)
#endif
```

- [ ] **Step 2: Smoke-compile by including from an existing translation unit**

Edit `src/Profiling.cpp` — add at the top of includes (existing includes preserved):

```cpp
#include <SciQLopPlots/Assert.hpp>
```

This proves the header parses cleanly under the project's compile flags.

- [ ] **Step 3: Build and verify**

Run:
```bash
meson compile -C build
```

Expected: build succeeds, no new warnings from `Assert.hpp`.

- [ ] **Step 4: Revert the smoke include**

Remove the `#include <SciQLopPlots/Assert.hpp>` line from `src/Profiling.cpp`. The header will be picked up organically by consumers in later tasks.

- [ ] **Step 5: Commit**

```bash
git add include/SciQLopPlots/Assert.hpp
git commit -m "feat: add SQP_ASSERT macro for internal invariants

No-op in release; abort with diagnostic in debug. Used for L3 internal
invariants where boundary validation has already run."
```

---

## Task 2: Create `Validation.hpp` with `validate_buffer`

**Files:**
- Create: `include/SciQLopPlots/Python/Validation.hpp`
- Create: `tests/integration/test_hardening_foundation.py`
- Modify: `SciQLopPlots/bindings/bindings.h`
- Modify: `SciQLopPlots/bindings/bindings.xml`
- Modify: `SciQLopPlots/meson.build`

- [ ] **Step 1: Write the failing test**

Create `tests/integration/test_hardening_foundation.py`:

```python
"""Tests for Sub-effort 1 (Foundation) of python-hardening."""
import numpy as np
import pytest
import SciQLopPlots as sqp


# ---------- validate_buffer ----------

def test_validate_buffer_none_raises_typeerror():
    with pytest.raises(TypeError):
        sqp.validate_buffer(None, "x")


def test_validate_buffer_non_numeric_raises_typeerror():
    arr = np.array(["a", "b"], dtype=object)
    with pytest.raises(TypeError):
        sqp.validate_buffer(arr, "x")


def test_validate_buffer_ok():
    arr = np.arange(10.0)
    sqp.validate_buffer(arr, "x")  # no exception


def test_validate_buffer_wrong_ndim_raises_valueerror():
    arr = np.arange(10.0)
    with pytest.raises(ValueError, match=r"x.*ndim.*1.*expected.*2"):
        sqp.validate_buffer(arr, "x", 2)


def test_validate_buffer_wrong_dtype_raises_typeerror():
    arr = np.arange(10, dtype=np.int32)
    with pytest.raises(TypeError, match=r"x.*dtype"):
        sqp.validate_buffer(arr, "x", -1, ord('d'))
```

- [ ] **Step 2: Run to verify failure**

Run:
```bash
cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/test_hardening_foundation.py -v
```
Expected: `AttributeError: module 'SciQLopPlots' has no attribute 'validate_buffer'`.

- [ ] **Step 3: Create the header**

Create `include/SciQLopPlots/Python/Validation.hpp`:

```cpp
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
```

- [ ] **Step 4: Register the header with Meson**

Edit `SciQLopPlots/meson.build` — extend the `headers` list at line 147-152:

```meson
headers = moc_headers + \
         [project_source_root+'/include/SciQLopPlots/Python/PythonInterface.hpp',
         project_source_root+'/include/SciQLopPlots/Python/Views.hpp',
         project_source_root+'/include/SciQLopPlots/Python/Validation.hpp',
         project_source_root+'/include/SciQLopPlots/constants.hpp',
         project_source_root+'/include/SciQLopPlots/Products/SubsequenceMatcher.hpp',
         project_source_root+'/include/SciQLopPlots/Products/QueryParser.hpp']
```

- [ ] **Step 5: Include the new header in `bindings.h`**

Edit `SciQLopPlots/bindings/bindings.h` — after the existing `PythonInterface.hpp` include:

```cpp
#include "SciQLopPlots/Python/Validation.hpp"
```

- [ ] **Step 6: Expose `validate_buffer` in `bindings.xml`**

Edit `SciQLopPlots/bindings/bindings.xml` — add inside the top-level `<typesystem>` block, after the `<enum-type>` declarations (around line 26):

```xml
    <namespace-type name="sqp::validation" generate="no">
        <function signature="validate_buffer(const PyBuffer&amp;, std::string_view, int, char)"
                  rename="validate_buffer"/>
    </namespace-type>
```

Then, near the bottom of the file (wherever other top-level injected helpers live), add:

```xml
    <add-function signature="validate_buffer(PyBuffer, std::string, int, char)" return-type="void">
        <inject-code class="target" position="beginning">
            sqp::validation::validate_buffer(%1, %2, %3, %4);
        </inject-code>
        <modify-argument index="3">
            <default-value>-1</default-value>
        </modify-argument>
        <modify-argument index="4">
            <default-value>0</default-value>
        </modify-argument>
    </add-function>
```

**Note on `std::string_view` vs `std::string`:** Shiboken has better out-of-the-box support for `std::string`; the Python-callable wrapper takes `std::string` and the C++ function takes it by `string_view` (implicit conversion). No data is copied beyond the name string itself.

- [ ] **Step 7: Build and run the test**

Run:
```bash
meson compile -C build && \
  cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
     /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/ && \
  cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/test_hardening_foundation.py -v
```
Expected: the 5 `validate_buffer` tests pass.

- [ ] **Step 8: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add include/SciQLopPlots/Python/Validation.hpp \
        SciQLopPlots/meson.build \
        SciQLopPlots/bindings/bindings.h \
        SciQLopPlots/bindings/bindings.xml \
        tests/integration/test_hardening_foundation.py
git commit -m "feat(python): add validate_buffer and expose to Python

First boundary validator. Throws std::invalid_argument on bad dtype, ndim,
or null buffer; Shiboken translates to Python ValueError / TypeError."
```

---

## Task 3: Add `validate_index`

**Files:**
- Modify: `include/SciQLopPlots/Python/Validation.hpp`
- Modify: `SciQLopPlots/bindings/bindings.xml`
- Modify: `tests/integration/test_hardening_foundation.py`

- [ ] **Step 1: Append failing tests**

Append to `tests/integration/test_hardening_foundation.py`:

```python
# ---------- validate_index ----------

def test_validate_index_ok():
    sqp.validate_index(0, 10, "row")
    sqp.validate_index(9, 10, "row")


def test_validate_index_negative_raises_indexerror():
    with pytest.raises(IndexError, match=r"row.*negative"):
        sqp.validate_index(-1, 10, "row")


def test_validate_index_out_of_range_raises_indexerror():
    with pytest.raises(IndexError, match=r"row.*out of range.*size 10"):
        sqp.validate_index(10, 10, "row")


def test_validate_index_empty_container_always_raises():
    with pytest.raises(IndexError):
        sqp.validate_index(0, 0, "row")
```

- [ ] **Step 2: Run to verify failure**

```bash
cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/test_hardening_foundation.py::test_validate_index_ok -v
```
Expected: `AttributeError: module 'SciQLopPlots' has no attribute 'validate_index'`.

- [ ] **Step 3: Implement `validate_index`**

Append inside the `sqp::validation` namespace in `Validation.hpp`:

```cpp
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
```

**Signature rationale:** `long long` accepts Python's arbitrary-size ints transparently (up to 64-bit). `qsizetype` is a typedef of `ptrdiff_t` (signed), compatible with `long long` on all supported platforms.

- [ ] **Step 4: Expose in `bindings.xml`**

Add next to the `validate_buffer` `<add-function>` block:

```xml
    <add-function signature="validate_index(long long, long long, std::string)" return-type="void">
        <inject-code class="target" position="beginning">
            sqp::validation::validate_index(%1, %2, %3);
        </inject-code>
    </add-function>
```

- [ ] **Step 5: Build and run tests**

```bash
meson compile -C build && \
  cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
     /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/ && \
  cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/test_hardening_foundation.py -v
```
Expected: all prior tests still pass plus the 4 new `validate_index` tests.

**Shiboken translation note:** `std::out_of_range` is a subclass of `std::exception` and Shiboken translates it to Python `IndexError` via the default type-system exception mapping. If the mapping surfaces `RuntimeError` instead, extend the `exception-handling` configuration — see Task 11.

- [ ] **Step 6: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add include/SciQLopPlots/Python/Validation.hpp \
        SciQLopPlots/bindings/bindings.xml \
        tests/integration/test_hardening_foundation.py
git commit -m "feat(python): add validate_index boundary validator

Rejects negative and out-of-range indices with std::out_of_range
(Python IndexError)."
```

---

## Task 4: Add `validate_same_length`, `validate_xy`, `validate_xyz`

**Files:**
- Modify: `include/SciQLopPlots/Python/Validation.hpp`
- Modify: `SciQLopPlots/bindings/bindings.xml`
- Modify: `tests/integration/test_hardening_foundation.py`

- [ ] **Step 1: Append failing tests**

Append to `tests/integration/test_hardening_foundation.py`:

```python
# ---------- validate_same_length ----------

def test_validate_same_length_ok():
    x = np.arange(10.0)
    y = np.arange(10.0)
    sqp.validate_same_length(x, "x", y, "y")


def test_validate_same_length_mismatch_raises_valueerror():
    x = np.arange(10.0)
    y = np.arange(5.0)
    with pytest.raises(ValueError, match=r"length.*x.*10.*y.*5"):
        sqp.validate_same_length(x, "x", y, "y")


# ---------- validate_xy ----------

def test_validate_xy_1d_ok():
    x = np.arange(10.0)
    y = np.arange(10.0)
    sqp.validate_xy(x, y)


def test_validate_xy_2d_y_ok():
    x = np.arange(10.0)
    y = np.random.rand(10, 3)
    sqp.validate_xy(x, y)


def test_validate_xy_x_not_1d_raises():
    x = np.random.rand(10, 3)
    y = np.arange(10.0)
    with pytest.raises(ValueError, match=r"x.*ndim"):
        sqp.validate_xy(x, y)


def test_validate_xy_shape_mismatch_raises():
    x = np.arange(10.0)
    y = np.arange(5.0)
    with pytest.raises(ValueError, match=r"length"):
        sqp.validate_xy(x, y)


def test_validate_xy_y_3d_raises():
    x = np.arange(10.0)
    y = np.random.rand(10, 3, 2)
    with pytest.raises(ValueError, match=r"y.*ndim"):
        sqp.validate_xy(x, y)


# ---------- validate_xyz ----------

def test_validate_xyz_ok():
    x = np.arange(10.0)
    y = np.arange(5.0)
    z = np.random.rand(10, 5)
    sqp.validate_xyz(x, y, z)


def test_validate_xyz_z_wrong_shape_raises():
    x = np.arange(10.0)
    y = np.arange(5.0)
    z = np.random.rand(9, 5)  # wrong x dimension
    with pytest.raises(ValueError, match=r"z"):
        sqp.validate_xyz(x, y, z)
```

- [ ] **Step 2: Run to verify failure**

```bash
cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/test_hardening_foundation.py -v -k "same_length or xy"
```
Expected: new tests fail with `AttributeError`.

- [ ] **Step 3: Implement the three validators**

Append inside `sqp::validation` in `Validation.hpp`:

```cpp
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
    validate_buffer(y, "y");  // numeric, any ndim already checked above
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
```

- [ ] **Step 4: Expose all three in `bindings.xml`**

Add next to the other `<add-function>` blocks:

```xml
    <add-function signature="validate_same_length(PyBuffer, std::string, PyBuffer, std::string)" return-type="void">
        <inject-code class="target" position="beginning">
            sqp::validation::validate_same_length(%1, %2, %3, %4);
        </inject-code>
    </add-function>
    <add-function signature="validate_xy(PyBuffer, PyBuffer)" return-type="void">
        <inject-code class="target" position="beginning">
            sqp::validation::validate_xy(%1, %2);
        </inject-code>
    </add-function>
    <add-function signature="validate_xyz(PyBuffer, PyBuffer, PyBuffer)" return-type="void">
        <inject-code class="target" position="beginning">
            sqp::validation::validate_xyz(%1, %2, %3);
        </inject-code>
    </add-function>
```

- [ ] **Step 5: Build and run tests**

```bash
meson compile -C build && \
  cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
     /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/ && \
  cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/test_hardening_foundation.py -v
```
Expected: all tests pass.

- [ ] **Step 6: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add include/SciQLopPlots/Python/Validation.hpp \
        SciQLopPlots/bindings/bindings.xml \
        tests/integration/test_hardening_foundation.py
git commit -m "feat(python): add validate_same_length, validate_xy, validate_xyz

Cover the dominant set_data argument-shape patterns across plottables."
```

---

## Task 5: Add `validate_nd_list` and `validate_finite`

**Files:**
- Modify: `include/SciQLopPlots/Python/Validation.hpp`
- Modify: `SciQLopPlots/bindings/bindings.xml`
- Modify: `tests/integration/test_hardening_foundation.py`

- [ ] **Step 1: Append failing tests**

Append to `tests/integration/test_hardening_foundation.py`:

```python
# ---------- validate_nd_list ----------

def test_validate_nd_list_ok():
    arrs = [np.arange(10.0), np.arange(10.0), np.arange(10.0)]
    sqp.validate_nd_list(arrs, 3)


def test_validate_nd_list_wrong_count_raises():
    arrs = [np.arange(10.0), np.arange(10.0)]
    with pytest.raises(ValueError, match=r"expected 3.*got 2"):
        sqp.validate_nd_list(arrs, 3)


def test_validate_nd_list_length_mismatch_raises():
    arrs = [np.arange(10.0), np.arange(5.0), np.arange(10.0)]
    with pytest.raises(ValueError, match=r"length"):
        sqp.validate_nd_list(arrs, 3)


# ---------- validate_finite ----------

def test_validate_finite_ok():
    sqp.validate_finite(1.0, "v")
    sqp.validate_finite(-1e9, "v")


def test_validate_finite_nan_raises():
    with pytest.raises(ValueError, match=r"v.*finite"):
        sqp.validate_finite(float("nan"), "v")


def test_validate_finite_inf_raises():
    with pytest.raises(ValueError, match=r"v.*finite"):
        sqp.validate_finite(float("inf"), "v")
```

- [ ] **Step 2: Run to verify failure**

```bash
cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/test_hardening_foundation.py -v -k "nd_list or finite"
```
Expected: new tests fail with `AttributeError`.

- [ ] **Step 3: Implement both validators**

Append inside `sqp::validation` in `Validation.hpp`:

```cpp
inline void validate_nd_list(const QList<PyBuffer>& bufs, std::size_t expected_count)
{
    if (static_cast<std::size_t>(bufs.size()) != expected_count)
    {
        std::ostringstream os;
        os << "buffer list: expected " << expected_count
           << " elements, got " << bufs.size();
        throw std::invalid_argument(os.str());
    }
    if (bufs.isEmpty())
        return;
    const auto ref_len = bufs.front().size(0);
    for (qsizetype i = 0; i < bufs.size(); ++i)
    {
        validate_buffer(bufs[i], "buffers[" + std::to_string(i) + "]");
        if (bufs[i].size(0) != ref_len)
        {
            std::ostringstream os;
            os << "buffer list: length mismatch at index " << i
               << " (got " << bufs[i].size(0) << ", expected " << ref_len << ")";
            throw std::invalid_argument(os.str());
        }
    }
}

inline void validate_finite(double v, std::string_view name)
{
    if (!std::isfinite(v))
    {
        std::ostringstream os;
        os << name << ": value must be finite (got " << v << ")";
        throw std::invalid_argument(os.str());
    }
}
```

Note: the `validate_buffer` call inside `validate_nd_list` uses a `std::string` temporary for the name; `string_view` binds to it via implicit conversion. Kept inline for clarity — list lengths here are small (typically 2–4 buffers).

- [ ] **Step 4: Expose in `bindings.xml`**

```xml
    <add-function signature="validate_nd_list(QList&lt;PyBuffer&gt;, std::size_t)" return-type="void">
        <inject-code class="target" position="beginning">
            sqp::validation::validate_nd_list(%1, %2);
        </inject-code>
    </add-function>
    <add-function signature="validate_finite(double, std::string)" return-type="void">
        <inject-code class="target" position="beginning">
            sqp::validation::validate_finite(%1, %2);
        </inject-code>
    </add-function>
```

- [ ] **Step 5: Build and run tests**

```bash
meson compile -C build && \
  cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
     /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/ && \
  cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/test_hardening_foundation.py -v
```
Expected: all tests pass.

- [ ] **Step 6: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add include/SciQLopPlots/Python/Validation.hpp \
        SciQLopPlots/bindings/bindings.xml \
        tests/integration/test_hardening_foundation.py
git commit -m "feat(python): add validate_nd_list and validate_finite

Completes the free-function validator surface for Sub-effort 1."
```

---

## Task 6: Create `MatchedBuffers.hpp` with `MatchedXY`

**Files:**
- Create: `include/SciQLopPlots/Python/MatchedBuffers.hpp`
- Modify: `SciQLopPlots/bindings/bindings.h`
- Modify: `SciQLopPlots/bindings/bindings.xml`
- Modify: `SciQLopPlots/meson.build`
- Modify: `tests/integration/test_hardening_foundation.py`

- [ ] **Step 1: Write failing tests**

Append to `tests/integration/test_hardening_foundation.py` (use method-call syntax for `rows()` / `cols()` — they are bound as methods, not attributes):

```python
# ---------- MatchedXY ----------

def test_matched_xy_1d_y():
    x = np.arange(10.0)
    y = np.arange(10.0)
    m = sqp.MatchedXY.from_py(x, y)
    assert m.rows() == 10
    assert m.cols() == 1


def test_matched_xy_2d_y():
    x = np.arange(10.0)
    y = np.random.rand(10, 4)
    m = sqp.MatchedXY.from_py(x, y)
    assert m.rows() == 10
    assert m.cols() == 4


def test_matched_xy_mismatch_raises():
    x = np.arange(10.0)
    y = np.arange(5.0)
    with pytest.raises(ValueError):
        sqp.MatchedXY.from_py(x, y)
```

- [ ] **Step 2: Run to verify failure**

```bash
cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/test_hardening_foundation.py::test_matched_xy_1d_y -v
```
Expected: `AttributeError: module 'SciQLopPlots' has no attribute 'MatchedXY'`.

- [ ] **Step 3: Create the header**

Create `include/SciQLopPlots/Python/MatchedBuffers.hpp`:

```cpp
#pragma once

#include <SciQLopPlots/Python/PythonInterface.hpp>
#include <SciQLopPlots/Python/Validation.hpp>

#include <utility>

// MatchedXY / MatchedXYZ live in the global namespace to match the rest of
// the project's public class surface (SciQLop*, PyBuffer, GetDataPyCallable,
// etc.) and to simplify Shiboken value-type declarations.

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
```

- [ ] **Step 4: Register with Meson**

Edit `SciQLopPlots/meson.build`, adding `MatchedBuffers.hpp` next to `Validation.hpp`:

```meson
headers = moc_headers + \
         [project_source_root+'/include/SciQLopPlots/Python/PythonInterface.hpp',
         project_source_root+'/include/SciQLopPlots/Python/Views.hpp',
         project_source_root+'/include/SciQLopPlots/Python/Validation.hpp',
         project_source_root+'/include/SciQLopPlots/Python/MatchedBuffers.hpp',
         project_source_root+'/include/SciQLopPlots/constants.hpp',
         project_source_root+'/include/SciQLopPlots/Products/SubsequenceMatcher.hpp',
         project_source_root+'/include/SciQLopPlots/Products/QueryParser.hpp']
```

- [ ] **Step 5: Include in `bindings.h`**

Edit `SciQLopPlots/bindings/bindings.h`, after the Validation include:

```cpp
#include "SciQLopPlots/Python/MatchedBuffers.hpp"
```

- [ ] **Step 6: Expose in `bindings.xml`**

Add inside the `<typesystem>` block (next to other `<value-type>` declarations):

```xml
    <value-type name="MatchedXY"/>
```

This exposes `MatchedXY` to Python. `from_py` shows up as a classmethod-style call (`SciQLopPlots.MatchedXY.from_py(x, y)`). The default constructor stays in Python (returns an empty wrapper) — leaving it avoids fighting Shiboken's internal construction paths; misuse is harmless since `rows()` / `cols()` on an empty wrapper return 0 and `x()` / `y()` return invalid PyBuffers.

- [ ] **Step 7: Build and run tests**

```bash
meson compile -C build && \
  cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
     /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/ && \
  cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/test_hardening_foundation.py -v
```
Expected: all tests pass, including the three MatchedXY tests.

- [ ] **Step 8: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add include/SciQLopPlots/Python/MatchedBuffers.hpp \
        SciQLopPlots/meson.build \
        SciQLopPlots/bindings/bindings.h \
        SciQLopPlots/bindings/bindings.xml \
        tests/integration/test_hardening_foundation.py
git commit -m "feat(python): add MatchedXY typed wrapper

Zero-copy typed wrapper for (x, y) argument pairs. Constructor validates
once; downstream code receives a known-good object."
```

---

## Task 7: Add `MatchedXYZ`

**Files:**
- Modify: `include/SciQLopPlots/Python/MatchedBuffers.hpp`
- Modify: `SciQLopPlots/bindings/bindings.xml`
- Modify: `tests/integration/test_hardening_foundation.py`

- [ ] **Step 1: Append failing tests**

Append to `tests/integration/test_hardening_foundation.py`:

```python
# ---------- MatchedXYZ ----------

def test_matched_xyz_ok():
    x = np.arange(10.0)
    y = np.arange(5.0)
    z = np.random.rand(10, 5)
    m = sqp.MatchedXYZ.from_py(x, y, z)
    assert m.x_len() == 10
    assert m.y_len() == 5


def test_matched_xyz_mismatch_raises():
    x = np.arange(10.0)
    y = np.arange(5.0)
    z = np.random.rand(9, 5)
    with pytest.raises(ValueError):
        sqp.MatchedXYZ.from_py(x, y, z)
```

- [ ] **Step 2: Run to verify failure**

```bash
cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/test_hardening_foundation.py::test_matched_xyz_ok -v
```
Expected: `AttributeError`.

- [ ] **Step 3: Implement `MatchedXYZ`**

Append to `include/SciQLopPlots/Python/MatchedBuffers.hpp` (same global namespace as `MatchedXY`):

```cpp
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
```

- [ ] **Step 4: Expose in `bindings.xml`**

Add next to the `MatchedXY` `<value-type>`:

```xml
    <value-type name="MatchedXYZ"/>
```

- [ ] **Step 5: Build and run tests**

```bash
meson compile -C build && \
  cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
     /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/ && \
  cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/test_hardening_foundation.py -v
```
Expected: all tests pass.

- [ ] **Step 6: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add include/SciQLopPlots/Python/MatchedBuffers.hpp \
        SciQLopPlots/bindings/bindings.xml \
        tests/integration/test_hardening_foundation.py
git commit -m "feat(python): add MatchedXYZ typed wrapper

Zero-copy typed wrapper for the (x, y, z) colormap argument pattern."
```

---

## Task 8: Create `SafeSlot.hpp`

**Files:**
- Create: `include/SciQLopPlots/Python/SafeSlot.hpp`
- Modify: `SciQLopPlots/meson.build`

- [ ] **Step 1: Create the header**

Create `include/SciQLopPlots/Python/SafeSlot.hpp`:

```cpp
#pragma once

#include <QDebug>

#include <exception>
#include <string_view>
#include <utility>

// SQP_SAFE_SLOT_BEGIN / SQP_SAFE_SLOT_END — try/catch guards for Qt slot
// lambda bodies. Prevents uncaught C++ exceptions from propagating into the
// Qt event loop (which would call std::terminate).
//
// Usage:
//   connect(sender, &Sender::sig, this, [this](int x) {
//       SQP_SAFE_SLOT_BEGIN
//       do_work(x);
//       SQP_SAFE_SLOT_END("MyClass::on_sig")
//   });

#define SQP_SAFE_SLOT_BEGIN try {
#define SQP_SAFE_SLOT_END(ctx)                                                 \
    }                                                                          \
    catch (const std::exception& _sqp_e)                                       \
    {                                                                          \
        qWarning() << "[safe_slot]" << (ctx) << ":" << _sqp_e.what();          \
    }                                                                          \
    catch (...)                                                                \
    {                                                                          \
        qWarning() << "[safe_slot]" << (ctx) << ": unknown exception";         \
    }

namespace sqp {

// safe_slot — higher-order wrapper for non-macro call sites. Wraps a callable
// so it swallows + logs any exceptions it throws. Return type of f is
// discarded; returns void.
template <typename F>
auto safe_slot(F&& f, std::string_view context = "slot")
{
    return [fn = std::forward<F>(f), ctx = std::string(context)]
           (auto&&... args) mutable -> void {
        try
        {
            fn(std::forward<decltype(args)>(args)...);
        }
        catch (const std::exception& e)
        {
            qWarning() << "[safe_slot]" << ctx.c_str() << ":" << e.what();
        }
        catch (...)
        {
            qWarning() << "[safe_slot]" << ctx.c_str() << ": unknown exception";
        }
    };
}

} // namespace sqp
```

- [ ] **Step 2: Register with Meson**

Edit `SciQLopPlots/meson.build`, adding `SafeSlot.hpp` next to the other Python headers:

```meson
headers = moc_headers + \
         [project_source_root+'/include/SciQLopPlots/Python/PythonInterface.hpp',
         project_source_root+'/include/SciQLopPlots/Python/Views.hpp',
         project_source_root+'/include/SciQLopPlots/Python/Validation.hpp',
         project_source_root+'/include/SciQLopPlots/Python/MatchedBuffers.hpp',
         project_source_root+'/include/SciQLopPlots/Python/SafeSlot.hpp',
         project_source_root+'/include/SciQLopPlots/constants.hpp',
         project_source_root+'/include/SciQLopPlots/Products/SubsequenceMatcher.hpp',
         project_source_root+'/include/SciQLopPlots/Products/QueryParser.hpp']
```

- [ ] **Step 3: Smoke-compile the header**

Add a temporary include in `src/Profiling.cpp`:

```cpp
#include <SciQLopPlots/Python/SafeSlot.hpp>
```

Run:
```bash
meson compile -C build
```
Expected: compiles clean. Then revert the include.

- [ ] **Step 4: Add a compile-only test exercise**

Append to `src/Profiling.cpp` inside an anonymous namespace (if one doesn't exist, create one at top of file after includes):

```cpp
namespace {

[[maybe_unused]] void _compile_test_safe_slot()
{
    auto guarded = sqp::safe_slot([](int x) { (void)x; }, "test");
    guarded(42);
}

} // namespace
```

Run:
```bash
meson compile -C build
```
Expected: compiles clean.

- [ ] **Step 5: Revert the compile-test**

Remove the `_compile_test_safe_slot` function from `src/Profiling.cpp`. The header is picked up by Sub-effort 3's slot-audit work.

- [ ] **Step 6: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add include/SciQLopPlots/Python/SafeSlot.hpp SciQLopPlots/meson.build
git commit -m "feat(python): add SafeSlot try/catch helpers

SQP_SAFE_SLOT_BEGIN/END macros and sqp::safe_slot template wrap Qt slot
bodies to prevent C++ exceptions from reaching the event loop. Consumed
by Sub-effort 3 slot audit."
```

---

## Task 9: `PyBuffer::data()` throws on invalid buffer

**Files:**
- Modify: `src/PythonInterface.cpp`
- Modify: `tests/integration/test_hardening_foundation.py`

- [ ] **Step 1: Append a failing test**

Append to `tests/integration/test_hardening_foundation.py`:

```python
# ---------- PyBuffer::data() invalidity (exercised via validate_buffer) ----------
# Direct C++ access to PyBuffer::data() is not exposed to Python, so this
# is a compile-level contract verified by other suites. The negative path
# (validate_buffer on a None object) already covers the "invalid buffer"
# branch end-to-end.

def test_validate_buffer_rejects_none_cleanly():
    # PyBuffer from None is the "invalid" case; validate_buffer must raise,
    # not segfault, even though internally PyBuffer::data() would throw on
    # a hypothetical downstream call.
    with pytest.raises(TypeError):
        sqp.validate_buffer(None, "x")
```

This test was already present after Task 2 (`test_validate_buffer_none_raises_typeerror`). The purpose of this task is to fix the internal path: previously `PyBuffer::data()` returned `nullptr` on an invalid buffer, leaving a dereference-trap for any caller that forgot to check. We change it to throw so such callers get an exception instead.

- [ ] **Step 2: Read the current `PyBuffer::data()` implementation**

Read `src/PythonInterface.cpp` lines 403-413. Current body:

```cpp
double* PyBuffer::data() const
{
    if (is_valid())
    {
        if (_impl->buffer.format[0] != 'd')
            throw std::runtime_error(
                "PyBuffer::data() called on non-double buffer; use raw_data() + format_code()");
        return reinterpret_cast<double*>(this->_impl->buffer.buf);
    }
    return nullptr;
}
```

- [ ] **Step 3: Replace the nullptr return with a throw**

Edit `src/PythonInterface.cpp`, replace lines 403-413 with:

```cpp
double* PyBuffer::data() const
{
    if (!is_valid())
        throw std::runtime_error("PyBuffer::data() called on invalid buffer");
    if (_impl->buffer.format[0] != 'd')
        throw std::runtime_error(
            "PyBuffer::data() called on non-double buffer; use raw_data() + format_code()");
    return reinterpret_cast<double*>(this->_impl->buffer.buf);
}
```

- [ ] **Step 4: Audit existing callers for the `nullptr` contract**

Run:
```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
```

Check all call sites in C++:
```bash
```
(Use `Grep` tool for `\.data\(\)` in `src/` with glob `*.cpp`.)

For each hit, verify the caller either:
- calls `is_valid()` first (safe under the new contract too), **or**
- unconditionally dereferences the result (was a latent nullptr-deref bug; is now an exception — `set_data` paths in plottables already check `x.is_valid()` so they're unaffected).

**Record findings in the commit message.** No code changes in callers expected; the change is a strengthening of the contract, not a breaking one.

- [ ] **Step 5: Build and run the full test suite**

```bash
meson compile -C build && \
  cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
     /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/ && \
  cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/ -v
```
Expected: every integration test passes (not just the hardening file — this change affects general code paths).

- [ ] **Step 6: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add src/PythonInterface.cpp tests/integration/test_hardening_foundation.py
git commit -m "fix(python): PyBuffer::data() throws on invalid buffer

Previously returned nullptr, leaving a dereference trap for callers who
forgot to check is_valid(). Now throws std::runtime_error. Audited
existing call sites: all guarded by is_valid() or not reachable with an
invalid buffer."
```

---

## Task 10: Tighten `dispatch_dtype` exception

**Files:**
- Modify: `include/SciQLopPlots/Python/DtypeDispatch.hpp`
- Modify: `tests/integration/test_hardening_foundation.py`

- [ ] **Step 1: Append a failing test**

Append to `tests/integration/test_hardening_foundation.py`:

```python
# ---------- dispatch_dtype error surface (via validate_buffer) ----------

def test_validate_buffer_rejects_float16():
    # float16 is not currently supported by dispatch_dtype. validate_buffer's
    # numeric check should reject it with TypeError (format code 'e' not in
    # our numeric_formats whitelist), before reaching dispatch.
    arr = np.arange(10, dtype=np.float16)
    with pytest.raises(TypeError):
        sqp.validate_buffer(arr, "x")
```

- [ ] **Step 2: Improve the exception**

Edit `include/SciQLopPlots/Python/DtypeDispatch.hpp`. Replace line 23 (`default: throw std::runtime_error("Unsupported numpy dtype");`) with:

```cpp
        default:
        {
            std::string msg = "Unsupported numpy dtype format code: '";
            if (format_code == '\0')
                msg += "\\0";
            else
                msg += format_code;
            msg += "'";
            throw std::invalid_argument(msg);
        }
```

Add `#include <string>` to the top-of-file includes.

**Why `invalid_argument` instead of `runtime_error`:** maps to `ValueError` in Python, which better communicates "your input is wrong" vs `RuntimeError` (internal failure).

- [ ] **Step 3: Build and run tests**

```bash
meson compile -C build && \
  cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
     /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/ && \
  cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/test_hardening_foundation.py -v
```
Expected: all tests pass, including the new float16 test.

- [ ] **Step 4: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add include/SciQLopPlots/Python/DtypeDispatch.hpp \
        tests/integration/test_hardening_foundation.py
git commit -m "fix(python): dispatch_dtype throws invalid_argument with code

Improves exception type (runtime_error -> invalid_argument, mapping to
Python ValueError) and the message (includes the actual format code)."
```

---

## Task 11: `bindings.xml` exception-translation audit

**Goal:** Verify that every exception type used in Validation.hpp and DtypeDispatch.hpp translates to the intended Python exception. If any maps unexpectedly, add an `exception-handling` / type-system mapping so the intent holds.

**Files:**
- Modify (if needed): `SciQLopPlots/bindings/bindings.xml`
- Modify: `tests/integration/test_hardening_foundation.py`

- [ ] **Step 1: Add a consolidated exception-taxonomy test**

Append to `tests/integration/test_hardening_foundation.py`:

```python
# ---------- Exception taxonomy (translation audit) ----------

import builtins

@pytest.mark.parametrize("call,exc_cls", [
    (lambda: sqp.validate_buffer(None, "x"),                     TypeError),
    (lambda: sqp.validate_buffer(np.arange(10.0), "x", 2),       ValueError),
    (lambda: sqp.validate_index(-1, 10, "i"),                    IndexError),
    (lambda: sqp.validate_index(10, 10, "i"),                    IndexError),
    (lambda: sqp.validate_xy(np.arange(10.0), np.arange(5.0)),   ValueError),
    (lambda: sqp.validate_finite(float("nan"), "v"),             ValueError),
])
def test_exception_taxonomy(call, exc_cls):
    with pytest.raises(exc_cls):
        call()
```

- [ ] **Step 2: Run the test to find any mismatch**

```bash
meson compile -C build && \
  cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
     /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/ && \
  cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/test_hardening_foundation.py::test_exception_taxonomy -v
```

Expected (in the common case): all pass. If any row fails because the actual exception is `RuntimeError`, Shiboken's default mapping is collapsing our specific C++ exception types into `RuntimeError`. In that case proceed to Step 3; otherwise skip to Step 4.

- [ ] **Step 3 (conditional): Add explicit exception mapping to `bindings.xml`**

If Step 2 showed `RuntimeError` where we expect `ValueError` / `IndexError` / `TypeError`, edit the `<add-function>` entries for the affected validators to include an explicit `inject-code` rewrite that catches and re-raises with the intended Python type. Example for `validate_buffer`:

```xml
    <add-function signature="validate_buffer(PyBuffer, std::string, int, char)" return-type="void">
        <inject-code class="target" position="beginning">
            try {
                sqp::validation::validate_buffer(%1, %2, %3, %4);
            } catch (const std::invalid_argument& e) {
                PyErr_SetString(PyExc_ValueError, e.what());
                return nullptr;
            }
        </inject-code>
        <modify-argument index="3">
            <default-value>-1</default-value>
        </modify-argument>
        <modify-argument index="4">
            <default-value>0</default-value>
        </modify-argument>
    </add-function>
```

Apply the corresponding pattern to `validate_index` (`std::out_of_range` → `PyExc_IndexError`), `validate_xy`, `validate_xyz`, `validate_same_length`, `validate_nd_list`, `validate_finite` (`std::invalid_argument` → `PyExc_ValueError`). For null-buffer cases the test expects `TypeError`: add a `catch (const std::invalid_argument& e) where e.what() starts with "…: invalid or null buffer"` path that raises `PyExc_TypeError`. Simplest implementation: split the null-check out of `validate_buffer` into `validate_buffer_non_null` which throws `std::domain_error`, and map that to `TypeError`.

(If no remapping is needed because Shiboken's default mapping already does the right thing, this step is a no-op.)

- [ ] **Step 4: Re-run the test suite**

```bash
meson compile -C build && \
  cp build/SciQLopPlots/SciQLopPlotsBindings.cpython-313-x86_64-linux-gnu.so \
     /var/home/jeandet/Documents/prog/SciQLop/.venv/lib/python3.13/site-packages/SciQLopPlots/ && \
  cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/test_hardening_foundation.py -v
```
Expected: every test in the file passes.

- [ ] **Step 5: Commit**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git add tests/integration/test_hardening_foundation.py \
        SciQLopPlots/bindings/bindings.xml
git commit -m "feat(python): exception-translation audit for hardening foundation

Parametrized taxonomy test pins TypeError / ValueError / IndexError to
specific validator failure modes. bindings.xml updated with explicit
exception mapping where Shiboken's default collapsed to RuntimeError."
```

---

## Task 12: Run full integration suite + push branch

- [ ] **Step 1: Run the full integration suite**

```bash
cd /var/home/jeandet/Documents/prog && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  SciQLop/.venv/bin/python -m pytest \
    SciQLopPlots/tests/integration/ -v
```
Expected: all existing tests still pass plus the new hardening tests.

- [ ] **Step 2: Run the unit suite**

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots && \
  LD_LIBRARY_PATH=~/Qt/6.10.2/gcc_64/lib \
  /var/home/jeandet/Documents/prog/SciQLop/.venv/bin/python -m pytest tests/unit/ -v
```
Expected: all pass.

- [ ] **Step 3: Push the branch to the fork**

Create and push a feature branch:

```bash
cd /var/home/jeandet/Documents/prog/SciQLopPlots
git checkout -b feature/python-hardening-foundation
git push -u origin feature/python-hardening-foundation
```

- [ ] **Step 4: Confirm done**

All 12 tasks completed means Sub-effort 1 plumbing is merged-ready:
- Headers (`Assert.hpp`, `Validation.hpp`, `MatchedBuffers.hpp`, `SafeSlot.hpp`) exist and are reachable from bindings.
- `PyBuffer::data()` throws on invalid buffers.
- `dispatch_dtype` throws `invalid_argument` with a clear message.
- Validators are callable from Python and have a passing test file (`tests/integration/test_hardening_foundation.py`).
- Exception taxonomy is pinned by the parametrized test.

No caller wiring changed — that is Sub-effort 2's scope.
