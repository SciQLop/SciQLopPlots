# Python-side crash-hardening — design

**Date:** 2026-04-17
**Status:** Draft

## 1. The contract

**Invariant.** Any Python call into SciQLopPlots — regardless of argument
validity, object state, or user-supplied callback behavior — must either
succeed, be a no-op, or raise a Python exception. **It must never segfault,
abort, or invoke undefined behavior.**

### In scope

- All classes exposed via `SciQLopPlots/bindings/bindings.xml` (plots, panels,
  plottables, items, axes, spans, collections, overlays).
- The `plot()` dispatcher in `SciQLopPlots/__init__.py`.
- User-supplied `get_data` callbacks — C++ validates the values they return.
- Reactive `.on` pipelines, Inspector / PropertiesView, Products view — any
  surface reachable from `import SciQLopPlots`.

### Out of scope (initial release)

- Memory leaks, deadlocks, performance pathology — not segfaults.
- OpenGL driver / Qt platform-plugin crashes.
- Internal C++ bugs reachable only from within the library; these are
  covered by debug-mode assertions, not the public contract.

### Trade-offs

- Validation at entry is O(1) per call (format / ndim / size / contiguity /
  shape-match). Negligible next to any real data operation.
- We replace today's "silent no-op on bad data" with "exception with
  descriptive message." Some internal callers may rely on the silent-no-op
  behavior; we sweep the codebase.

## 2. Architecture

Four layers, each with a single responsibility. Validation lives at exactly
one of them per code path.

```
Python user code
      │
      ▼
┌─────────────────────────────────────┐
│ L1: Python dispatch (__init__.py)   │  Validates arg count / graph_type;
│     plot() dispatcher               │  pre-Shiboken ergonomic errors.
└─────────────────────────────────────┘
      │
      ▼
┌─────────────────────────────────────┐
│ L2: C++ API boundary                │  Every method taking PyBuffer /
│     set_data, add_*, ranges, etc.   │  PyObject validates at entry.
│                                     │  Typed wrappers MatchedXY / XYZ
│                                     │  for the dominant xy(z) pattern.
└─────────────────────────────────────┘
      │ (validated, known-good)
      ▼
┌─────────────────────────────────────┐
│ L3: C++ internals                   │  No re-validation. Trust the
│     resamplers, data sources        │  boundary. Debug-mode asserts only.
└─────────────────────────────────────┘
      ▲
      │  (results from Python callable)
┌─────────────────────────────────────┐
│ L4: Callback return boundary        │  GetDataPyCallable validates the
│     worker thread ← get_data()      │  list/tuple, each buffer's dtype,
│                                     │  ndim, and cross-array shape match
│                                     │  before constructing PyBuffers.
└─────────────────────────────────────┘
```

### Cross-cutting concerns (all live at L2)

- **Exception translation** — Shiboken typesystem maps
  `std::runtime_error` / `std::invalid_argument` / `std::out_of_range` /
  `std::bad_alloc` to `RuntimeError` / `ValueError` / `IndexError` /
  `MemoryError`. `bindings.xml` is audited for every bound method.
- **Signal-handler safety** — a `safe_slot` helper wraps Qt slot bodies in
  try/catch so C++ exceptions can't cross back into the Qt event loop.
  Exceptions inside slots are logged and swallowed.
- **Object lifetime safety** — Shiboken's wrapper invalidation handles
  Python → C++ calls on deleted QObjects. C++-internal non-owning
  `QObject*` members are converted to `QPointer<T>` and null-checked.
- **Index safety** — every bound method taking an integer index
  bounds-checks at entry; negative indices are rejected, not wrapped.

### New headers

- `include/SciQLopPlots/Python/Validation.hpp`
- `include/SciQLopPlots/Python/MatchedBuffers.hpp`
- `include/SciQLopPlots/Python/SafeSlot.hpp`

## 3. Components

### 3.1 `Validation.hpp`

Free functions, no state. Throw `std::invalid_argument` /
`std::out_of_range`, translated to Python `ValueError` / `IndexError` by
Shiboken.

```cpp
void validate_buffer(const PyBuffer& b, std::string_view name,
                     int expected_ndim = -1,   // -1 = any
                     char expected_dtype = 0); // 0 = any numeric

void validate_same_length(const PyBuffer& a, std::string_view name_a,
                          const PyBuffer& b, std::string_view name_b);

void validate_xy(const PyBuffer& x, const PyBuffer& y);
void validate_xyz(const PyBuffer& x, const PyBuffer& y, const PyBuffer& z);
void validate_nd_list(const QList<PyBuffer>& bufs, std::size_t expected_count);

void validate_index(qsizetype i, qsizetype size, std::string_view name);
void validate_finite(double v, std::string_view name);
```

Error messages follow the form
`"<method>: <param>: <what was wrong> (got <actual>, expected <spec>)"`.

### 3.2 `MatchedBuffers.hpp`

Typed wrappers for the dominant xy / xyz pattern. Construction =
validation. Once constructed, known-good; no checks downstream.

```cpp
class MatchedXY {
public:
    static MatchedXY from_py(PyBuffer x, PyBuffer y);   // throws on mismatch
    const PyBuffer& x() const;
    const PyBuffer& y() const;
    std::size_t rows() const;
    std::size_t cols() const;   // 1 for 1D y
};

class MatchedXYZ { /* x 1D, y 1D, z 2D row/col-major, shapes match */ };
```

Shiboken typesystem converter accepts a Python tuple/list
`(x, y)` or `(x, y, z)` and produces a `MatchedXY` / `MatchedXYZ`, raising
`ValueError` on failure. If the codegen path forces a copy, drop back to
taking two/three `PyBuffer` parameters in the method and call
`MatchedXY::from_py` inside — same zero-copy guarantee, simpler binding.

### 3.3 `SafeSlot.hpp`

```cpp
template <typename F>
auto safe_slot(F&& f, std::string_view context = "");

#define SQP_SAFE_SLOT_BEGIN try {
#define SQP_SAFE_SLOT_END(ctx) \
    } catch (const std::exception& e) { qWarning() << ctx << e.what(); } \
      catch (...) { qWarning() << ctx << "unknown exception"; }
```

Applied to every `connect(..., [this](...) { ... })` lambda body and to
every override that calls into user code (`timerEvent`, `paintEvent` where
relevant).

### 3.4 Changes to existing types

- **`PyBuffer::data()`** — throws `std::runtime_error("PyBuffer is invalid")`
  instead of returning `nullptr` when invalid. Callers needing nullable
  access use `raw_data()` or test `is_valid()` first.
- **`dispatch_dtype`** — asserts on unknown format code today; replace
  with `throw std::invalid_argument("unsupported dtype: <code>")`.
- **`GetDataPyCallable::get_data(...)`** — before wrapping callback results
  into `PyBuffer`s:
  - Result must be a list or tuple, else `qWarning` + return empty vector.
  - Every element must satisfy `PyObject_CheckBuffer`, else
    `qWarning` + return empty.
  - For `get_data(x, y)` / `get_data(x, y, z)`: returned buffers must have
    matching leading-axis length.
  - Catch `std::runtime_error` from `PyBuffer` construction (bad dtype);
    `qWarning` + return empty.
  - Never propagates exceptions up — worker-thread callers expect
    `vector<PyBuffer>{}` on failure.

### 3.5 Lifetime audit sweep (one-time)

1. Grep all headers for non-owning `QObject*` member variables. Each is
   either:
   - owned by a guaranteed-outliving parent (Qt parent-child): document
     at the field, no change; or
   - otherwise: convert to `QPointer<T>`, null-check before each deref.
2. Grep for all slot lambda bodies; wrap with `SQP_SAFE_SLOT_BEGIN/END`
   or confirm trivial-body exemption.
3. Audit all resampler-held pointers across the thread boundary.

### 3.6 Index audit sweep (one-time)

Grep for every bound method signature containing `int`, `size_t`,
`qsizetype`, `std::size_t`. Triage:

- **Index into container** → `validate_index(i, container.size(), "param")`
  at entry.
- **Count / size** → reject negative.
- **Flag / enum** → OK (bounded by type), no change.

### 3.7 Exception translation in `bindings.xml`

One-shot XML edit. Audit every `<object-type>` / `<value-type>` block:
ensure `exception-handling="on"` (or the global typesystem equivalent) so
`std::runtime_error`, `std::invalid_argument`, `std::out_of_range`, and
`std::bad_alloc` map to the expected Python exceptions. Verified by the
fuzz suite in §5.

### 3.8 Debug-mode assertions

A `SQP_ASSERT(expr)` macro. No-op in release; `std::abort()` in debug
builds (dev and CI). Internal invariants (L3) use this. Release stays
fast; internal bugs get caught in CI.

### 3.9 Zero-copy invariant

**No validator, wrapper, or boundary transform copies the underlying
buffer data.** Only `shared_ptr<_PyBuffer_impl>` refcount bumps.

- `PyBuffer` uses `shared_ptr<_PyBuffer_impl>` (already); copy/move is a
  refcount bump. The `Py_buffer` view is a numpy view, not a copy.
- Validators take `const PyBuffer&` — no copy.
- `MatchedXY` / `MatchedXYZ` hold `PyBuffer` by value; factory takes
  rvalues and moves into members. RVO on the returned wrapper.
- Shiboken converter for `MatchedXY`: generated code calls
  `PyBuffer(obj_x)`, `PyBuffer(obj_y)`, then `MatchedXY::from_py(...)`
  with move semantics. Zero data copies end-to-end. Fallback (two
  `PyBuffer` args + construct inside method) has the same guarantee.
- `GetDataPyCallable` result path wraps returned objects with
  `PyBuffer(PyList_GetItem(...))` — a view.
- Existing `dataGuard` `shared_ptr` pattern preserves zero-copy safety
  across the async resampler thread.

Enforcement: every PR touching a boundary method has a "no copies
introduced" acceptance criterion. A benchmark in `tests/perf/` verifies
`set_data` with 10M samples runs within noise of today's timings — catches
accidental `memcpy` regressions.

## 4. Error model and data flow

### Exception taxonomy

| Failure | Python exception | Raised from |
|---|---|---|
| `None`, non-buffer where buffer expected | `TypeError` | Shiboken converter / `PyBuffer` ctor |
| Non-numeric dtype (object, string, void) | `TypeError` | `PyBuffer::init_buffer` |
| Wrong ndim | `ValueError` | `validate_buffer` |
| Shape mismatch (x.len ≠ y.rows) | `ValueError` | `validate_xy` / `validate_xyz` |
| Unsupported dtype reaching dispatch | `ValueError` | `dispatch_dtype` |
| Index out of range | `IndexError` | `validate_index` |
| Negative index | `IndexError` | `validate_index` |
| Empty array where non-empty required | `ValueError` | per-call validator |
| Allocation failure | `MemoryError` | `std::bad_alloc` translation |
| Method on deleted C++ object | `RuntimeError` | Shiboken built-in |
| Bad `graph_type` / arg count in `plot()` | `ValueError` | Python dispatcher |

Messages: `"<method>: <param>: <what was wrong> (got <actual>, expected <spec>)"`.
Example: `"set_data: y: ndim must be 1 or 2 (got 3)"`.

### Sync vs async paths

- **Sync path** (user calls `set_data`, `add_graph`, `set_range`, …):
  exceptions raise and propagate. Plot state is unchanged on failure —
  validate first, then mutate.
- **Async path** (worker thread invokes user's `get_data` callback):
  exceptions are caught and logged via `qWarning`; they cannot raise
  (no Python frame to raise into). Data source returns empty result;
  plot keeps its last good data. One `qWarning` per failure, no flood.
  `PyErr_Print` is used for exceptions *inside* the user callback;
  validation failures on the *return value* use `qWarning`.

### State invariants after failure

- `set_*` family: validate at entry; mutate only after validation passes.
  Failure → state unchanged.
- `add_*`: if construction fails partway, destructor cleans up; no
  half-registered plottable survives.
- `remove_*` with bad index: no-op + `IndexError`.
- Callback return validation failure: data source keeps the last good
  data; no partial update.

### Logging

- `qWarning` for async failures only, via `sqp_warn("context", ...)` helper
  with a stable prefix so users can filter.
- Validators silent on success; no hot-path `qDebug` spam.
- Env var `SQP_LOG_VERBOSE=1` enables diagnostic lines (sizes, dtypes) on
  validation failures — dev aid, off by default.

### Rejected alternatives

- In-plot error overlays — too opinionated; downstream (SciQLop) can layer
  this on top.
- Fail-soft return codes — Python idiom is exceptions; mixing breaks
  ergonomics.
- Automatic retry on async failures — masks real bugs.

## 5. Testing strategy

### 5.1 Targeted regression tests — `tests/integration/test_hardening/`

One test file per boundary class. Each exercises:

- Every validation failure mode (wrong dtype, ndim, shape, index, None,
  empty, NaN where rejected).
- Asserts the right exception class and that the message contains the
  parameter name.
- Asserts plot state is unchanged after a failed call (pre/post
  snapshot).

Deliverable: one file per plottable + per item + per plot class.

### 5.2 Property-based fuzz — `tests/integration/test_fuzz_crashproof.py`

Using Hypothesis. Harness generates arbitrary inputs and calls every bound
method. Oracle is simple: **the process must not crash.** Pass = either
success or a Python exception from the whitelisted set (`TypeError`,
`ValueError`, `IndexError`, `RuntimeError`, `MemoryError`).

Strategies cover:

- numpy arrays of every dtype, including object / void / strings
- arbitrary ndim (0D through 5D)
- NaN / Inf / negative / huge values
- `None`, strings, lists, dicts where arrays expected
- negative and huge integer indices
- misbehaving callbacks: raises, returns wrong count, returns None,
  returns non-numeric, blocks briefly

Per-example timeout; `SCIQLOP_FUZZ_MINUTES=N` controls budget (CI default
2–5 min; nightly 30 min).

### 5.2b Stateful fuzz — `RuleBasedStateMachine`

A single stateful machine models a SciQLopPlots session. Rules:

- `create_plot_panel` / `add_plot` / `remove_plot`
- `add_graph` / `add_colormap` / `add_scatter` / `remove_plottable`
  (random index, including out-of-range)
- `set_data` with random valid or garbage arrays
- `set_range` / `set_axis_type` / `set_visible`
- `add_span` / `remove_span`
- `set_callback` with a misbehaving callback
- `resize` / `show` / `hide` / `delete_panel`
- brief sleep to let the async resampler thread run

Invariant after every rule: process alive; no stray C++ log marker; no
uncaught Python warning. 200 rule sequences per CI run; longer at night.

### 5.3 Sanitizer CI job

Nightly job builds with `-fsanitize=address,undefined` and runs the full
test suite (unit + integration + fuzz). Any sanitizer report fails the
build. Catches stale-pointer reads and signed-overflow UB that Python-level
tests miss.

### Acceptance — "the contract holds"

- Full fuzz suite completes without process crash (no `SIGSEGV` /
  `SIGABRT` exit).
- Sanitizer CI green.
- Targeted regression tests all pass.
- No crash signal in any CI run across the past 7 days.

### Test dependencies

Hypothesis and any additional test deps are added to
`.github/workflows/test.yml`; tests are never skipped for missing
packages.

### Out of scope for testing

- Performance regression — covered by existing `tests/perf/`. The one
  addition here is the zero-copy benchmark from §3.9.
- Graphics-rendering correctness — not a crash concern.

## 6. Rollout decomposition

One spec (this document). Four sequential implementation plans; each is
its own PR stack.

### Sub-effort 1 — Foundation

No user-visible behavior change. Lands first; later sub-efforts depend
on the plumbing.

- `Validation.hpp`, `MatchedBuffers.hpp`, `SafeSlot.hpp` created.
- `PyBuffer::data()` / `dispatch_dtype` — throw instead of UB/assert.
- `bindings.xml` exception-translation audit.
- `SQP_ASSERT` macro added.

**Size:** small.

### Sub-effort 2 — Apply to data boundary

Main body of work. Can be split by subsystem (plottables → items →
panel → axes) for reviewability.

- Every `set_data` / `add_*` / integer-index method gets the appropriate
  validator call at entry.
- Rewrite the six `set_data` implementations to use `MatchedXY` /
  `MatchedXYZ` (or equivalent free-function validation).
- `GetDataPyCallable` result-validation added.
- Targeted regression tests (§5.1) land alongside each method.

**Size:** largest.

### Sub-effort 3 — Lifetime and signals

- Member-variable audit: non-owning `QObject*` → `QPointer<T>`.
- Slot-lambda audit: wrap with `SQP_SAFE_SLOT_BEGIN/END`.
- Resampler cross-thread pointer audit.
- Regression tests exercising deleted-object calls.

**Size:** medium.

### Sub-effort 4 — Fuzz harness and CI

- Per-method fuzz (§5.2).
- Stateful fuzz (§5.2 stateful).
- ASan/UBSan CI job (§5.3).
- Nightly fuzz budget.
- Zero-copy perf benchmark (§3.9).

**Size:** medium. Can scaffold in parallel with 2/3; finishes last.

### Sequencing

- 1 → 2 is a hard dependency.
- 3 is independent of 2 but reads better once 2 is merged.
- 4 scaffolds in parallel; lands last to validate.

### Done definition

- All four sub-efforts merged.
- Fuzz CI green for 7 consecutive days.
- Sanitizer CI green for 7 consecutive days.
- Invariant (§1) documented in the public README.
