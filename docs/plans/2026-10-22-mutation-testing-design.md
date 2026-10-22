# Mutation testing for SciQLopPlots and NeoQCP

Status: backlog (2026-10-22), the last step before V1. Design only, not approved for implementation. Do it after the API review (docs/api-review-2026-10-20.md) is finished.

## Why

Strong coverage, and no useless code or tests. A surviving mutant points at one of four things: a real bug, a weak test, dead code, or an equivalent change.

## Tools on this machine

- Mull is not installed and not packaged for Fedora; it would have to be built against LLVM 22.
- `gcovr`, `llvm-cov` and `lcov` are installed.
- mutmut does not fit: the Python package is loaded from copies in `build-venv/SciQLopPlots/`.

## Approach

1. **Coverage first**, from a separate coverage build (`-Db_coverage=true`). An uncovered line is a missing test or dead code. Coverage also maps each source file to the tests that run it, and removes uncovered lines from the mutation set.
2. **Mutation on covered lines only**, with one small data-driven mutator (`tools/mutation/`):
   - operator table: comparison flips (`<`/`<=`, `==`/`!=`), `&&`/`||`, negated conditions, `+`/`-`, off-by-one, forced early return or constant, deleted statement;
   - per mutant: apply, rebuild the one object with ninja (`-j 4`), run only the tests that cover the file, restore the file;
   - C++ and Python share the tool; for Python the "build" is copying the file into `build-venv`.
3. **Kill matrix**: which test kills which mutant. A test that kills nothing its neighbours don't, and covers nothing unique, is a removal candidate. NeoQCP needs a `QCP_TEST_FUNCTION` filter in `tests/auto/autotest.cpp` for per-test runs.

## Triage of each survivor

- real bug: fix it, test first;
- weak test: add or tighten a test that kills the mutant;
- dead or redundant code: remove it, suites as the guard;
- equivalent mutant: record it, no action.

## Phases

1. The tool, plus the code added by `feat/fills-histogram` (fills, error bars, histogram, pipeline fix): a few hundred mutants, hours.
2. The rest of NeoQCP, module by module (data sources, pipeline, graphs, axes, legend).
3. SciQLopPlots: C++ core, then the Python layer.

## Rules

- One build at a time, `-j 4`; runs are resumable, in chunks under 10 minutes, with a state file.
- The source tree is restored after every mutant; a run never leaves a modified file behind.
- Every run ends with the orphan-Xvfb check (`ps -eo pid,ppid,cmd | awk '/[X]vfb/ && $2==1'` must be empty).
- Reports go to the scratchpad, not the repo.
