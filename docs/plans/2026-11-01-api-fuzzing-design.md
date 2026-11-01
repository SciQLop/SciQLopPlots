# API fuzzing campaign — design

Date: 2026-11-01 (UTC). Scope: **SciQLopPlots only.** Nothing in SciQLop, TSCat or TSCatGui is read,
run or changed.

## Intent

Harden the public Python API and find bugs that only show up after a sequence of interactions.

The user's rule: **wrong input never crashes.** It raises an exception that tells the user what was wrong.
Errors are never swallowed.

## The contract every public call must meet

Given any input, a call either works or raises:

| Input problem | Expected exception |
|---|---|
| wrong type (string where an array is expected, wrong enum, a plot of the wrong class) | `TypeError` |
| bad value (NaN/inf where a finite number is needed, negative or zero counts, mismatched array lengths, empty where non-empty is needed, inverted range where order matters) | `ValueError` |
| object already deleted (C++ side gone) | `RuntimeError` (shiboken's own message is fine) |

The message must say what was wrong. Naming the argument is enough. An empty message, a bare
"error" or "failed", or a generic `Exception` all count as failures.

These count as failures:

- **Crash:** segfault, abort, `std::terminate`, a fatal Qt message.
- **Hang:** no return within the per-case timeout.
- **Swallowed error:** something written to stderr, such as a Qt warning, a shiboken "Error calling..."
  line or a Python traceback printed by a slot or callback, while the call returned normally.
  - The swallowed-error check covers only the case's own call, not setup.
  - Known-benign Qt noise (offscreen platform and font warnings) is filtered by an explicit allow-list
    in the harness. Each entry in that list needs a comment saying why it is harmless.
- **Wrong exception class** per the table above, or a message that doesn't explain.

Valid input that is merely unusual (empty data on a graph, a zero-width range on a log axis) may be
accepted. It must not crash, and it must not print errors. Whether the library accepts it or raises
is decided per case; see "Judgment calls" below.

## Parts

### 1. Automatic API sweep (new, `tests/fuzzing/api_sweep/`)

- **Find every method.** Enumerate every public class exported by `SciQLopPlots`. List each public
  method with its parameters, read from shiboken signatures. This is the same mechanism
  `test_binding_signatures.py` and `test_docs_signatures.py` already use. Python-level wrappers from
  `SciQLopPlots/__init__.py`, such as `plot()`, are included.
- **Build each target.** Every receiver comes from a small table of factories, one per class: a panel,
  a plot in a panel, a line graph with data, a colour map, items, and so on. A class with no factory
  is reported as uncovered, never skipped silently.
- **Generate bad calls.** For each method, take a valid call and replace **one** argument at a time
  with each value from a bad-value table:
  - `None`, NaN, ±inf, 0, -1, 2**63, a huge float;
  - empty arrays, 0-d arrays, 3-d arrays, wrong dtypes (object, complex, bool, int8), non-contiguous
    arrays, mismatched lengths;
  - strings, lists, dicts;
  - an already-deleted object of the right type.

  Also call each method with too few and too many arguments.
- **Isolate.** Each case runs in a subprocess so that a crash is recorded rather than fatal.
  - The case is a small generated Python script with `QT_QPA_PLATFORM=offscreen`,
    `cwd=tempfile.gettempdir()` and `faulthandler` enabled.
  - Cases are batched, several per process. When a batch crashes, it is re-run one case per process to
    find which one crashed.
- **Classify.** The harness records crash, hang, stderr output, and the exception class and message.
  It applies the contract table.
  - The output is a JSON report plus a short readable summary (failures grouped by method and kind).
  - The report goes to `test-reports/`, which is already the reports directory for the fuzzer.
- **CI.** CI runs a reduced deterministic subset as a normal pytest test. The full sweep runs behind
  an environment variable (`SCIQLOP_PLOTS_DEEP_FUZZ=1`).

### 2. opencode adversarial rounds

opencode generates multi-step cases that the one-argument sweep can't reach. Examples:

- a callback that deletes its own plot;
- a data provider that returns arrays with mismatched lengths or the wrong dtype;
- removing an item while dragging it;
- re-entrancy through signals;
- set_data from a thread;
- deleting a panel mid-fetch.

The loop:

1. opencode writes standalone case scripts into a scratch folder. Each script ends by printing an
   "ok" line.
2. The same subprocess harness runs them and classifies them against the contract.
3. opencode gets back the results: what crashed, what passed, and which kinds of failure appeared.
   It is asked for nastier variations.

The loop runs at most three rounds. It stops early when a round finds nothing new.

Rules for the loop:

- opencode is reached only through `herdr agent prompt` on the agreed pane.
- Its scripts are treated as untrusted. They are read before they run, and they run only through the
  harness.
- A script that found a real bug becomes a regression test under `tests/integration/`.

### 3. Story fuzzer completion

- **Coverage report.** A new test lists the public methods (the same list as part 1) and the methods
  the story actions call. It prints the methods no action calls.
  - The first pass tries to call each uncovered method, or explains why the method doesn't belong in
    stories.
  - After that, the report is informational. It does not fail CI.
- **Fill the gaps.** Add actions for the uncovered areas the coverage report shows. Likely candidates:
  histograms, bands, error bars, plot groups, projection plots and the newer item factories.
- **Deep run.** Behind `SCIQLOP_PLOTS_DEEP_FUZZ=1`, run 100 stories of 50 steps instead of 10×10.
  - It runs in a subprocess so that a crash reports the story instead of killing pytest.
  - The existing story dump (`_dump_story`) gives the reproducer.
  - CI keeps its current 10×10 settings.
- **Hidden failures.** Review the fuzzer's own `except` clauses. A clause that hides a library error
  must be narrowed or removed:
  - `graph_actions.py` catches `RuntimeError` and `NotImplementedError`.
  - `actions.py` re-raises, which is fine.

## Fixing what we find

**Every known violation is fixed in this campaign.** That includes crashes, hangs, swallowed errors,
wrong exception classes and unhelpful messages. Nothing is deferred to a later pass. The campaign
is done when the deep sweep and the deep story run report zero violations. Judgment calls are
decided with the user and then fixed in the same campaign.

- **Test first.** Every bug gets a reproducer test before its fix: a subprocess test for crashes, a
  `pytest.raises` with a match on the message for wrong or missing exceptions.
- **Fix the shared code.** Fix the shared validation point, not each caller. Validation belongs where
  the data enters, for example the `PyBuffer` and array conversion in the bindings or the C++ setter.
  It does not belong in Python wrappers on Qt slots; see the "no Python wrappers on slots" rule.
- **Converting C++ errors.** C++ code that throws must become a Python exception of the right class.
  Shiboken does not convert C++ exceptions by default, so how this is done is a decision for the
  plan, not a per-call patch.
- **Library code is never bent to please the harness.**
- **Judgment calls.** Some cases need an API decision, for example whether an empty array is accepted
  or raises, or whether NaN in a range is clamped or rejected. These are collected in a list for the
  user and not decided unilaterally. Clear-cut cases are fixed directly: a crash, a missing
  exception, the wrong class, an empty message.

## Non-goals

- No coverage-guided native fuzzing (libFuzzer/AFL) of NeoQCP internals. A NeoQCP crash found through
  the Python API is fixed in NeoQCP as usual.
- No fuzzing of the wasm/JS library in this campaign.
- No performance work.
- No change to valid-input behaviour beyond what a fix requires.

## Constraints

- **Builds.** One build or test run at a time, in the foreground, with `-j 4`. Subprocess
  parallelism inside the sweep is capped at 4 processes.
- **Environment.** Build and test recipe as in `CLAUDE.md` (`build-venv`, tests run from `/tmp`,
  offscreen).
- **Full suite.** `tests/{integration,unit,fuzzing,resampler,tracing}` must stay green.
- **Commits.** Explicit pathspecs. `qt.ini` is never touched. Nothing is pushed without the user's
  consent.
- **Time.** All times are UTC.

## Decisions settled after the builder check (Claude's calls, user to check)

1. **Valid baseline calls.**
   - **Where receivers come from.** Each receiver comes from the factory table.
   - **Where arguments come from.** Arguments come from a sample table keyed by parameter type: numpy
     arrays, `QColor`, the first member of an enum, a valid range, a callable that returns valid data,
     and so on. A small per-method override table covers the methods that need specific arguments.
   - **Checking the baseline.** The valid call runs first and must succeed.
     - If it fails, the method is reported as "baseline broken" and is not mutated.
     - A method with no buildable baseline is reported as uncovered.
2. **Exception classes.** The classifier accepts these classes:
   - `TypeError` and its subclasses for type problems;
   - `ValueError` and its subclasses for value problems;
   - `OverflowError` for out-of-range integers, which shiboken raises itself;
   - `IndexError` for out-of-range indices;
   - `RuntimeError` for deleted objects.

   Any other class is reported as "unexpected class" for review. It is not automatically called a
   bug. An existing deliberate refusal is accepted through an explicit per-method override. One
   example is the `RuntimeError` for "already has a colour map". Each override carries a one-line
   reason.
3. **Deleted objects.** Both receivers and arguments are tested, for example a deleted plot passed to
   a panel method. Both must raise `RuntimeError`. A silent no-op is a failure.
4. **When a case ends.** The case's window covers the call itself plus event processing until things
   settle, about 200 ms of `processEvents`. Errors printed by queued work are therefore caught. Setup
   output is excluded.
5. **CI policy.** There is no known-failures or xfail baseline file, because that would itself be a
   way of swallowing errors.
   - The deep sweep is informational.
   - The CI subset is a fixed list of classes, deterministic, and capped at about 2 minutes. It
     starts with the classes that are clean after the fixes and grows as fixes land. It is green at
     every commit.
6. **Story fuzzer `except` clauses.** Broad catches become explicit expected refusals. Each one
   asserts the exception class and its message, following the pattern of `colormap_or_refusal`. A
   catch that matches no deliberate refusal is removed.
7. **opencode rounds.** I run the rounds myself, not a subagent, using the existing reviewer pane. I
   check that the pane is still live before each round. The plan builds the harness and a "run case
   folder" entry point. The rounds are execution steps in the plan.
8. **Display.** Everything runs offscreen by default. Factories that need the GPU path (the GPU line
   graph) run under `xvfb-run`, as the existing GPU tests do.

## Left to the builder

- The case-script format and the batching size.
- The exact bad-value table entries beyond those listed.
- The per-case timeout. Suggested: 20 s per case, 120 s per batch.
- The report's file layout.
