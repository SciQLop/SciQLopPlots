# API Fuzzing Campaign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended)
> or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`)
> syntax for tracking.

**Goal:** Every public SciQLopPlots call either works or raises a Python exception of the right class
with a message that says why. It never crashes, hangs or prints an error it swallows. Every violation
found is fixed.

**Architecture:**

- **Out-of-process sweep.** A new sweep lives next to the existing self-delete sweep and reuses its
  receivers and argument guessing.
  - It lists every public method from shiboken signatures.
  - It generates one-bad-argument cases from a working baseline call.
  - It runs the cases in batched subprocesses, isolating a crashing case by bisection, and classifies
    the outcome against the contract.
- **Shared exception translation.** One fix makes C++ `std::invalid_argument` and `std::out_of_range`
  reach Python as `ValueError` and `IndexError`. Today shiboken's `exception-handling="auto-on"` turns
  every C++ exception into `RuntimeError`.
- **Story fuzzer.** It gets runtime coverage measurement, new actions and a deep subprocess run.

**Tech Stack:** Python 3.13 venv, PySide6/shiboken6 6.12.0, Hypothesis, pytest, meson/ninja,
herdr (opencode).

**Spec:** `docs/plans/2026-11-01-api-fuzzing-design.md`. Read it first. Its contract table and its
"Decisions settled" section are binding.

Planner's ruling: the sweep lives in `tests/sweeps/api_fuzz/`, not `tests/fuzzing/api_sweep/` as
the spec suggested. The reason is that it reuses `tests/sweeps/self_delete/common.py` (receivers and
argument guessing), which already covers 37 object kinds. If that is wrong, the cost is one directory
move.

## Global Constraints

- **Scope.** SciQLopPlots only. Never read, run or change anything in SciQLop, TSCat or TSCatGui.
- **Branch.** Work on `feat/api-fuzzing`, created from the current HEAD (`fc606a61`). Never push.
- **Build.**
  `VENV=$PWD/.venv; export PATH="$VENV/bin:/home/jeandet/Qt/6.12.0/gcc_64/bin:$PATH" PKG_CONFIG_PATH=/home/jeandet/Qt/6.12.0/gcc_64/lib/pkgconfig LLVM_INSTALL_DIR=/usr; ninja -C build-venv -j 4`
  - Run it in the foreground.
  - Run one build or test at a time.
  - Python-only package edits must be copied into `build-venv/SciQLopPlots/`.
- **Full suite.**
  `cd /tmp && QT_QPA_PLATFORM=offscreen PYTHONPATH=$P/build-venv:$P timeout -k 20 570 $P/.venv/bin/python -m pytest $P/tests/{integration,unit,fuzzing,resampler,tracing} -q -p no:cacheprovider`
  - Baseline: 4459 passed, 3 skipped.
  - Read the real counts and the exit code.
- **Subprocesses.** Every subprocess runs with `cwd=tempfile.gettempdir()` and
  `QT_QPA_PLATFORM=offscreen`. Sweep parallelism is at most 4 processes.
- **Commits.**
  - Use explicit pathspecs. Never touch `qt.ini`.
  - End every message with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- **Bug fixes.**
  - Reproducer test first; it must fail before the fix.
  - Fix the shared validation point, never each caller.
  - Never wrap a Qt slot in Python.
  - Never bend library code to suit the harness.
- **Time.** All times are UTC.
- **Contract (verbatim from the spec).**
  - A wrong type raises `TypeError`.
  - A bad value raises `ValueError`.
  - An already-deleted object raises `RuntimeError`.
  - The message must say what was wrong.
  - These are also accepted: subclasses of those classes, `OverflowError` for out-of-range integers,
    and `IndexError` for out-of-range indices.
  - Any other class is "unexpected class", for review.

## Review Focus

1. **Exceptions inside a user's data callback.** The callback runs on a worker thread, so the caller
   can't catch the error. The user still needs to see it, once, with the traceback, and the plot must
   stay usable.
   - This is pinned in Task 8: a story action whose provider raises, then a later valid call works.
   - How it is surfaced is a judgment call in Task 6.
2. **Unusual but valid arrays:** non-contiguous, big-endian, read-only, 0-length. Each must either
   work or raise `ValueError`. It must never read past the buffer.
   - Pinned in Task 4: these are entries in the bad-value table, and Task 3's test checks that every
     buffer parameter gets them.
3. **Python-level wrappers from `SciQLopPlots/__init__.py`** (`plot`, `.on`, `save`) are not shiboken
   methods. They must still be swept.
   - Pinned in Task 3: `test_catalogue_includes_python_wrappers`.
4. **A deleted object passed as an argument,** not only as the receiver. One example is a deleted plot
   given to `panel.remove_plot`.
   - Pinned in Task 4: `test_deleted_argument_cases_generated`.
5. **Calls from a non-GUI thread.** Factories must raise and mutators must queue, as since 0.42.2,
   with no crash and no stderr output.
   - Pinned in Task 9: the opencode round 1 brief requires thread cases.

---

### Task 1: Shared C++ → Python exception classes

**Files:**
- Modify: `SciQLopPlots/bindings/helper_scripts/shiboken-gen.py`, or the typesystem if shiboken has
  a native way (Step 1 decides).
- Test: `tests/integration/test_exception_classes.py`

**Interfaces:**
- **Produces:** after this task, a C++ throw maps to Python as follows:
  - `std::invalid_argument` → `ValueError`;
  - `std::out_of_range` → `IndexError`;
  - `sqp::unsupported_call` → `NotImplementedError`;
  - any other `std::exception` → `RuntimeError`.

  This applies to every wrapped call. Message is `what()`.
- **Later tasks rely on this.** Library code raises a value problem by throwing `std::invalid_argument`
  and a type problem by throwing the new `sqp::type_error`. `sqp::type_error` derives from
  `std::invalid_argument`, lives in `include/SciQLopPlots/Python/Validation.hpp`, and maps to
  `TypeError`.

- [ ] **Step 1: Check for native support.** Search the Qt docs (qt-docs MCP,
  `qt_documentation_search`) for a shiboken typesystem way to map C++ exception types to Python ones
  under `exception-handling`.
  - If there is one, use it.
  - Otherwise, post-process the generated `*_wrapper.cpp` in `shiboken-gen.py`. Insert catch clauses
    before every generated `catch (const std::exception &e)`, most-derived first:
    `sqp::type_error` → `PyExc_TypeError`, `std::invalid_argument` → `PyExc_ValueError`,
    `std::out_of_range` → `PyExc_IndexError`, `sqp::unsupported_call` → `PyExc_NotImplementedError`.
  - Write which route was taken and why into the task report.
- [ ] **Step 2: Write failing tests.**
  - `test_invalid_argument_is_value_error`: `SciQLopPlot().line(np.zeros((2, 2, 2)), np.zeros(2))`
    raises `ValueError`, and the message names the argument.
  - `test_out_of_range_is_index_error`: pick a public call whose C++ throws `std::out_of_range`, found
    by grep. If none exists, drop this test and say so in the report.
  - `test_unsupported_call_still_not_implemented`: a colour map's `set_data(x, y)` raises
    `NotImplementedError`.
  - `test_type_error_class`: `validate_buffer` given a non-numeric dtype (`np.array(["a"])`) raises
    `TypeError`. Change that check in `Validation.hpp` to throw `sqp::type_error`.
- [ ] **Step 3: Run the new test file.** Expect the `ValueError` and `TypeError` tests to fail, getting
  `RuntimeError`.
- [ ] **Step 4: Implement the Step 1 route.** Then rebuild: rm one generated wrapper, run
  `meson setup --reconfigure build-venv`, then build.
- [ ] **Step 5: Run the new tests, then the full suite.** All pass. Some existing tests expecting
  `RuntimeError` from validation may now see `ValueError`.
  - Update those tests only where the new class is the contract's class.
  - List each one in the report.
- [ ] **Step 6: Commit.** `fix(bindings): C++ argument errors reach Python as ValueError/TypeError/IndexError`

### Task 2: Sweep harness — run and classify cases

**Files:**
- Create: `tests/sweeps/api_fuzz/harness.py`
- Create: `tests/sweeps/api_fuzz/README.md`, with the usage of every entry point (kept current by later
  tasks).
- Test: `tests/integration/test_api_fuzz_harness.py`

**Interfaces:**
- **Produces:**
  - `Case(id: str, setup: str, call: str, expect: str)`: a dataclass.
    - `expect` is one of `"ok"`, `"type"`, `"value"`, `"deleted"`, `"ok_or_value"`, `"script"`.
    - `setup` and `call` are Python source.
    - `call` runs after `setup` in the same namespace.
  - `Outcome(case_id: str, kind: str, exc_type: str | None, message: str | None, stderr: str)`, where
    `kind` is one of `"returned"`, `"raised"`, `"crash"`, `"hang"`.
  - `run_batch(cases: list[Case], per_case_s: float = 20, batch_s: float = 120) -> list[Outcome]`.
  - `classify(case: Case, outcome: Outcome) -> Verdict`, where
    `Verdict(case_id: str, status: str, detail: str)` and `status` is one of `"pass"`, `"crash"`,
    `"hang"`, `"swallowed"`, `"wrong_class"`, `"bad_message"`, `"unexpected_class"`, `"no_error"`,
    `"baseline_broken"`.
  - `STDERR_ALLOW: list[tuple[re.Pattern, str]]`: pattern plus the reason it is harmless.
- **The child protocol.**
  - The harness writes one child script per batch. It prints a stderr sentinel line `@@CASE <id>@@`
    before each case and `@@END <id>@@` after it.
  - After each call it pumps events for 200 ms.
  - It prints one JSON line per case to stdout.
  - stderr between the sentinels belongs to that case. Setup's stderr is excluded.
  - It enables `faulthandler`.
- **Crash or hang inside a batch.** The cases before the crash keep their results. The crashing case
  is isolated by re-running the remaining cases one per process.

- [ ] **Step 1: Write failing tests,** one per behaviour, using synthetic cases.
  - `os.abort()` → status `crash`.
  - `time.sleep(60)` with `per_case_s=2` → `hang`.
  - `sys.stderr.write("boom\n")` with a normal return, expect `ok` → `swallowed`.
  - `raise TypeError("x must be an array")`, expect `type` → `pass`.
  - `raise TypeError("")` → `bad_message`. So do `"error"` and `"failed"`, compared case-insensitively.
  - `raise ValueError("bad x")`, expect `type` → `wrong_class`.
  - `raise KeyError("k")`, expect `value` → `unexpected_class`.
  - A normal return, expect `value` → `no_error`.
  - A normal return, expect `ok_or_value` → `pass`.
  - A batch `[ok, abort, ok]` → statuses `[pass, crash, pass]`.
  - A stderr line matching a `STDERR_ALLOW` pattern → `pass`.
  - Expect `ok` that raises → `baseline_broken`.
- [ ] **Step 2: Run the tests.** They fail because the module does not exist.
- [ ] **Step 3: Implement `harness.py`** to the interfaces above.
  - Use `subprocess.run` with `timeout=batch_s`.
  - Detect a per-case hang from the last sentinel seen.
  - Use the same interpreter as the test (`sys.executable`).
- [ ] **Step 4: Run the tests.** All pass.
- [ ] **Step 5: Commit.** `test(sweep): API fuzz harness runs cases out of process and classifies them`

### Task 3: Catalogue, receivers and baselines

**Files:**
- Create: `tests/sweeps/api_fuzz/catalogue.py`
- Test: `tests/integration/test_api_fuzz_catalogue.py`

**Interfaces:**
- **Consumes:** `tests/sweeps/self_delete/common.py`, imported with its directory on `sys.path`:
  - `BUILDERS`, `Hosts`, `candidates`, `from_signatures`, `_value_for`.
- **Produces:**
  - `Method(cls: str, name: str, signatures: list[str], python_wrapper: bool)`.
  - `public_methods() -> list[Method]`.
    - It covers every non-enum class exported by `SciQLopPlots` and its own public callables, read
      from `vars(cls)`.
    - It includes the Python wrappers installed by `SciQLopPlots/__init__.py`.
    - It excludes methods inherited from Qt.
  - `RECEIVERS: dict[str, str]` maps a class name to setup source that binds `obj`.
    - The setup source is built on the `BUILDERS` kinds. New entries are added for panels, plots, axes,
      the projection plot, the inspector and products views, the theme and so on.
  - `EXEMPT: dict[str, str]` maps a class name to the reason it is exempt. Examples: re-exported
    `date`/`datetime`/`timezone`, `*Ptr` aliases, abstract interfaces covered by their concrete
    receivers.
  - `baseline(method: Method) -> str | None` gives the call source of a working call, or `None`.
    It tries, in order:
    1. `OVERRIDES[(cls, name)]`;
    2. `common.candidates`;
    3. `common.from_signatures`;
    4. a zero-argument call.

    A baseline counts only once it ran as an `expect="ok"` case and passed.

- [ ] **Step 1: Write failing tests.**
  - `test_every_class_has_receiver_or_reason`: every class in `public_methods()` is in `RECEIVERS` or
    `EXEMPT`, and every `EXEMPT` reason is non-empty.
  - `test_receivers_build`: each `RECEIVERS` entry runs through `run_batch` as an `ok` case and passes.
  - `test_catalogue_includes_python_wrappers`: `("SciQLopPlot", "plot")` and
    `("SciQLopMultiPlotPanel", "plot")` are present with `python_wrapper=True`.
  - `test_baseline_coverage_report`: writes `test-reports/api_fuzz_baselines.json`, listing the methods
    without a baseline. It only asserts that the file was written.
- [ ] **Step 2: Run the tests.** They fail.
- [ ] **Step 3: Implement `catalogue.py`.**
- [ ] **Step 4: Run the tests until they pass.** Then look at the baseline report. Add `OVERRIDES`
  until every method has a baseline, or an override that states why no valid call exists, such as
  "needs a running drag".
- [ ] **Step 5: Commit.** `test(sweep): catalogue every public method with a working baseline call`

### Task 4: Mutations — bad-argument cases

**Files:**
- Create: `tests/sweeps/api_fuzz/mutations.py`
- Test: `tests/integration/test_api_fuzz_mutations.py`

**Interfaces:**
- **Consumes:** `Case`, `Method`, `baseline`.
- **Produces:**
  - `BAD_VALUES: list[BadValue]`, where `BadValue(name: str, source: str, applies_to: str, expect: str)`.
    - `applies_to` is one of `"any"`, `"number"`, `"buffer"`, `"sequence"`, `"object"`.
  - `cases_for(method: Method, baseline_call: str) -> list[Case]`.
- **The table contents.** The spec's list, with these expectations:
  - Type-wrong values expect `type`:
    - `None` for a non-optional parameter;
    - a string where a number or buffer is expected;
    - a dict;
    - a list given to a scalar parameter;
    - `object()`.
  - Numbers expect `ok_or_value`: NaN, ±inf, 0, -1, `2**63` (accepting `OverflowError`), and `1e308`.
  - Buffers:
    - These expect `ok_or_value`: empty; 0-d; 3-d; object, complex, bool and int8 dtypes;
      non-contiguous (`a[::2]`); big-endian (`a.astype(">f8")`); read-only.
    - Mismatched lengths expect `value`.
  - Argument count:
    - too few arguments expect `type`;
    - one argument too many expects `type`.
  - Deleted objects expect `deleted`:
    - a deleted receiver, created with `shiboken6.delete(obj)` and then the call;
    - a deleted wrapped-QObject argument.

- [ ] **Step 1: Write failing tests.**
  - `test_one_argument_mutated_at_a_time`: for a fake two-parameter method, each case differs from the
    baseline in exactly one argument position.
  - `test_buffer_parameters_get_buffer_values`: a parameter whose signature type mentions
    `PyBuffer`/`ndarray` gets every `"buffer"` entry.
  - `test_deleted_argument_cases_generated`: a method taking a `SciQLopPlotInterface` gets a
    deleted-argument case expecting `deleted`.
  - `test_count_cases`: too-few and too-many cases exist.
- [ ] **Step 2: Run the tests.** They fail.
- [ ] **Step 3: Implement `mutations.py`.** Parameters and their types are parsed from the shiboken
  signature string, the same parsing as `common.from_signatures`. Reuse it; don't fork it.
- [ ] **Step 4: Run the tests.** They pass.
- [ ] **Step 5: Commit.** `test(sweep): one-bad-argument mutations for every parameter kind`

### Task 5: Driver, reports and the CI subset

**Files:**
- Create: `tests/sweeps/api_fuzz/driver.py`
- Create: `tests/integration/test_api_contract.py`
- Modify: `tests/sweeps/api_fuzz/README.md`

**Interfaces:**
- **Consumes:** everything above.
- **Produces:**
  - `python driver.py sweep [--classes A,B] [--jobs 4]`
    - It writes `test-reports/api_fuzz.jsonl`, one verdict per line with the case source.
    - It also writes `test-reports/api_fuzz_summary.md`, with counts per status and failures grouped by
      class.method and status.
    - Exit code 1 if any verdict is not `pass`.
  - `python driver.py folder PATH`
    - Each `*.py` file in PATH is one `Case(expect="script")`.
    - The verdict is `pass` only if the process exits 0, its last stdout line is `ok`, and its stderr
      is empty after `STDERR_ALLOW`.
  - In `test_api_contract.py`:
    - `CI_CLASSES: list[str]` starts empty and grows in Task 7.
    - `test_contract(cls)` is parametrized over `CI_CLASSES` and asserts that every verdict for that
      class passes.
    - `test_deep_sweep` is skipped unless `SCIQLOP_PLOTS_DEEP_FUZZ=1`. It runs the whole sweep.

- [ ] **Step 1: Write a failing test.** `test_folder_mode`: a temp folder with three scripts, one per
  outcome (`print("ok")`, `os.abort()`, `sys.stderr.write("x"); print("ok")`). It should get the
  verdicts `pass`, `crash`, `swallowed`.
- [ ] **Step 2: Run the test.** It fails.
- [ ] **Step 3: Implement `driver.py` and `test_api_contract.py`.**
- [ ] **Step 4: Run the test.** It passes. Then run `driver.py sweep --classes SciQLopPlotRange` as a
  smoke run, and check that the summary file exists.
- [ ] **Step 5: Commit.** `test(sweep): API fuzz driver, reports and CI contract test`

### Task 6: First deep sweep and triage (execution)

**Files:**
- Create: `docs/plans/2026-11-01-api-fuzzing-findings.md`

- [ ] **Step 1: Run the full sweep.** Run `driver.py sweep --jobs 4` in the foreground, with a timeout
  of 600000 ms per call. If one run doesn't fit, run it in class chunks.
- [ ] **Step 2: Group every non-`pass` verdict by root cause,** not by method. One example: "buffers
  with ndim≠1 reach `std::vector` construction unchecked". For each group, the findings file records:
  - the root cause;
  - the shared fix point (file:function);
  - the affected methods and their count;
  - one minimal reproducer;
  - the status (`open` / `fixed <sha>`).
- [ ] **Step 3: Collect the judgment calls in their own section.** These are cases that need an API
  decision, not just a fix. Examples:
  - empty data accepted or refused;
  - NaN in a range clamped or refused;
  - how callback errors from worker threads are surfaced (Review Focus 1).

  Send them to the user in one message, most important first. Record the answers in the file.
- [ ] **Step 4: Commit.** `docs(plans): API fuzz first-sweep findings`

### Task 7: Fix loop (repeat per root-cause group, crashes first)

**Files:** per group. Fix points are usually `include/SciQLopPlots/Python/Validation.hpp`,
`src/PythonInterface.cpp`, the `bindings.xml` templates, or the owning C++ setter. Tests go in the
integration test file that already covers that class, or in `tests/integration/test_api_contract_fixes.py`.

- [ ] **Step 1: Write the reproducer.**
  - For a crash or hang: a subprocess test, in the style of `test_container_teardown.py`.
  - Otherwise: `pytest.raises(<contract class>, match=<argument name>)`.
- [ ] **Step 2: Run it.** It fails for the recorded reason.
- [ ] **Step 3: Fix at the group's shared point.**
  - Grep every caller of the function you touch.
  - Value problems throw `std::invalid_argument`. Type problems throw `sqp::type_error` (Task 1).
- [ ] **Step 4: Run the reproducer, then the affected classes.** The reproducer passes. Then run
  `driver.py sweep --classes <affected>`, with all verdicts `pass` for that group.
- [ ] **Step 5: Run the full suite.** Green, and read the counts.
- [ ] **Step 6: Record the fix.** Add every class that is now fully clean to `CI_CLASSES`. Mark the
  group `fixed <sha>` in the findings file.
- [ ] **Step 7: Commit, one per group.** `fix(<area>): <what callers now get>`

Repeat until the full sweep reports zero non-`pass` verdicts. The runtime cap for `CI_CLASSES` is
about 2 minutes. If all classes exceed it, keep a fixed representative list and say so in the
README.

### Task 8: Story fuzzer — coverage, new actions, deep run

**Files:**
- Create: `tests/fuzzing/coverage.py`
- Create: `tests/fuzzing/test_story_coverage.py`
- Create: new `*_actions.py` modules as needed.
- Modify: `tests/fuzzing/fuzzer_actions.py` and `tests/fuzzing/test_fuzzing.py`.

**Interfaces:**
- **Consumes:** `catalogue.public_methods()`.
- **Produces:**
  - `collect_called(fn) -> set[str]`.
    - It runs `fn` under `sys.setprofile`, recording `c_call` events.
    - Each event is recorded as the builtin's `__qualname__` (`Class.method`), restricted to
      SciQLopPlots classes.
  - `test_story_coverage` runs 20 stories of 20 steps under the collector. It writes
    `test-reports/story_coverage.md` with the public methods never called. It asserts only that more
    than zero were called.

- [ ] **Step 1: Write the test first.** Write `test_story_coverage` and see it fail (no module). Then
  implement `coverage.py` and see it pass.
- [ ] **Step 2: Read the uncovered list.** Add actions until each uncovered method is called or listed
  in `coverage.py`'s `NOT_IN_STORIES` with a reason.
  - At minimum, add actions for histogram, histogram2d, band (`set_fill_between`), error bars, plot
    groups, the projection plot, the item factories (text, ellipse, pixmap, curved line,
    horizontal/vertical lines and spans), legend toggling, theme switching, save to PNG, and
    `.on` observers.
  - Also add a data provider that raises (Review Focus 1). A later valid action on the same plot must
    work.
  - Each action has a real `verify` (not `lambda: True`) where state can be checked.
- [ ] **Step 3: Add the deep run to `test_fuzzing.py`.**
  - With `SCIQLOP_PLOTS_DEEP_FUZZ=1`, use `max_examples=100, stateful_step_count=50`.
  - `test_deep_story_fuzz` runs the fuzzer in a subprocess (`python -m pytest ...::test_ui_fuzzing`,
    with an env flag selecting deep settings). A crash then fails with the dumped story path instead
    of killing pytest.
  - CI keeps 10×10.
- [ ] **Step 4: Review every `except` in `tests/fuzzing/`.** Each must assert the exception class and
  its message as a deliberate refusal, like `colormap_or_refusal`, or be removed.
- [ ] **Step 5: Run the deep fuzzer.** Every failure goes into the findings file and through the Task 7
  loop.
- [ ] **Step 6: Run the full suite, then commit.** `test(fuzzing): story coverage report, missing actions, deep run`

### Task 9: opencode adversarial rounds (execution, controller only)

**Files:**
- Regression tests: `tests/integration/test_api_fuzz_regressions.py`.
- Scratch scripts: `$SCRATCH/opencode_round_<n>/`.

- [ ] **Step 1: Check the opencode pane.** `herdr agent list` must show the opencode pane (`w6:p10`)
  as live. If it is gone, start a new opencode in a new pane, launched with `-- --auto`. Never use
  `pane send-text`.
- [ ] **Step 2: Round 1.** Send the prompt with `herdr agent prompt` (wait, `--timeout 600000`).
  - Contents of the prompt:
    - the contract table;
    - the sweep README;
    - where the public API is (`SciQLopPlots/__init__.py`, `bindings.xml`);
    - a request for 30 or more standalone scripts in `$SCRATCH/opencode_round_1/`, each ending with
      `print("ok")`, aimed at multi-step interactions.
  - Required themes:
    - callbacks that delete their own plot or panel;
    - providers returning bad data;
    - deletion during a fetch;
    - re-entrant signals;
    - calls from a `threading.Thread`;
    - `save` during resize;
    - nested panels;
    - plot groups across removed plots.
- [ ] **Step 3: Read every script before running it.** Reject any that touch files outside
  `$SCRATCH` or the network. Then run `driver.py folder $SCRATCH/opencode_round_1`.
- [ ] **Step 4: Triage the failures.** Each real failure goes into the findings file and through the
  Task 7 loop. A script that found a bug becomes a test in `test_api_fuzz_regressions.py`, run through
  the harness.
- [ ] **Step 5: Run rounds 2 and 3.** Send opencode the verdict summary and ask for nastier variants
  around what failed. Stop early when a round finds no new root cause. At most 3 rounds.
- [ ] **Step 6: Commit the regression tests** with their fixes, per Task 7.

### Task 10: Close the campaign

- [ ] **Step 1: Run the final sweeps.**
  - `SCIQLOP_PLOTS_DEEP_FUZZ=1` deep sweep: zero non-`pass` verdicts.
  - Deep story run: green.
  - The GPU line graph receiver under `xvfb-run -a -s "-screen 0 1600x1200x24" env QT_QPA_PLATFORM=xcb`:
    zero non-`pass` verdicts.
- [ ] **Step 2: Run the full suite.** Green. Report the real counts.
- [ ] **Step 3: Ask opencode to review the whole branch diff.** Verify each finding against the code
  and fix the real ones through Task 7.
- [ ] **Step 4: Finish the paperwork.**
  - The findings file has every group `fixed` and every judgment call answered.
  - The README is current.
  - Add a project memory entry and its `MEMORY.md` line.
- [ ] **Step 5: Commit.** Do not push. Report to the user and wait for consent.
