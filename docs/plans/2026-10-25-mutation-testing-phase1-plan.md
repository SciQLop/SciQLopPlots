# Mutation testing, phase 1: implementation plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A small, data-driven mutation tool in `tools/mutation/`, and its first run on the code `feat/fills-histogram` added: fills, error bars, the 1-D histogram, the histogram2d fixes and the pipeline fix, in NeoQCP and in SciQLopPlots.

**Architecture:**
- A coverage build per repo (`build-cov`, `-Db_coverage=true`). Each test runs alone from zeroed counters, and gcovr (C++) or coverage.py (Python) records which target lines it ran. That map is also the baseline: whether each test passes, and how long it takes.
- Mutants come from one operator table: regex, replacement, languages. They are made only on covered target lines. A target is a whole file, or the lines the branch changed (`git diff -U0 main...HEAD`), or named Python functions.
- Per mutant:
  1. save a backup and an `in_flight` record in the state file;
  2. write the mutant;
  3. build with ninja through meson, `-j 4`;
  4. run only the tests that cover that line;
  5. restore the file in a `finally`.
  Every command first restores whatever `in_flight` still lists. This covers SIGKILL, a crash and the shell timeout.
- The state is one JSON file per project, in `<build-cov>/mutation/`. Every command is resumable and stops itself on a time budget. The report is markdown: survivors with a triage column, uncovered lines, and a kill-matrix summary, plus the matrix as JSON.

**Tech Stack:** Python 3.13 stdlib only (tomllib, argparse, re, json, subprocess, fcntl), gcovr 8.6, coverage.py (installed in Task 7), meson 1.10.2 + ninja, Qt 6.11.2, QtTest (NeoQCP `auto-tests`), pytest 9 + pytest-qt (SciQLopPlots).

**Spec:** docs/plans/2026-10-22-mutation-testing-design.md. Read it before your task. The user approved phase 1 on 2026-10-25. Where this plan and the spec disagree, the spec wins.

## Global Constraints

- **One build at a time.** Use `-j 4`, run in the foreground, and set the shell timeout to 600000 ms. Never background a build, a test run or a mutation chunk. Never start one while another runs. This includes the tool's own pytest suite (it compiles a fixture with g++) and the `mutate.py` commands that build: `cover`, `run`, `restore` and `try`.
- **Every mutation-tool invocation stops on its own in under 10 minutes.** Run it as `timeout -k 20 570 <command>` with the tool's default `--budget 520`. 570 + 20 s stays under the 600 s shell limit. When it exits 10 ("work left") or 3 ("interrupted"), run the same command again. The state file carries the work over.
- **A run never leaves a modified tracked file.** After every chunk run both commands below. Neither may list a file:
  - `git -C /home/jeandet/Documents/prog/SciQLopPlots status --porcelain --untracked-files=no`
  - `git -C /home/jeandet/Documents/prog/SciQLopPlots/subprojects/NeoQCP status --porcelain --untracked-files=no`

  If one does, run `mutate.py ... restore`, then check again. Then stop and report.
- **Coverage builds only:**
  - `build-cov` (SciQLopPlots) and `subprojects/NeoQCP/build-cov`.
  - Never configure, reconfigure or build `build-venv` or NeoQCP's `build` in this plan.
  - Never copy files into `.venv` or any SciQLop venv.
- **Build environment:** run from the SciQLopPlots root, with:
  ```bash
  export PATH="/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.11.2/gcc_64/bin:$PATH"
  export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.2/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
  export LLVM_INSTALL_DIR=/usr
  ```
  The tool's config sets the same variables for its own builds.
- **NeoQCP tests need a display.** Run every NeoQCP `cover`/`run`/`try` chunk under `xvfb-run -a -s "-screen 0 1280x1024x24"`, with `xvfb-run` *outside* `timeout`. After each such run, check that `ps -eo pid,ppid,cmd | awk '/[X]vfb/ && $2==1'` prints nothing. If it prints a line, kill that pid and say so in your report.
- **SciQLopPlots Python tests:** they run from `/tmp`, with `PYTHONPATH=<build dir>:<repo>` and `QT_QPA_PLATFORM=offscreen`. The config already does this. The build dir holds copies of `SciQLopPlots/*.py`; a Python mutant is written to that copy, never to the repo file.
- **NeoQCP per-test filter:** the env vars `QCP_TEST_CLASS` (one QtTest class) and `QCP_TEST_FUNCTION` (needs `QCP_TEST_CLASS`) in `subprojects/NeoQCP/tests/auto/autotest.cpp`. The tool runs one class per invocation and reads per-function kills from QtTest's `FAIL!  : Class::function(` lines.
- **Tool tests:** `cd /home/jeandet/Documents/prog/SciQLopPlots && timeout 540 .venv/bin/python -m pytest tools/mutation/tests -q -p no:cacheprovider`. Read the real pass/fail count and exit code. They are not part of the library's full suite (`tests/{integration,unit,fuzzing}`), and this plan does not change the library, so the library suite does not need to run.
- TDD: write the test first, watch it fail for the expected reason, then implement.
- Code style: small pure functions, data over code (the operator table, the TOML config), and no comment that restates code. A `why` comment is fine.
- `cat` and `ls` are aliased in this shell, and heredocs hang. Create and change files with the Write/Edit tools. In the shell, use `command cat`, `command ls` and `test -f`.
- Commits:
  - Commit in the SciQLopPlots repo only. This plan does not change NeoQCP.
  - Use explicit paths, never `git add -A`. `qt.ini` is not ours: never stage it.
  - Write messages with two `-m` flags, no heredoc: `git commit -m "<subject>" -m "Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"`.
  - Never push.
- Reports go to the session scratchpad. Call it `$SCRATCH`: the scratchpad directory your system prompt names. Reports never go in the repo.
- Never touch /home/jeandet/Documents/prog/SciQLop: no reads, no greps, no runs.
- Implementers do not dispatch subagents.

## Review Focus

1. **A chunk killed in the middle of a mutant.** This can be `timeout -k` escalating to SIGKILL, a crash, or a closed terminal. The mutated file must be back to the original at the next command's start. Task 2 tests SIGKILL at the state level. Task 4 tests that `run` restores a leftover first. Task 6 tests SIGTERM through the CLI.
2. **A chunk that ends, or is interrupted, right after a C++ mutant.** The source is restored, but the last binary was built from the mutant. The next `run`, `cover` or `try` must not test that stale binary. Every command builds before its first test and `run` builds again at the end. Task 4 tests that the fixture binary passes every test after a run.
3. **Sources changed between `cover`/`plan` and `run`.** This is what triage fixes do. Line numbers shift, so the mutants and the coverage map are stale. `plan` and `run` must refuse, with a message that names `cover --reset`. After `cover --reset` and `plan`, only the changed file's mutants are re-made, and a survivor that a new test covers is queued again. Task 4 tests both.
4. **A test that fails or flakes in the baseline.** It would "kill" every mutant it runs against. Such a test is left out of every mutant run and listed in the `plan` output and in the report. Task 4 has a `broken` fixture test. Task 8 checks the real baseline.
5. **Operator text in the report.** `||` and `|` would break the markdown table the triage happens in. Cells escape `|`. Task 5 tests it.

---

## File map

| File | Responsibility | Task |
|---|---|---|
| `tools/mutation/operators.py` | operator table, mutants for a line or file, `apply` | 1 |
| `tools/mutation/proc.py` | run a command in its own process group, with timeout; SIGTERM → `Interrupted` | 2 |
| `tools/mutation/state.py` | source I/O, sha256, atomic JSON state, `mutated()` with backup + `in_flight`, `restore_leftovers` | 2 |
| `tools/mutation/config.py` | TOML loading, templates, `Project`, target lines (all / diff / Python functions), test discovery | 3 |
| `tools/mutation/covermap.py` | per-test coverage + baseline, gcovr and coverage.py readers, resumable `cover` | 3 |
| `tools/mutation/runner.py` | `plan`, resumable `run`, `run_one`, `try_mutant`, failure parsers | 4 |
| `tools/mutation/report.py` | markdown report, kill matrix JSON | 5 |
| `tools/mutation/mutate.py` | CLI: cover / plan / run / report / restore / try; lock; exit codes | 6 |
| `tools/mutation/phase1.toml` | phase 1 targets, build and test commands for both projects | 7 |
| `tools/mutation/tests/` | `conftest.py`, `fixture_repo.py`, `fixture/` (calc.cpp, test_calc.cpp, tests.txt, limits.py, fake_report.py), one `test_<module>.py` per module, `test_phase1_config.py` | 1-7 |
| `.gitignore` | `build-cov` | 7 |
| `CLAUDE.md` | `coverage` in the dev-venv install line | 7 |
| `docs/plans/2026-10-22-mutation-testing-design.md` | status line with phase 1 numbers | 8, 9 |

Out of phase 1: the plain declaration headers (`multigraph-fill.h` and the like) and the `gapsBefore()` overrides in the datasource headers. NeoQCP mutants are judged by NeoQCP's own auto-tests only, and SciQLopPlots mutants by the SciQLopPlots pytest suite only.

---

### Task 1: Operator table and mutant generation

**Files:**
- Create: `tools/mutation/operators.py`
- Create: `tools/mutation/tests/conftest.py`
- Test: `tools/mutation/tests/test_operators.py`

**Interfaces:**
- Produces: `Mutant(path: str, line: int, col: int, op: str, before: str, after: str)`, a frozen dataclass with property `id -> "path:line:col:op"`. `line` is 1-based, `before`/`after` are whole lines without the newline.
- Produces: `OPERATORS: tuple[Operator, ...]`; `mutants_for_line(path, number, line) -> list[Mutant]`; `mutants_for_file(path, text, lines: set[int]) -> list[Mutant]`; `apply(text, mutant) -> str`. `apply` raises `ValueError("<id>: line changed since the plan")` when the line is no longer `before`.
- Operator names, used by every later task and by `[[project.equivalent]]` entries: `lt_to_le le_to_lt gt_to_ge ge_to_gt eq_to_ne ne_to_eq and_to_or or_to_and plus_to_minus minus_to_plus negate_if int_plus_one true_to_false false_to_true delete_statement`.

- [ ] **Step 1: Write the conftest and the failing tests**

`tools/mutation/tests/conftest.py`:

```python
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path[:0] = [str(HERE.parent), str(HERE)]
```

`tools/mutation/tests/test_operators.py`:

```python
import pytest

from operators import apply, mutants_for_file, mutants_for_line

CPP_SRC = """\
#include <vector>
// a < b in a comment
int clamp(int v, std::vector<double> &out)
{
    if (v < 0 && out.empty())
        return 0;
    out.push_back(v + 1);
    return v;
}
"""


def ops(path, line):
    return {m.op: m.after for m in mutants_for_line(path, 1, line)}


def test_comparison_logic_negation_and_constant():
    found = ops("a.cpp", "    if (v < 0 && out.empty())")
    assert found["lt_to_le"] == "    if (v <= 0 && out.empty())"
    assert found["and_to_or"] == "    if (v < 0 || out.empty())"
    assert found["negate_if"] == "    if (!(v < 0 && out.empty()))"
    assert found["int_plus_one"] == "    if (v < 1 && out.empty())"


def test_templates_comments_and_preprocessor_are_not_mutated():
    assert ops("a.cpp", "int clamp(int v, std::vector<double> &out)") == {}
    assert ops("a.cpp", "// a < b in a comment") == {}
    assert ops("a.cpp", "#include <vector>") == {}
    assert ops("a.cpp", "    double x = 1.0;") == {}


def test_trailing_comment_is_kept_and_not_mutated():
    found = ops("a.cpp", "    x = a + b; // a - b")
    assert found["plus_to_minus"] == "    x = a - b; // a - b"
    assert "minus_to_plus" not in found


def test_delete_statement_only_on_calls_and_assignments():
    assert ops("a.cpp", "    out.push_back(v + 1);")["delete_statement"] == "    ;"
    assert ops("a.cpp", "    mRange = range;")["delete_statement"] == "    ;"
    assert "delete_statement" not in ops("a.cpp", "    return v;")
    assert "delete_statement" not in ops("a.cpp", "    auto x = f();")


def test_return_constant_flip():
    assert ops("a.cpp", "        return true;")["true_to_false"] == "        return false;"
    assert ops("m.py", "        return False")["false_to_true"] == "        return True"


def test_python_operators():
    found = ops("m.py", "    if a is None and b > 2:")
    assert found["and_to_or"] == "    if a is None or b > 2:"
    assert found["gt_to_ge"] == "    if a is None and b >= 2:"
    assert found["negate_if"] == "    if not (a is None and b > 2):"
    assert found["int_plus_one"] == "    if a is None and b > 3:"
    assert ops("m.py", "    elif x:")["negate_if"] == "    elif not (x):"
    assert ops("m.py", "    plot.replot()")["delete_statement"] == "    pass"
    assert ops("m.py", "    # a < b") == {}


def test_one_mutant_per_operator_per_line():
    found = mutants_for_line("a.cpp", 1, "    x = a + b + c;")
    assert [m.op for m in found].count("plus_to_minus") == 1


def test_mutants_for_file_only_on_listed_lines():
    found = mutants_for_file("a.cpp", CPP_SRC, {5})
    assert found and {m.line for m in found} == {5}
    assert found[0].id == f"a.cpp:5:{found[0].col}:{found[0].op}"


def test_apply_replaces_one_line_and_keeps_newlines():
    mutant = next(m for m in mutants_for_file("a.cpp", CPP_SRC, {7}) if m.op == "plus_to_minus")
    out = apply(CPP_SRC, mutant)
    assert out.splitlines()[6] == "    out.push_back(v - 1);"
    assert out.count("\n") == CPP_SRC.count("\n")


def test_apply_refuses_a_changed_line():
    mutant = mutants_for_file("a.cpp", CPP_SRC, {7})[0]
    with pytest.raises(ValueError, match="line changed"):
        apply(CPP_SRC.replace("v + 1", "v + 2"), mutant)
```

- [ ] **Step 2: Run them and watch them fail**

Run: `timeout 540 .venv/bin/python -m pytest tools/mutation/tests/test_operators.py -q -p no:cacheprovider`
Expected: collection error, `ModuleNotFoundError: No module named 'operators'`.

- [ ] **Step 3: Write `tools/mutation/operators.py`**

```python
"""Mutation operators: one table of (name, languages, pattern, replacement).

A mutant replaces one match of one operator on one line. Binary operators are
matched with their surrounding spaces: the code is clang-formatted (C++) or
PEP 8 (Python), and the spaces keep templates (`QVector<double>`), `->` and
`<<` out of the table.
"""
import re
from dataclasses import dataclass

CPP, PY = "cpp", "py"


@dataclass(frozen=True)
class Operator:
    name: str
    langs: frozenset
    pattern: re.Pattern
    replacement: object  # a re template string, or a callable(match) -> str


@dataclass(frozen=True)
class Mutant:
    path: str
    line: int
    col: int
    op: str
    before: str
    after: str

    @property
    def id(self):
        return f"{self.path}:{self.line}:{self.col}:{self.op}"


def _op(name, langs, pattern, replacement):
    return Operator(name, frozenset(langs), re.compile(pattern), replacement)


def _plus_one(match):
    return str(int(match[0]) + 1)


OPERATORS = (
    _op("lt_to_le", {CPP, PY}, r" < ", " <= "),
    _op("le_to_lt", {CPP, PY}, r" <= ", " < "),
    _op("gt_to_ge", {CPP, PY}, r" > ", " >= "),
    _op("ge_to_gt", {CPP, PY}, r" >= ", " > "),
    _op("eq_to_ne", {CPP, PY}, r" == ", " != "),
    _op("ne_to_eq", {CPP, PY}, r" != ", " == "),
    _op("and_to_or", {CPP}, r" && ", " || "),
    _op("or_to_and", {CPP}, r" \|\| ", " && "),
    _op("and_to_or", {PY}, r" and ", " or "),
    _op("or_to_and", {PY}, r" or ", " and "),
    _op("plus_to_minus", {CPP, PY}, r" \+ ", " - "),
    _op("minus_to_plus", {CPP, PY}, r" - ", " + "),
    _op("negate_if", {CPP}, r"\bif \((.*)\)(\s*\{?)\s*$", r"if (!(\1))\2"),
    _op("negate_if", {PY}, r"^(\s*)(el)?if (.*):\s*$", r"\1\2if not (\3):"),
    _op("int_plus_one", {CPP, PY}, r"(?<![\w.])\d+(?![\w.])", _plus_one),
    _op("true_to_false", {CPP}, r"\btrue\b", "false"),
    _op("false_to_true", {CPP}, r"\bfalse\b", "true"),
    _op("true_to_false", {PY}, r"\bTrue\b", "False"),
    _op("false_to_true", {PY}, r"\bFalse\b", "True"),
    _op("delete_statement", {CPP}, r"^(\s*)(?:\w+(?:->|\.|::))*\w+\s*(?:\(.*\)|[-+*/|&]?= .*);\s*$", r"\1;"),
    _op("delete_statement", {PY}, r"^(\s*)(?:\w+\.)*\w+\(.*\)\s*$", r"\1pass"),
)


def language(path):
    return PY if path.endswith(".py") else CPP


def code_part(line, lang):
    """The part of a line operators may touch: no comment, no preprocessor line."""
    stripped = line.lstrip()
    if stripped.startswith(("//", "/*", "*", "#") if lang == CPP else "#"):
        return ""
    return line.split("//" if lang == CPP else "#", 1)[0]


def mutants_for_line(path, number, line):
    # simplify: first match only, one mutant per operator per line; the ceiling is a missed
    # second `+` on the same line. Upgrade: finditer, when phase 1 shows those lines survive.
    lang = language(path)
    code = code_part(line, lang)
    found = []
    for op in OPERATORS:
        match = op.pattern.search(code) if lang in op.langs else None
        if match is None:
            continue
        replacement = op.replacement(match) if callable(op.replacement) else match.expand(op.replacement)
        found.append(Mutant(path, number, match.start(), op.name, line, code[: match.start()] + replacement + line[match.end():]))
    return found


def mutants_for_file(path, text, lines):
    source = text.splitlines()
    return [m for n in sorted(lines) if 1 <= n <= len(source) for m in mutants_for_line(path, n, source[n - 1])]


def apply(text, mutant):
    lines = text.splitlines(keepends=True)
    current = lines[mutant.line - 1]
    body = current.rstrip("\r\n")
    if body != mutant.before:
        raise ValueError(f"{mutant.id}: line changed since the plan")
    lines[mutant.line - 1] = mutant.after + current[len(body):]
    return "".join(lines)
```

- [ ] **Step 4: Run the tests and watch them pass**

Run: `timeout 540 .venv/bin/python -m pytest tools/mutation/tests/test_operators.py -q -p no:cacheprovider`
Expected: `10 passed`, exit code 0.

- [ ] **Step 5: Commit**

```bash
git add tools/mutation/operators.py tools/mutation/tests/conftest.py tools/mutation/tests/test_operators.py
git commit -m "feat(mutation): operator table and mutant generation" -m "Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 2: Safe execution: process groups, state file, guaranteed restore

**Files:**
- Create: `tools/mutation/proc.py`, `tools/mutation/state.py`
- Test: `tools/mutation/tests/test_proc.py`, `tools/mutation/tests/test_state.py`

**Interfaces:**
- Produces (`proc`): `class Interrupted(Exception)`; `install_signal_handlers()`, which turns SIGTERM, SIGHUP and SIGINT into `Interrupted("<SIGNAME>")`; `Result(code: int | None, output: str, seconds: float)` with property `timed_out` (code is None); `run(cmd, *, cwd=None, env=None, timeout) -> Result`. `run` starts the command in a new session, merges stderr into stdout, and on timeout or exception kills the whole process group with SIGKILL.
- Produces (`state`): `read_source(path) -> str`; `write_source(path, text)` (UTF-8, newlines untouched); `sha(path) -> str`; `new_state() -> dict` with keys `version tests mutants files uncovered in_flight`; `load(path) -> dict`; `save(path, data)` (atomic); `restore_leftovers(state_path, data) -> list[str]`; `mutated(state_path, data, path, text, backup_dir)`, a context manager.
- `in_flight` entries: `{"path": str, "backup": str, "sha": str}`.

- [ ] **Step 1: Write the failing tests**

`tools/mutation/tests/test_proc.py`:

```python
import subprocess
import sys
import time
from pathlib import Path

import proc


def _gone(pid, wait=5.0):
    deadline = time.monotonic() + wait
    while time.monotonic() < deadline:
        try:
            if Path(f"/proc/{pid}/stat").read_text().split(") ", 1)[1].startswith("Z"):
                return True
        except FileNotFoundError:
            return True
        time.sleep(0.05)
    return False


def test_result_keeps_output_and_exit_code():
    result = proc.run(["bash", "-c", "echo hello; echo oops >&2; exit 3"], timeout=10)
    assert result.code == 3 and not result.timed_out
    assert "hello" in result.output and "oops" in result.output


def test_timeout_kills_the_whole_process_group(tmp_path):
    pidfile = tmp_path / "pid"
    result = proc.run(["bash", "-c", f"sleep 60 & echo $! > {pidfile}; wait"], timeout=1)
    assert result.timed_out
    assert _gone(int(pidfile.read_text()))


def test_sigterm_becomes_interrupted():
    tool_dir = str(Path(proc.__file__).parent)
    code = (
        f"import os, signal, sys; sys.path.insert(0, {tool_dir!r})\n"
        "import proc\n"
        "proc.install_signal_handlers()\n"
        "try:\n"
        "    os.kill(os.getpid(), signal.SIGTERM)\n"
        "except proc.Interrupted as why:\n"
        "    print('caught', why)\n"
    )
    out = subprocess.run([sys.executable, "-c", code], capture_output=True, text=True, timeout=10).stdout
    assert out.strip() == "caught SIGTERM"
```

`tools/mutation/tests/test_state.py`:

```python
import signal
import subprocess
import sys
import time
from pathlib import Path

import pytest

import state as st

TOOL_DIR = Path(st.__file__).resolve().parent
ORIGINAL = "int f() { return 1 < 2; }\n"


@pytest.fixture
def files(tmp_path):
    src = tmp_path / "f.cpp"
    src.write_text(ORIGINAL)
    return src, tmp_path / "run" / "state.json", tmp_path / "run" / "backup"


def test_save_is_atomic_and_load_fills_defaults(files):
    _, state_path, _ = files
    st.save(state_path, {"mutants": {"a": 1}})
    data = st.load(state_path)
    assert data["mutants"] == {"a": 1} and data["in_flight"] == []
    assert not state_path.with_name("state.json.tmp").exists()


def test_mutated_writes_then_restores(files):
    src, state_path, backup = files
    with st.mutated(state_path, st.new_state(), src, "MUTANT\n", backup):
        assert src.read_text() == "MUTANT\n"
        assert st.load(state_path)["in_flight"][0]["path"] == str(src)
    assert src.read_text() == ORIGINAL
    assert st.load(state_path)["in_flight"] == []


def test_mutated_restores_on_exception(files):
    src, state_path, backup = files
    with pytest.raises(RuntimeError, match="boom"):
        with st.mutated(state_path, st.new_state(), src, "MUTANT\n", backup):
            raise RuntimeError("boom")
    assert src.read_text() == ORIGINAL


def _child(src, state_path, backup, then):
    code = (
        f"import os, signal, sys, time; sys.path.insert(0, {str(TOOL_DIR)!r})\n"
        "import proc, state as st\n"
        "proc.install_signal_handlers()\n"
        f"with st.mutated({str(state_path)!r}, st.new_state(), {str(src)!r}, 'MUTANT\\n', {str(backup)!r}):\n"
        f"    {then}\n"
    )
    return subprocess.Popen([sys.executable, "-c", code])


def _wait_for_mutant(src):
    deadline = time.monotonic() + 10
    while src.read_text() != "MUTANT\n":
        assert time.monotonic() < deadline, "the child never wrote the mutant"
        time.sleep(0.05)


def test_a_killed_run_leaves_a_leftover_that_restore_puts_back(files):
    src, state_path, backup = files
    _child(src, state_path, backup, "os.kill(os.getpid(), signal.SIGKILL)").wait(timeout=10)
    assert src.read_text() == "MUTANT\n"
    data = st.load(state_path)
    assert st.restore_leftovers(state_path, data) == [str(src)]
    assert src.read_text() == ORIGINAL
    assert st.load(state_path)["in_flight"] == []


def test_sigterm_inside_the_block_restores(files):
    src, state_path, backup = files
    child = _child(src, state_path, backup, "time.sleep(30)")
    _wait_for_mutant(src)
    child.send_signal(signal.SIGTERM)
    assert child.wait(timeout=10) != 0
    assert src.read_text() == ORIGINAL
    assert st.load(state_path)["in_flight"] == []
```

- [ ] **Step 2: Run them and watch them fail**

Run: `timeout 540 .venv/bin/python -m pytest tools/mutation/tests/test_proc.py tools/mutation/tests/test_state.py -q -p no:cacheprovider`
Expected: collection errors, `No module named 'proc'` and `No module named 'state'`.

- [ ] **Step 3: Write `tools/mutation/proc.py`**

```python
"""Commands run in their own process group, so a timeout or an interrupt kills
everything they started (ninja's compilers, pytest's children), not only the
direct child."""
import os
import signal
import subprocess
import time
from dataclasses import dataclass


class Interrupted(Exception):
    pass


def _raise_interrupted(signum, frame):
    raise Interrupted(signal.Signals(signum).name)


def install_signal_handlers():
    """SIGTERM (from `timeout`), SIGHUP and SIGINT unwind through the finally blocks that restore sources."""
    for sig in (signal.SIGTERM, signal.SIGHUP, signal.SIGINT):
        signal.signal(sig, _raise_interrupted)


@dataclass
class Result:
    code: int | None
    output: str
    seconds: float

    @property
    def timed_out(self):
        return self.code is None


def _kill_group(child):
    try:
        os.killpg(child.pid, signal.SIGKILL)
    except ProcessLookupError:
        pass


def run(cmd, *, cwd=None, env=None, timeout):
    start = time.monotonic()
    child = subprocess.Popen(cmd, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                             text=True, errors="replace", start_new_session=True)
    try:
        output, _ = child.communicate(timeout=timeout)
        return Result(child.returncode, output, time.monotonic() - start)
    except subprocess.TimeoutExpired:
        _kill_group(child)
        output, _ = child.communicate()
        return Result(None, output, time.monotonic() - start)
    finally:
        if child.poll() is None:
            _kill_group(child)
            child.wait()
```

- [ ] **Step 4: Write `tools/mutation/state.py`**

```python
"""Run state: one JSON file per project, saved atomically after every step.

`in_flight` names the file a mutant changed, its backup and the original's
sha256. It is saved before the mutant is written, so a run killed by SIGKILL,
a crash or the shell's timeout leaves enough to put the original back: every
command starts with restore_leftovers().
"""
import hashlib
import json
import os
import shutil
from contextlib import contextmanager
from pathlib import Path


def read_source(path):
    with open(path, encoding="utf-8", newline="") as f:
        return f.read()


def write_source(path, text):
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(text)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def new_state():
    return {"version": 1, "tests": {}, "mutants": {}, "files": {}, "uncovered": {}, "in_flight": []}


def load(path):
    path = Path(path)
    if not path.exists():
        return new_state()
    return {**new_state(), **json.loads(path.read_text(encoding="utf-8"))}


def save(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".tmp")
    tmp.write_text(json.dumps(data, indent=1, sort_keys=True), encoding="utf-8")
    os.replace(tmp, path)


def restore_leftovers(state_path, data):
    restored = []
    for entry in data["in_flight"]:
        # copyfile, not copy2: the restored file must look newer than the object
        # built from the mutant, so the next ninja run rebuilds it.
        shutil.copyfile(entry["backup"], entry["path"])
        if sha(entry["path"]) != entry["sha"]:
            raise RuntimeError(f"restored {entry['path']} does not match its original sha256")
        restored.append(entry["path"])
    data["in_flight"] = []
    save(state_path, data)
    return restored


@contextmanager
def mutated(state_path, data, path, text, backup_dir):
    path = Path(path)
    backup = Path(backup_dir) / (path.name + ".orig")
    backup.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(path, backup)
    data["in_flight"] = [{"path": str(path), "backup": str(backup), "sha": sha(path)}]
    save(state_path, data)
    try:
        write_source(path, text)
        yield
    finally:
        restore_leftovers(state_path, data)
```

- [ ] **Step 5: Run the tests and watch them pass**

Run: `timeout 540 .venv/bin/python -m pytest tools/mutation/tests -q -p no:cacheprovider`
Expected: `18 passed` (10 + 3 + 5), exit code 0.

- [ ] **Step 6: Commit**

```bash
git add tools/mutation/proc.py tools/mutation/state.py tools/mutation/tests/test_proc.py tools/mutation/tests/test_state.py
git commit -m "feat(mutation): process-group runner and a state file that always restores the source" -m "Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 3: Config, target lines and the per-test coverage map

**Files:**
- Create: `tools/mutation/config.py`, `tools/mutation/covermap.py`
- Create: `tools/mutation/tests/fixture_repo.py`, `tools/mutation/tests/fixture/fake_report.py`
- Test: `tools/mutation/tests/test_config.py`, `tools/mutation/tests/test_covermap.py`

**Interfaces:**
- Consumes: `proc.run`, `state.read_source/sha/load/save/restore_leftovers`.
- Produces (`config`):
  - `Target(path, scope="all", functions=(), deploy="")`. `deploy` is an absolute path or `""`.
  - `Equivalent(path, op, before, reason)`.
  - `expand(value, variables)`. A list item that is exactly `"{tests}"` becomes one item per test.
  - `load(config_path, name) -> Project`.
  - `Project` fields: `name, root, repo, build_dir, raw, targets, equivalents`.
  - `Project` properties: `run_dir` (`build_dir/"mutation"`), `state_path`, `scratch`.
  - `Project` methods: `variables(tests=())`, `expand(value, tests=())`, `env(tests=(), mutation=False)` (mutation sets `GCOV_PREFIX=<run_dir>/gcda-junk`), `command(key, tests=())`, `cwd()`, `target(path)`, `deploy_path(target) -> Path`, `build() -> proc.Result`.
  - `changed_lines(repo, base, path) -> set[int]`, `function_lines(text, names) -> set[int]`, `target_lines(project, target) -> set[int]`, `discover_tests(project) -> list[str]`.
- Produces (`covermap`):
  - `READERS = {"gcovr": ..., "coveragepy": ...}`. Each reader is `(json_data, root) -> {realpath: (covered: set, code: set)}`.
  - `source_shas(project) -> {target.path: sha}`, `build_or_die(project)`, `reset_counters(build_dir)`, `prune_counters(build_dir, keep_regex)`, `read_reports(project)`.
  - `cover_one(project, name) -> {"status": "passed"|"failed"|"timeout", "seconds": float, "lines": {path: [int]}, "code_lines": {path: [int]}}`.
  - `cover(project, budget, reset=False) -> int`: how many tests are still pending. It sets `data["cover_files"]` (shas at cover time) and refuses changed sources unless `reset`.
- Config keys, read by later tasks:
  - top level: `root`;
  - per project: `name repo base build_dir env require_env build_cmd build_timeout test_cwd test_env test_cmd cover_test_cmd cover_timeout cover_keep_gcda cover_cmds cover_reports batch_tests stop_on_first_kill max_tests failure_parser tests target equivalent`;
  - `tests` is `{glob=[...], prefix=...}` or `{regex_file=..., pattern=...}`;
  - `target` is `{path, scope, functions, deploy}`.

- [ ] **Step 1: Write the helpers and the failing tests**

`tools/mutation/tests/fixture_repo.py`:

```python
"""Throwaway git repos for the tool's tests."""
import subprocess

GIT = ["-c", "user.name=t", "-c", "user.email=t@t", "-c", "commit.gpgsign=false", "-c", "core.hooksPath=/dev/null"]


def git(repo, *args):
    subprocess.run(["git", "-C", str(repo), *GIT, *args], check=True, capture_output=True)


def init_repo(path):
    """`path` becomes a git repo on branch main, with everything in it committed."""
    git(path, "init", "-q", "-b", "main")
    git(path, "add", ".")
    git(path, "commit", "-qm", "base")
    return path


def make_repo(path, files):
    path.mkdir(parents=True)
    for name, text in files.items():
        (path / name).write_text(text)
    return init_repo(path)
```

`tools/mutation/tests/fixture/fake_report.py`:

```python
"""Stands in for gcovr: test 'a' ran lines 1-2 of a.cpp, test 'b' ran line 3; 1-4 are code."""
import json
import sys

scratch = sys.argv[1]
ran = open(f"{scratch}/ran").read().strip()
covered = {"a": [1, 2], "b": [3]}.get(ran, [])
lines = [{"line_number": n, "count": int(n in covered)} for n in (1, 2, 3, 4)]
lines.append({"line_number": 5, "count": 0, "gcovr/noncode": True})
json.dump({"files": [{"file": "a.cpp", "lines": lines}]}, open(f"{scratch}/gcovr.json", "w"))
```

`tools/mutation/tests/test_config.py`:

```python
import pytest

import config
from fixture_repo import git, make_repo

PY = "def keep(x):\n    if x > 1:\n        return x\n    return 0\n\n\ndef drop(x):\n    return x + 1\n"

CONFIG = """
root = "."

[[project]]
name = "p"
repo = "repo"
base = "main"
build_dir = "build"
env = { GREETING = "hi {test}" }
test_cmd = ["run", "{tests}", "--in", "{build_dir}"]
tests = { glob = ["repo/test_*.py"], prefix = "{root}/" }

[[project.target]]
path = "a.cpp"
scope = "diff"

[[project.target]]
path = "m.py"
functions = ["keep"]
deploy = "{build_dir}/m.py"

[[project.equivalent]]
path = "a.cpp"
op = "eq_to_ne"
before = "TWO"
reason = "demo"

[[project]]
name = "q"
repo = "repo"
base = "main"
build_dir = "build-q"
tests = { regex_file = "repo/autotest.cpp", pattern = 'QCPTEST\\((\\w+)\\);' }

[[project.target]]
path = "a.cpp"
"""


@pytest.fixture
def root(tmp_path):
    repo = make_repo(tmp_path / "repo", {
        "a.cpp": "one\ntwo\nthree\nfour\n",
        "m.py": PY,
        "test_a.py": "",
        "test_b.py": "",
        "autotest.cpp": "#define QCPTEST(t) x\nQCPTEST(TestA);\nQCPTEST(TestB);\n",
    })
    git(repo, "checkout", "-qb", "feature")
    (repo / "a.cpp").write_text("one\nTWO\nthree\nfour\nfive\n")
    git(repo, "commit", "-qam", "change")
    (tmp_path / "phase.toml").write_text(CONFIG)
    return tmp_path


def test_load_resolves_paths_and_deploy(root):
    project = config.load(root / "phase.toml", "p")
    assert project.repo == (root / "repo").resolve()
    assert project.run_dir == (root / "build" / "mutation").resolve()
    assert project.targets[1].deploy == str((root / "build" / "m.py").resolve())
    assert project.deploy_path(project.targets[0]) == project.repo / "a.cpp"
    assert project.equivalents[0].op == "eq_to_ne"


def test_unknown_project_is_a_clear_error(root):
    with pytest.raises(SystemExit, match="no project 'nope'"):
        config.load(root / "phase.toml", "nope")


def test_tests_expand_to_one_argument_each(root):
    project = config.load(root / "phase.toml", "p")
    base = str(project.root)
    assert project.command("test_cmd", ["repo/test_a.py", "repo/test_b.py"]) == [
        "run", f"{base}/repo/test_a.py", f"{base}/repo/test_b.py", "--in", str(project.build_dir)]


def test_env_templates_and_gcov_prefix(root):
    project = config.load(root / "phase.toml", "p")
    assert project.env(["x"])["GREETING"] == "hi x"
    assert "GCOV_PREFIX" not in project.env()
    assert project.env(mutation=True)["GCOV_PREFIX"] == str(project.run_dir / "gcda-junk")


def test_diff_scope_is_the_changed_lines(root):
    project = config.load(root / "phase.toml", "p")
    assert config.target_lines(project, project.targets[0]) == {2, 5}


def test_python_functions_limit_the_lines(root):
    project = config.load(root / "phase.toml", "p")
    assert config.target_lines(project, project.targets[1]) == {1, 2, 3, 4}


def test_discover_tests_by_glob_and_by_regex(root):
    assert config.discover_tests(config.load(root / "phase.toml", "p")) == ["repo/test_a.py", "repo/test_b.py"]
    assert config.discover_tests(config.load(root / "phase.toml", "q")) == ["TestA", "TestB"]
```

`tools/mutation/tests/test_covermap.py`:

```python
import os
import shutil
import sys
from pathlib import Path

import pytest

import config
import covermap
import state as st
from fixture_repo import make_repo

FIXTURE = Path(__file__).parent / "fixture"

CONFIG = """
root = "."

[[project]]
name = "p"
repo = "repo"
base = "main"
build_dir = "build"
build_cmd = ["true"]
cover_test_cmd = ["{env_PYTHON}", "-c", "import sys; open(sys.argv[1] + '/ran', 'w').write(sys.argv[2]); sys.exit(sys.argv[2] == 'bad')", "{scratch}", "{test}"]
cover_cmds = [["{env_PYTHON}", "{repo}/fake_report.py", "{scratch}"]]
cover_reports = [{ kind = "gcovr", file = "{scratch}/gcovr.json", root = "{repo}" }]
tests = { regex_file = "repo/tests.txt", pattern = '(\\w+)' }

[[project.target]]
path = "a.cpp"
"""


@pytest.fixture
def project(tmp_path, monkeypatch):
    monkeypatch.setenv("PYTHON", sys.executable)
    repo = make_repo(tmp_path / "repo", {"a.cpp": "1\n2\n3\n4\n5\n", "tests.txt": "a b bad\n"})
    shutil.copy(FIXTURE / "fake_report.py", repo)
    (tmp_path / "build").mkdir()
    (tmp_path / "phase.toml").write_text(CONFIG)
    return config.load(tmp_path / "phase.toml", "p")


def test_cover_maps_each_test_to_its_lines_and_keeps_the_baseline(project):
    assert covermap.cover(project, budget=600) == 0
    tests = st.load(project.state_path)["tests"]
    assert tests["a"]["lines"] == {"a.cpp": [1, 2]}
    assert tests["b"]["lines"] == {"a.cpp": [3]}
    assert tests["a"]["code_lines"] == {"a.cpp": [1, 2, 3, 4]}
    assert tests["a"]["status"] == "passed" and tests["bad"]["status"] == "failed"
    assert tests["a"]["seconds"] >= 0


def test_cover_stops_on_its_budget_and_resumes(project):
    assert covermap.cover(project, budget=0) == 3
    assert covermap.cover(project, budget=600) == 0


def test_cover_reset_queues_every_test_again(project):
    covermap.cover(project, budget=600)
    assert covermap.cover(project, budget=0, reset=True) == 3


def test_cover_refuses_sources_changed_since_it_started(project):
    covermap.cover(project, budget=0)
    (project.repo / "a.cpp").write_text("changed\n")
    with pytest.raises(SystemExit, match="cover --reset"):
        covermap.cover(project, budget=600)


def test_cover_stops_when_the_unmutated_build_fails(tmp_path, monkeypatch):
    monkeypatch.setenv("PYTHON", sys.executable)
    make_repo(tmp_path / "repo", {"a.cpp": "1\n", "tests.txt": "a\n"})
    (tmp_path / "phase.toml").write_text(CONFIG.replace('build_cmd = ["true"]', 'build_cmd = ["false"]'))
    with pytest.raises(SystemExit, match="build failed"):
        covermap.cover(config.load(tmp_path / "phase.toml", "p"), budget=600)


def test_gcovr_reader_skips_noncode_lines():
    data = {"files": [{"file": "a.cpp", "lines": [
        {"line_number": 1, "count": 2}, {"line_number": 2, "count": 0},
        {"line_number": 3, "count": 0, "gcovr/noncode": True}]}]}
    assert covermap.READERS["gcovr"](data, "/r") == {os.path.realpath("/r/a.cpp"): ({1}, {1, 2})}


def test_coveragepy_reader():
    data = {"files": {"/x/m.py": {"executed_lines": [1, 3], "missing_lines": [2]}}}
    assert covermap.READERS["coveragepy"](data, "/tmp") == {os.path.realpath("/x/m.py"): ({1, 3}, {1, 2, 3})}


def test_prune_keeps_only_matching_counters(tmp_path):
    for name in ("src_plottables_multigraph-fill.cpp.gcda", "src_core.cpp.gcda"):
        (tmp_path / name).write_text("")
    covermap.prune_counters(tmp_path, r"multigraph-fill\.cpp\.gcda$")
    assert sorted(p.name for p in tmp_path.iterdir()) == ["src_plottables_multigraph-fill.cpp.gcda"]
```

- [ ] **Step 2: Run them and watch them fail**

Run: `timeout 540 .venv/bin/python -m pytest tools/mutation/tests/test_config.py tools/mutation/tests/test_covermap.py -q -p no:cacheprovider`
Expected: collection errors, `No module named 'config'` / `'covermap'`.

- [ ] **Step 3: Write `tools/mutation/config.py`**

```python
"""A phase config (TOML): what to mutate, and how to build, test and cover each project.

Everything project-specific is data in the TOML; this module only expands it.
Strings are templates: {root} {repo} {build_dir} {run_dir} {scratch} {test}
{env_<NAME>}. A list item that is exactly "{tests}" becomes one item per test.
"""
import ast
import dataclasses
import glob
import os
import re
import subprocess
import tomllib
from dataclasses import dataclass
from pathlib import Path

import proc
from state import read_source


@dataclass(frozen=True)
class Target:
    path: str
    scope: str = "all"
    functions: tuple = ()
    deploy: str = ""


@dataclass(frozen=True)
class Equivalent:
    path: str
    op: str
    before: str
    reason: str


def expand(value, variables):
    if isinstance(value, str):
        return value.format_map(variables)
    if isinstance(value, dict):
        return {key: expand(item, variables) for key, item in value.items()}
    return [arg for item in value for arg in (variables["tests"] if item == "{tests}" else [expand(item, variables)])]


@dataclass
class Project:
    name: str
    root: Path
    repo: Path
    build_dir: Path
    raw: dict
    targets: tuple = ()
    equivalents: tuple = ()

    @property
    def run_dir(self):
        return self.build_dir / "mutation"

    @property
    def state_path(self):
        return self.run_dir / "state.json"

    @property
    def scratch(self):
        return self.run_dir / "scratch"

    def variables(self, tests=()):
        tests = list(tests)
        base = {
            **{f"env_{key}": value for key, value in os.environ.items()},
            "root": str(self.root), "repo": str(self.repo), "build_dir": str(self.build_dir),
            "run_dir": str(self.run_dir), "scratch": str(self.scratch),
            "test": tests[0] if len(tests) == 1 else "",
        }
        prefix = self.raw.get("tests", {}).get("prefix", "").format_map(base)
        return {**base, "tests": [prefix + t for t in tests]}

    def expand(self, value, tests=()):
        return expand(value, self.variables(tests))

    def env(self, tests=(), mutation=False):
        junk = {"GCOV_PREFIX": str(self.run_dir / "gcda-junk")} if mutation else {}
        return {**os.environ, **self.expand(self.raw.get("env", {}), tests),
                **self.expand(self.raw.get("test_env", {}), tests), **junk}

    def command(self, key, tests=()):
        return self.expand(self.raw[key], tests)

    def cwd(self):
        return self.expand(self.raw.get("test_cwd", "{root}"))

    def target(self, path):
        return next(t for t in self.targets if t.path == path)

    def deploy_path(self, target):
        return Path(target.deploy) if target.deploy else self.repo / target.path

    def build(self):
        return proc.run(self.command("build_cmd"), cwd=str(self.root), env=self.env(),
                        timeout=self.raw.get("build_timeout", 900))


def load(config_path, name):
    config_path = Path(config_path).resolve()
    data = tomllib.loads(config_path.read_text(encoding="utf-8"))
    projects = {p["name"]: p for p in data["project"]}
    if name not in projects:
        raise SystemExit(f"no project {name!r} in {config_path}; have {sorted(projects)}")
    raw = projects[name]
    root = (config_path.parent / data.get("root", ".")).resolve()
    project = Project(name, root, (root / raw["repo"]).resolve(), (root / raw["build_dir"]).resolve(), raw)
    targets = tuple(Target(t["path"], t.get("scope", "all"), tuple(t.get("functions", ())), project.expand(t.get("deploy", "")))
                    for t in raw["target"])
    equivalents = tuple(Equivalent(e["path"], e["op"], e["before"], e["reason"]) for e in raw.get("equivalent", ()))
    return dataclasses.replace(project, targets=targets, equivalents=equivalents)


def changed_lines(repo, base, path):
    diff = subprocess.run(["git", "-C", str(repo), "diff", "-U0", f"{base}...HEAD", "--", path],
                          capture_output=True, text=True, check=True).stdout
    hunks = re.finditer(r"^@@ -\S+ \+(\d+)(?:,(\d+))? @@", diff, re.M)
    return {n for h in hunks for n in range(int(h[1]), int(h[1]) + int(h[2] or 1))}


def function_lines(text, names):
    return {n for node in ast.walk(ast.parse(text))
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)) and node.name in names
            for n in range(node.lineno, node.end_lineno + 1)}


def target_lines(project, target):
    text = read_source(project.repo / target.path)
    lines = set(range(1, len(text.splitlines()) + 1)) if target.scope == "all" else changed_lines(project.repo, project.raw["base"], target.path)
    return lines & function_lines(text, set(target.functions)) if target.functions else lines


def discover_tests(project):
    spec = project.raw["tests"]
    if "glob" in spec:
        found = {Path(p).relative_to(project.root).as_posix() for pattern in spec["glob"] for p in glob.glob(str(project.root / pattern))}
        return sorted(found)
    return re.findall(spec["pattern"], read_source(project.root / spec["regex_file"]))
```

- [ ] **Step 4: Write `tools/mutation/covermap.py`**

```python
"""Per-test coverage of the target files: which test runs which target line.

Each test runs alone from zeroed gcov counters; the configured report commands
(gcovr, coverage.py) then dump what ran. A test's entry also keeps whether it
passed and how long it took: the baseline the mutant runs rely on.
"""
import json
import os
import re
import shutil
import time
from pathlib import Path

import proc
import state as st
from config import discover_tests


def _gcovr(data, root):
    files = {}
    for f in data["files"]:
        code = [line for line in f["lines"] if not line.get("gcovr/noncode")]
        files[os.path.realpath(Path(root) / f["file"])] = (
            {line["line_number"] for line in code if line["count"] > 0},
            {line["line_number"] for line in code},
        )
    return files


def _coveragepy(data, root):
    return {
        os.path.realpath(Path(root) / name): (set(f["executed_lines"]), set(f["executed_lines"]) | set(f["missing_lines"]))
        for name, f in data["files"].items()
    }


READERS = {"gcovr": _gcovr, "coveragepy": _coveragepy}


def source_shas(project):
    return {t.path: st.sha(project.repo / t.path) for t in project.targets}


def build_or_die(project):
    result = project.build()
    if result.code != 0:
        raise SystemExit(f"build failed on the unmutated sources:\n{result.output[-3000:]}")


def reset_counters(build_dir):
    for gcda in Path(build_dir).rglob("*.gcda"):
        gcda.unlink()


def prune_counters(build_dir, keep):
    """Drops the counters of non-target objects, so gcovr reads a handful of files, not hundreds."""
    wanted = re.compile(keep)
    for gcda in Path(build_dir).rglob("*.gcda"):
        if not wanted.search(gcda.name):
            gcda.unlink()


def read_reports(project):
    found = {}
    for spec in project.raw.get("cover_reports", []):
        path = Path(project.expand(spec["file"]))
        if not path.exists():
            continue
        data = json.loads(path.read_text(encoding="utf-8"))
        for name, (covered, code) in READERS[spec["kind"]](data, project.expand(spec.get("root", "/"))).items():
            old_covered, old_code = found.get(name, (set(), set()))
            found[name] = (old_covered | covered, old_code | code)
    return found


def _target_files(project):
    pairs = [(project.repo / t.path, t.path) for t in project.targets] + [(project.deploy_path(t), t.path) for t in project.targets]
    return {os.path.realpath(p): name for p, name in pairs}


def _run_reports(project):
    for cmd in project.raw.get("cover_cmds", []):
        result = proc.run(project.expand(cmd), cwd=str(project.root), env=project.env(), timeout=300)
        if result.code != 0:
            print(f"warning: {cmd[0]} exited {result.code}: {result.output[-500:]}")


def cover_one(project, name):
    shutil.rmtree(project.scratch, ignore_errors=True)
    project.scratch.mkdir(parents=True)
    reset_counters(project.build_dir)
    result = proc.run(project.command("cover_test_cmd", [name]), cwd=project.cwd(), env=project.env([name]),
                      timeout=project.raw.get("cover_timeout", 300))
    if "cover_keep_gcda" in project.raw:
        prune_counters(project.build_dir, project.raw["cover_keep_gcda"])
    _run_reports(project)
    targets = _target_files(project)
    found = {targets[f]: lines for f, lines in read_reports(project).items() if f in targets}
    return {
        "status": "timeout" if result.timed_out else "passed" if result.code == 0 else "failed",
        "seconds": round(result.seconds, 2),
        "lines": {path: sorted(covered) for path, (covered, _) in found.items() if covered},
        "code_lines": {path: sorted(code) for path, (_, code) in found.items()},
    }


def _estimate(tests):
    return 30 + max((e["seconds"] for e in tests.values() if "seconds" in e), default=60)


def cover(project, budget, reset=False):
    data = st.load(project.state_path)
    st.restore_leftovers(project.state_path, data)
    shas = source_shas(project)
    if reset or "cover_files" not in data:
        data["cover_files"] = shas
    elif data["cover_files"] != shas:
        raise SystemExit("sources changed since cover started: run cover --reset")
    for name in discover_tests(project):
        if reset or name not in data["tests"]:
            data["tests"][name] = {"status": "pending"}
    st.save(project.state_path, data)
    build_or_die(project)
    started = time.monotonic()
    for name, entry in data["tests"].items():
        if entry["status"] != "pending":
            continue
        if time.monotonic() - started + _estimate(data["tests"]) > budget:
            break
        entry.update(cover_one(project, name))
        st.save(project.state_path, data)
    return sum(e["status"] == "pending" for e in data["tests"].values())
```

- [ ] **Step 5: Run the tests and watch them pass**

Run: `timeout 540 .venv/bin/python -m pytest tools/mutation/tests -q -p no:cacheprovider`
Expected: `33 passed` (18 + 7 + 8), exit code 0.

- [ ] **Step 6: Commit**

```bash
git add tools/mutation/config.py tools/mutation/covermap.py tools/mutation/tests/fixture_repo.py tools/mutation/tests/fixture/fake_report.py tools/mutation/tests/test_config.py tools/mutation/tests/test_covermap.py
git commit -m "feat(mutation): TOML config, target lines and a resumable per-test coverage map" -m "Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 4: Plan and run mutants, end to end on a C++ and a Python fixture

**Files:**
- Create: `tools/mutation/runner.py`
- Create: `tools/mutation/tests/fixture/calc.cpp`, `test_calc.cpp`, `tests.txt`, `limits.py`
- Test: `tools/mutation/tests/test_runner.py`

**Interfaces:**
- Consumes: everything from Tasks 1-3.
- Produces:
  - `FAILURES = {"qtest": re, "pytest": re}`, `DONE` (status tuple), `MAX_ATTEMPTS = 2`.
  - `covering_tests(tests) -> {path: {line: [names]}}`. Tests whose baseline is not `passed` are left out.
  - `plan(project) -> dict`. It refuses dirty targets, unfinished cover and changed sources. It drops the mutants of changed or removed files, and re-queues a survivor when a new test covers it.
  - `run(project, budget) -> int`: mutants still pending.
  - `run_one(project, data, entry) -> dict`.
  - `try_mutant(project, path, line, op, tests) -> dict`.
  - `failures(parser, result, batch) -> list[str]`.
- Mutant entry fields: `path line col op before after status tests covering killed_by seconds build_seconds attempts`.
- Statuses: `pending killed survived timeout build_error equivalent interrupted`.
- Killer names:
  - from QtTest: `Class::function`;
  - from pytest: `tests/.../test_x.py::test_y`;
  - when a run fails with no matching line: `<test>::<exit N>`;
  - on timeout: `<test>::<timeout>`.

- [ ] **Step 1: Write the fixture files**

`tools/mutation/tests/fixture/calc.cpp`, exactly these 8 lines, because the tests use the line numbers:

```cpp
int clamp_index(int i, int n)
{
    if (i < 0)
        return 0;
    if (i >= n)
        return n - 1;
    return i;
}
```

`tools/mutation/tests/fixture/test_calc.cpp`:

```cpp
#include <cstdio>
#include <cstring>

int clamp_index(int i, int n);

static int check(const char* name, int got, int want)
{
    if (got == want)
        return 0;
    std::printf("FAIL!  : Calc::%s() got %d, want %d\n", name, got, want);
    return 1;
}

int main(int argc, char** argv)
{
    const char* test = argc > 1 ? argv[1] : "";
    if (std::strcmp(test, "low") == 0)
        return check("low", clamp_index(-5, 10), 0);
    if (std::strcmp(test, "high") == 0)
        return check("high", clamp_index(12, 10), 9);
    if (std::strcmp(test, "edge") == 0)
        return check("edge", clamp_index(10, 10), 9);
    if (std::strcmp(test, "broken") == 0)
        return check("broken", clamp_index(4, 10), 5);
    return check("mid", clamp_index(4, 10), 4);
}
```

`tools/mutation/tests/fixture/tests.txt` (`edge` is added later by a test):

```
low high mid broken
```

`tools/mutation/tests/fixture/limits.py`:

```python
def clamp(value, low, high):
    if value < low:
        return low
    return min(value, high)
```

- [ ] **Step 2: Write the failing tests**

`tools/mutation/tests/test_runner.py`:

```python
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

import config
import covermap
import runner
import state as st
from fixture_repo import git, init_repo

FIXTURE = Path(__file__).parent / "fixture"

CONFIG = """
root = "."

[[project]]
name = "calc"
repo = "repo"
base = "main"
build_dir = "build"
build_cmd = ["bash", "-c", "mkdir -p {build_dir} && cd {build_dir} && g++ --coverage -O0 -c {repo}/calc.cpp -o calc.o && g++ --coverage -O0 -c {repo}/test_calc.cpp -o test_calc.o && g++ --coverage calc.o test_calc.o -o calc"]
test_cmd = ["{build_dir}/calc", "{test}"]
cover_test_cmd = ["{build_dir}/calc", "{test}"]
cover_cmds = [["gcovr", "--root", "{repo}", "--object-directory", "{build_dir}", "--json", "{scratch}/gcovr.json"]]
cover_reports = [{ kind = "gcovr", file = "{scratch}/gcovr.json", root = "{repo}" }]
failure_parser = "qtest"
tests = { regex_file = "repo/tests.txt", pattern = '(\\w+)' }

[[project.target]]
path = "calc.cpp"

[[project.equivalent]]
path = "calc.cpp"
op = "lt_to_le"
before = "    if (i < 0)"
reason = "i == 0 returns 0 either way"

[[project]]
name = "py"
repo = "repo"
base = "main"
build_dir = "build-py"
build_cmd = ["true"]
test_env = { PYTHONPATH = "{build_dir}" }
test_cmd = ["{env_PYTHON}", "-c", "import limits; assert limits.clamp(-1, 0, 5) == 0"]
failure_parser = "pytest"

[[project.target]]
path = "limits.py"
deploy = "{build_dir}/limits.py"
"""


def _setup(tmp_path, name):
    repo = tmp_path / "repo"
    shutil.copytree(FIXTURE, repo, ignore=shutil.ignore_patterns("__pycache__"))
    init_repo(repo)
    (tmp_path / "phase.toml").write_text(CONFIG)
    return config.load(tmp_path / "phase.toml", name)


@pytest.fixture
def calc(tmp_path):
    project = _setup(tmp_path, "calc")
    assert covermap.cover(project, budget=600) == 0
    runner.plan(project)
    return project


def by_op(project):
    return {(m["line"], m["op"]): m for m in st.load(project.state_path)["mutants"].values()}


def test_plan_picks_the_covering_tests_per_line(calc):
    mutants = by_op(calc)
    assert mutants[(4, "int_plus_one")]["tests"] == ["low"]
    assert mutants[(6, "minus_to_plus")]["tests"] == ["high"]
    assert mutants[(3, "lt_to_le")]["status"] == "equivalent"


def test_a_test_failing_its_baseline_runs_against_no_mutant(calc):
    data = st.load(calc.state_path)
    assert data["tests"]["broken"]["status"] == "failed"
    assert all("broken" not in m["tests"] for m in data["mutants"].values())


def test_run_kills_and_records_survivors(calc):
    assert runner.run(calc, budget=600) == 0
    mutants = by_op(calc)
    assert mutants[(6, "minus_to_plus")]["killed_by"] == ["Calc::high"]
    assert mutants[(4, "int_plus_one")]["killed_by"] == ["Calc::low"]
    assert set(mutants[(3, "negate_if")]["killed_by"]) == {"Calc::low", "Calc::high", "Calc::mid"}
    assert mutants[(5, "ge_to_gt")]["status"] == "survived"
    assert mutants[(3, "lt_to_le")]["killed_by"] == []


def test_run_leaves_the_source_and_the_binary_unmutated(calc):
    original = st.read_source(FIXTURE / "calc.cpp")
    runner.run(calc, budget=600)
    assert st.read_source(calc.repo / "calc.cpp") == original
    for name in ("low", "high", "mid", "edge"):
        assert subprocess.run([str(calc.build_dir / "calc"), name]).returncode == 0, name


def test_run_stops_on_its_budget_and_resumes(calc):
    pending = sum(m["status"] == "pending" for m in st.load(calc.state_path)["mutants"].values())
    assert pending > 0
    assert runner.run(calc, budget=0) == pending
    assert runner.run(calc, budget=600) == 0


def test_a_mutant_that_does_not_compile_is_a_build_error(calc):
    data = st.load(calc.state_path)
    entry = {"path": "calc.cpp", "line": 3, "col": 4, "op": "manual", "before": "    if (i < 0)",
             "after": "    if (i <<<< 0)", "tests": ["low"], "attempts": 0}
    assert runner.run_one(calc, data, entry)["status"] == "build_error"
    assert st.read_source(calc.repo / "calc.cpp") == st.read_source(FIXTURE / "calc.cpp")


def test_run_first_restores_what_a_killed_run_left(calc):
    path = calc.repo / "calc.cpp"
    backup = calc.run_dir / "backup" / "calc.cpp.orig"
    backup.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(path, backup)
    data = st.load(calc.state_path)
    data["in_flight"] = [{"path": str(path), "backup": str(backup), "sha": st.sha(path)}]
    st.save(calc.state_path, data)
    st.write_source(path, "garbage\n")
    runner.run(calc, budget=0)
    assert st.read_source(path) == st.read_source(FIXTURE / "calc.cpp")


def test_run_refuses_sources_changed_since_the_plan(calc):
    path = calc.repo / "calc.cpp"
    st.write_source(path, st.read_source(path).replace("return i;", "return i + 0;"))
    with pytest.raises(SystemExit, match="cover --reset"):
        runner.run(calc, budget=600)


def test_plan_refuses_a_target_with_uncommitted_changes(calc):
    path = calc.repo / "calc.cpp"
    st.write_source(path, st.read_source(path) + "// note\n")
    with pytest.raises(SystemExit, match="uncommitted changes"):
        runner.plan(calc)


def test_after_a_fix_cover_reset_and_plan_remake_the_file(calc):
    runner.run(calc, budget=600)
    path = calc.repo / "calc.cpp"
    st.write_source(path, st.read_source(path).replace("    return i;\n", "    return i; // clamped\n"))
    git(calc.repo, "commit", "-qam", "fix")
    with pytest.raises(SystemExit, match="cover --reset"):
        runner.plan(calc)
    covermap.cover(calc, budget=600, reset=True)
    data = runner.plan(calc)
    assert {m["status"] for m in data["mutants"].values()} == {"pending", "equivalent"}


def test_plan_requeues_a_survivor_a_new_test_covers(calc):
    runner.run(calc, budget=600)
    assert by_op(calc)[(5, "ge_to_gt")]["status"] == "survived"
    tests_txt = calc.repo / "tests.txt"
    tests_txt.write_text(tests_txt.read_text().strip() + " edge\n")
    covermap.cover(calc, budget=600, reset=True)
    runner.plan(calc)
    assert by_op(calc)[(5, "ge_to_gt")]["status"] == "pending"
    runner.run(calc, budget=600)
    assert by_op(calc)[(5, "ge_to_gt")]["killed_by"] == ["Calc::edge"]


def test_try_runs_one_mutant_of_the_current_source(calc):
    result = runner.try_mutant(calc, "calc.cpp", 5, "ge_to_gt", ["edge"])
    assert result["status"] == "killed" and result["killed_by"] == ["Calc::edge"]
    assert st.read_source(calc.repo / "calc.cpp") == st.read_source(FIXTURE / "calc.cpp")


def test_a_python_mutant_goes_to_the_deploy_copy_only(tmp_path, monkeypatch):
    monkeypatch.setenv("PYTHON", sys.executable)
    project = _setup(tmp_path, "py")
    project.build_dir.mkdir(parents=True)
    shutil.copyfile(project.repo / "limits.py", project.build_dir / "limits.py")
    data = st.load(project.state_path)
    data["tests"] = {"t": {"status": "passed", "seconds": 0.1,
                           "lines": {"limits.py": [1, 2, 3, 4]}, "code_lines": {"limits.py": [1, 2, 3, 4]}}}
    data["cover_files"] = covermap.source_shas(project)
    st.save(project.state_path, data)
    source = project.repo / "limits.py"
    stamp = source.stat().st_mtime_ns
    runner.plan(project)
    assert runner.run(project, budget=600) == 0
    assert source.stat().st_mtime_ns == stamp
    assert st.read_source(project.build_dir / "limits.py") == st.read_source(source)
    mutants = by_op(project)
    assert mutants[(2, "lt_to_le")]["status"] == "survived"
    assert mutants[(2, "negate_if")]["killed_by"] == ["t::<exit 1>"]


def test_failure_parsers():
    qtest = "PASS   : TestA::one()\nFAIL!  : TestA::two(row) Compared values\nFAIL!  : TestA::two(other) x\n"
    pytest_out = "FAILED tests/integration/test_x.py::test_y[1] - AssertionError\nERROR tests/unit/test_z.py::test_w\n"
    assert runner.failures("qtest", _result(1, qtest), ["TestA"]) == ["TestA::two"]
    assert runner.failures("pytest", _result(1, pytest_out), ["f"]) == [
        "tests/integration/test_x.py::test_y[1]", "tests/unit/test_z.py::test_w"]
    assert runner.failures("qtest", _result(-11, "Received signal"), ["TestA"]) == ["TestA::<exit -11>"]
    assert runner.failures("qtest", _result(None, ""), ["TestA"]) == ["TestA::<timeout>"]
    assert runner.failures("qtest", _result(0, qtest), ["TestA"]) == []


def _result(code, output):
    import proc
    return proc.Result(code, output, 1.0)
```

The equivalent `lt_to_le` on line 3 is a real equivalent mutant: at `i == 0` both versions return 0. It exercises the `[[project.equivalent]]` path.

- [ ] **Step 3: Run them and watch them fail**

Run: `timeout 540 .venv/bin/python -m pytest tools/mutation/tests/test_runner.py -q -p no:cacheprovider`
Expected: collection error, `No module named 'runner'`.

- [ ] **Step 4: Write `tools/mutation/runner.py`**

```python
"""Mutant runs: apply one mutant, build, run the tests that cover its line, record, restore.

Every step is saved to the state file, so a run stopped by its budget, a signal
or SIGKILL continues where it stopped.
"""
import re
import shutil
import subprocess
import time
from dataclasses import asdict

import covermap
import operators as ops
import proc
import state as st
from config import target_lines

FAILURES = {
    "qtest": re.compile(r"^FAIL!\s+:\s+(\w+::\w+)\(", re.M),
    "pytest": re.compile(r"^(?:FAILED|ERROR) (\S+)", re.M),
}
DONE = ("killed", "survived", "timeout", "build_error", "equivalent", "interrupted")
MAX_ATTEMPTS = 2


def covering_tests(tests):
    out = {}
    for name, entry in tests.items():
        if entry.get("status") != "passed":
            continue
        for path, lines in entry.get("lines", {}).items():
            for line in lines:
                out.setdefault(path, {}).setdefault(line, []).append(name)
    return out


def _select(names, tests, cap):
    # simplify: with a cap, a mutant runs only the fastest covering tests; the ceiling is a
    # survivor a slower test would kill. The report shows "run/covering"; `try` reruns with more.
    ordered = sorted(names, key=lambda n: tests[n].get("seconds", 0))
    return ordered[:cap] if cap else ordered


def _merge(old, fresh, equivalent):
    if old is None:
        return {**fresh, "status": "equivalent" if equivalent else "pending", "killed_by": [],
                "seconds": 0.0, "build_seconds": 0.0, "attempts": 0}
    if equivalent:
        return {**old, "status": "equivalent"}
    if old["status"] in ("pending", "survived") and set(fresh["tests"]) - set(old["tests"]):
        return {**old, **fresh, "status": "pending", "attempts": 0}
    return old


def _check_clean(project, when):
    paths = [t.path for t in project.targets]
    out = subprocess.run(["git", "-C", str(project.repo), "status", "--porcelain", "--untracked-files=no", "--", *paths],
                         capture_output=True, text=True, check=True).stdout
    if out.strip():
        raise SystemExit(f"{when}: target files have uncommitted changes:\n{out}")


def plan(project):
    _check_clean(project, "plan")
    data = st.load(project.state_path)
    if not data["tests"] or any(e["status"] == "pending" for e in data["tests"].values()):
        raise SystemExit("cover is not finished: run cover until it reports 0 left")
    shas = covermap.source_shas(project)
    if data.get("cover_files") != shas:
        raise SystemExit("sources changed since cover: run cover --reset, then plan")
    data["mutants"] = {k: m for k, m in data["mutants"].items() if data["files"].get(m["path"]) == shas.get(m["path"])}
    data["files"] = shas
    covering = covering_tests(data["tests"])
    equivalents = {(e.path, e.op, e.before) for e in project.equivalents}
    for target in project.targets:
        lines = target_lines(project, target)
        file_cover = covering.get(target.path, {})
        code = {n for e in data["tests"].values() for n in e.get("code_lines", {}).get(target.path, [])}
        data["uncovered"][target.path] = sorted((lines & code) - set(file_cover))
        text = st.read_source(project.repo / target.path)
        for m in ops.mutants_for_file(target.path, text, lines & set(file_cover)):
            names = file_cover[m.line]
            fresh = {**asdict(m), "tests": _select(names, data["tests"], project.raw.get("max_tests")), "covering": len(names)}
            data["mutants"][m.id] = _merge(data["mutants"].get(m.id), fresh, (m.path, m.op, m.before) in equivalents)
    st.save(project.state_path, data)
    return data


def failures(parser, result, batch):
    if result.timed_out:
        return [f"{n}::<timeout>" for n in batch]
    if result.code == 0:
        return []
    found = list(dict.fromkeys(FAILURES[parser].findall(result.output)))
    return found or [f"{n}::<exit {result.code}>" for n in batch]


def _status(killed):
    if not killed:
        return "survived"
    return "timeout" if all(k.endswith("::<timeout>") for k in killed) else "killed"


def _run_tests(project, names, baseline):
    batches = [names] if project.raw.get("batch_tests") else [[n] for n in names]
    killed, seconds = [], 0.0
    for batch in batches:
        timeout = 30 + 3 * sum(baseline.get(n, {}).get("seconds", 60) for n in batch)
        result = proc.run(project.command("test_cmd", batch), cwd=project.cwd(), env=project.env(batch, mutation=True), timeout=timeout)
        shutil.rmtree(project.run_dir / "gcda-junk", ignore_errors=True)
        seconds += result.seconds
        killed += failures(project.raw["failure_parser"], result, batch)
        if killed and project.raw.get("stop_on_first_kill"):
            break
    return killed, seconds


def run_one(project, data, entry):
    entry["attempts"] += 1
    if entry["attempts"] > MAX_ATTEMPTS:
        return {"status": "interrupted"}
    st.save(project.state_path, data)
    mutant = ops.Mutant(**{k: entry[k] for k in ("path", "line", "col", "op", "before", "after")})
    deploy = project.deploy_path(project.target(mutant.path))
    text = ops.apply(st.read_source(deploy), mutant)
    with st.mutated(project.state_path, data, deploy, text, project.run_dir / "backup"):
        built = project.build()
        if built.code != 0:
            return {"status": "build_error", "seconds": round(built.seconds, 1), "build_seconds": round(built.seconds, 1)}
        killed, seconds = _run_tests(project, entry["tests"], data["tests"])
    return {"status": _status(killed), "killed_by": killed,
            "seconds": round(built.seconds + seconds, 1), "build_seconds": round(built.seconds, 1)}


def _check_sources(project, data):
    changed = [p for p, digest in data["files"].items() if st.sha(project.repo / p) != digest]
    if changed:
        raise SystemExit(f"sources changed since plan: {changed}; run cover --reset, then plan")


def _estimate(mutants):
    done = [m for m in mutants.values() if m["status"] in DONE and m.get("seconds")]
    return max((m["seconds"] for m in done), default=120) + max((m.get("build_seconds", 0) for m in done), default=60)


def run(project, budget):
    data = st.load(project.state_path)
    st.restore_leftovers(project.state_path, data)
    _check_sources(project, data)
    started = time.monotonic()
    covermap.build_or_die(project)
    for entry in data["mutants"].values():
        if entry["status"] != "pending":
            continue
        if time.monotonic() - started + _estimate(data["mutants"]) > budget:
            break
        entry.update(run_one(project, data, entry))
        st.save(project.state_path, data)
    covermap.build_or_die(project)
    _check_clean(project, "run")
    return sum(m["status"] == "pending" for m in data["mutants"].values())


def try_mutant(project, path, line, op, tests):
    """One mutant of the current source against `tests`, outside the plan: the check after a triage fix."""
    data = st.load(project.state_path)
    st.restore_leftovers(project.state_path, data)
    covermap.build_or_die(project)
    text = st.read_source(project.repo / path)
    found = [m for m in ops.mutants_for_line(path, line, text.splitlines()[line - 1]) if m.op == op]
    if not found:
        raise SystemExit(f"no {op} mutant on {path}:{line}")
    result = run_one(project, data, {**asdict(found[0]), "tests": list(tests), "attempts": 0})
    covermap.build_or_die(project)
    return result
```

- [ ] **Step 5: Run the tests and watch them pass**

Run: `timeout 540 .venv/bin/python -m pytest tools/mutation/tests -q -p no:cacheprovider`
Expected: `47 passed` (33 + 14), exit code 0.

If `test_plan_picks_the_covering_tests_per_line` fails because gcov put a line on a neighbour, print the coverage map (`st.load(calc.state_path)["tests"]`) and fix the fixture, not the expectation. The fixture is built with `-O0`, so gcov's lines are exact.

- [ ] **Step 6: Commit**

```bash
git add tools/mutation/runner.py tools/mutation/tests/fixture/calc.cpp tools/mutation/tests/fixture/test_calc.cpp tools/mutation/tests/fixture/tests.txt tools/mutation/tests/fixture/limits.py tools/mutation/tests/test_runner.py
git commit -m "feat(mutation): plan and run mutants on covered lines, resumable, always restored" -m "Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 5: The report and the kill matrix

**Files:**
- Create: `tools/mutation/report.py`
- Test: `tools/mutation/tests/test_report.py`

**Interfaces:**
- Consumes: the state dict, with the mutant entry fields from Task 4.
- Produces:
  - `STATUSES`;
  - `ranges(numbers) -> str`, for example `"10-12, 20"`;
  - `cell(text) -> str`, a code span with `|` escaped;
  - `kill_matrix(data) -> {killer: [mutant ids]}`;
  - `redundancy(data) -> (never_killed_units: list, no_unique_kill: list)`;
  - `render(name, data, fail_fast=False) -> str`;
  - `write(project, data, out=None) -> Path`. It writes `<out>` and `<out stem>-kill-matrix.json`. `project` only needs `.name`, `.run_dir` and `.raw`.

- [ ] **Step 1: Write the failing tests**

`tools/mutation/tests/test_report.py`:

```python
import json
from types import SimpleNamespace

import report


def mutant(line, op, status, tests=("TestA",), killed_by=(), before="    if (a || b)", after="    if (a && b)", path="a.cpp"):
    return {"path": path, "line": line, "col": 4, "op": op, "before": before, "after": after, "status": status,
            "tests": list(tests), "covering": len(tests) + 1, "killed_by": list(killed_by),
            "seconds": 1.0, "build_seconds": 0.5, "attempts": 1}


DATA = {
    "tests": {"TestA": {"status": "passed"}, "TestX": {"status": "failed"}},
    "mutants": {
        "a.cpp:3:4:or_to_and": mutant(3, "or_to_and", "survived"),
        "a.cpp:5:4:lt_to_le": mutant(5, "lt_to_le", "killed", ("TestA", "TestB"), ("TestA::f", "TestB::g")),
        "a.cpp:7:4:negate_if": mutant(7, "negate_if", "killed", ("TestA",), ("TestA::f",)),
        "a.cpp:9:4:eq_to_ne": mutant(9, "eq_to_ne", "killed", ("TestC",), ("TestC::h",)),
        "b.cpp:2:4:true_to_false": mutant(2, "true_to_false", "build_error", path="b.cpp"),
        "b.cpp:4:4:plus_to_minus": mutant(4, "plus_to_minus", "survived", ("TestD",), path="b.cpp"),
    },
    "uncovered": {"a.cpp": [10, 11, 12, 20], "b.cpp": []},
}


def test_ranges():
    assert report.ranges([12, 10, 11, 20, 22, 21]) == "10-12, 20-22"
    assert report.ranges([5]) == "5"
    assert report.ranges([]) == ""


def test_survivor_rows_escape_pipes_and_leave_triage_empty():
    text = report.render("p", DATA)
    assert "| a.cpp:3:4:or_to_and | a.cpp:3 | or_to_and | `if (a \\|\\| b)` | `if (a && b)` | 1/2 |  |" in text
    assert "a.cpp:5:4:lt_to_le |" not in text


def test_counts_and_score():
    text = report.render("p", DATA)
    assert "| survived | 2 |" in text and "| killed | 3 |" in text and "| build_error | 1 |" in text
    assert "Mutation score (killed + timeout over decided): 60.0%" in text


def test_uncovered_lines_and_baseline_failures():
    text = report.render("p", DATA)
    assert "- a.cpp: 10-12, 20" in text
    assert "- b.cpp:" not in text
    assert "- TestX" in text


def test_kill_matrix_and_redundancy():
    matrix = report.kill_matrix(DATA)
    assert matrix["TestA::f"] == ["a.cpp:5:4:lt_to_le", "a.cpp:7:4:negate_if"]
    assert report.redundancy(DATA) == (["TestD"], ["TestB::g"])


def test_fail_fast_runs_are_flagged():
    assert "only hints" in report.render("p", DATA, fail_fast=True)
    assert "only hints" not in report.render("p", DATA)


def test_write_creates_the_report_and_the_matrix(tmp_path):
    project = SimpleNamespace(name="p", run_dir=tmp_path / "run", raw={})
    out = report.write(project, DATA, tmp_path / "r.md")
    assert out == tmp_path / "r.md" and out.read_text().startswith("# Mutation report: p")
    assert json.loads((tmp_path / "r-kill-matrix.json").read_text())["TestC::h"] == ["a.cpp:9:4:eq_to_ne"]
    assert report.write(project, DATA) == tmp_path / "run" / "report.md"
```

- [ ] **Step 2: Run them and watch them fail**

Run: `timeout 540 .venv/bin/python -m pytest tools/mutation/tests/test_report.py -q -p no:cacheprovider`
Expected: collection error, `No module named 'report'`.

- [ ] **Step 3: Write `tools/mutation/report.py`**

```python
"""The per-run report: survivors to triage, uncovered lines, kill-matrix summary."""
import itertools
import json
from collections import Counter
from pathlib import Path

STATUSES = ("pending", "killed", "timeout", "survived", "build_error", "equivalent", "interrupted")
TRIAGE = "real bug / weak test / dead code / equivalent"


def ranges(numbers):
    runs = itertools.groupby(enumerate(sorted(numbers)), key=lambda pair: pair[1] - pair[0])
    spans = [[n for _, n in run] for _, run in runs]
    return ", ".join(str(s[0]) if len(s) == 1 else f"{s[0]}-{s[-1]}" for s in spans)


def cell(text):
    return "`" + text.strip().replace("|", "\\|") + "`"


def kill_matrix(data):
    matrix = {}
    for mid, m in sorted(data["mutants"].items()):
        for killer in m.get("killed_by", []):
            matrix.setdefault(killer, []).append(mid)
    return matrix


def _unit(killer):
    return killer.split("::", 1)[0]


def redundancy(data):
    """Test units that ran against mutants and killed none, and killers with no kill of their own."""
    matrix = kill_matrix(data)
    ran = {t for m in data["mutants"].values() if m["status"] in ("killed", "survived", "timeout") for t in m["tests"]}
    sole = {m["killed_by"][0] for m in data["mutants"].values() if len(m.get("killed_by", [])) == 1}
    return sorted(ran - {_unit(k) for k in matrix}), sorted(k for k in matrix if k not in sole)


def _survivor_rows(data):
    survivors = sorted((m["path"], m["line"], mid, m) for mid, m in data["mutants"].items() if m["status"] == "survived")
    return [f"| {mid} | {m['path']}:{m['line']} | {m['op']} | {cell(m['before'])} | {cell(m['after'])} "
            f"| {len(m['tests'])}/{m.get('covering', len(m['tests']))} |  |"
            for _, _, mid, m in survivors]


def _bullets(items):
    return [f"- {item}" for item in items] or ["- none"]


def render(name, data, fail_fast=False):
    counts = Counter(m["status"] for m in data["mutants"].values())
    decided = counts["killed"] + counts["timeout"] + counts["survived"]
    score = f"{100 * (counts['killed'] + counts['timeout']) / decided:.1f}%" if decided else "n/a"
    never, no_unique = redundancy(data)
    uncovered = [f"{path}: {ranges(lines)}" for path, lines in sorted(data["uncovered"].items()) if lines]
    failing = sorted(n for n, e in data["tests"].items() if e.get("status") != "passed")
    return "\n".join([
        f"# Mutation report: {name}", "",
        "| status | count |", "|---|---|", *(f"| {s} | {counts[s]} |" for s in STATUSES), "",
        f"Mutation score (killed + timeout over decided): {score}", "",
        "## Survivors", "",
        f"One row, one follow-up. Fill the triage column: {TRIAGE}. "
        "'tests run' is run/covering: below covering means the run was capped.", "",
        "| id | file:line | operator | before | after | tests run | triage |",
        "|---|---|---|---|---|---|---|", *_survivor_rows(data), "",
        "## Uncovered target lines", "", "A missing test or dead code.", "", *_bullets(uncovered), "",
        "## Baseline failures", "", "Left out of every mutant run.", "", *_bullets(failing), "",
        "## Kill matrix", "", "The full matrix is the -kill-matrix.json file next to this report.", "",
        *(["Runs stopped at the first kill: the lists below are only hints.", ""] if fail_fast else []),
        "Ran against mutants and killed none (removal candidates if they cover nothing unique):", "",
        *_bullets(never), "",
        "Killed only mutants that other tests killed too:", "", *_bullets(no_unique), "",
    ])


def write(project, data, out=None):
    out = Path(out) if out else project.run_dir / "report.md"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(render(project.name, data, bool(project.raw.get("stop_on_first_kill"))), encoding="utf-8")
    out.with_name(out.stem + "-kill-matrix.json").write_text(json.dumps(kill_matrix(data), indent=1), encoding="utf-8")
    return out
```

- [ ] **Step 4: Run the tests and watch them pass**

Run: `timeout 540 .venv/bin/python -m pytest tools/mutation/tests -q -p no:cacheprovider`
Expected: `54 passed` (47 + 7), exit code 0.

- [ ] **Step 5: Commit**

```bash
git add tools/mutation/report.py tools/mutation/tests/test_report.py
git commit -m "feat(mutation): markdown survivor report with a triage column, and the kill matrix" -m "Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 6: The command line: one lock per build dir, signals, exit codes

**Files:**
- Create: `tools/mutation/mutate.py`
- Test: `tools/mutation/tests/test_cli.py`

**Interfaces:**
- Consumes: `config.load`, `covermap.cover/build_or_die`, `runner.plan/run/try_mutant`, `report.write`, `state.load/restore_leftovers`, `proc.install_signal_handlers/Interrupted`.
- Produces: `python tools/mutation/mutate.py --config <toml> --project <name> <command>`.
  - Commands: `cover [--reset] [--budget S]`, `plan`, `run [--budget S]`, `report [--out FILE]`, `restore`, `try --path P --line N --op OP --tests T [T ...]`.
  - Default budget: 520 s.
  - Exit codes: 0 done; 10 work left (run the same command again); 3 interrupted, sources restored; 4 another command holds this build dir's lock; 2 usage error (argparse).
  - A missing `require_env` variable aborts before anything runs, with a message that names `xvfb-run -a`.

- [ ] **Step 1: Write the failing tests**

`tools/mutation/tests/test_cli.py`:

```python
import fcntl
import shutil
import signal
import subprocess
import sys
import time
from pathlib import Path

import config
import operators as ops
import covermap
import state as st
from fixture_repo import init_repo

FIXTURE = Path(__file__).parent / "fixture"
MUTATE = Path(__file__).resolve().parents[1] / "mutate.py"

CONFIG = """
root = "."

[[project]]
name = "slow"
repo = "repo"
base = "main"
build_dir = "build"
build_cmd = ["true"]
test_cmd = ["sleep", "60"]
failure_parser = "qtest"

[[project.target]]
path = "calc.cpp"

[[project]]
name = "needs"
repo = "repo"
base = "main"
build_dir = "build-needs"
require_env = ["MUTATION_TEST_NEVER_SET"]
build_cmd = ["true"]

[[project.target]]
path = "calc.cpp"
"""


def _slow(tmp_path):
    repo = tmp_path / "repo"
    shutil.copytree(FIXTURE, repo, ignore=shutil.ignore_patterns("__pycache__"))
    init_repo(repo)
    (tmp_path / "phase.toml").write_text(CONFIG)
    project = config.load(tmp_path / "phase.toml", "slow")
    mutant = next(m for m in ops.mutants_for_line("calc.cpp", 3, "    if (i < 0)") if m.op == "negate_if")
    data = st.new_state()
    data["tests"] = {"t": {"status": "passed", "seconds": 1.0}}
    data["files"] = data["cover_files"] = covermap.source_shas(project)
    data["mutants"] = {mutant.id: {"path": mutant.path, "line": mutant.line, "col": mutant.col, "op": mutant.op,
                                   "before": mutant.before, "after": mutant.after, "status": "pending", "tests": ["t"],
                                   "covering": 1, "killed_by": [], "seconds": 0.0, "build_seconds": 0.0, "attempts": 0}}
    st.save(project.state_path, data)
    return project


def _cli(tmp_path, project, *args):
    return [sys.executable, str(MUTATE), "--config", str(tmp_path / "phase.toml"), "--project", project, *args]


def test_sigterm_mid_mutant_restores_and_exits_3(tmp_path):
    project = _slow(tmp_path)
    source = project.repo / "calc.cpp"
    original = st.read_source(source)
    child = subprocess.Popen(_cli(tmp_path, "slow", "run", "--budget", "600"))
    deadline = time.monotonic() + 20
    while st.read_source(source) == original:
        assert time.monotonic() < deadline, "the run never wrote the mutant"
        time.sleep(0.05)
    child.send_signal(signal.SIGTERM)
    assert child.wait(timeout=20) == 3
    assert st.read_source(source) == original
    data = st.load(project.state_path)
    assert data["in_flight"] == []
    entry = next(iter(data["mutants"].values()))
    assert entry["status"] == "pending" and entry["attempts"] == 1


def test_a_second_command_on_the_same_build_dir_is_refused(tmp_path):
    project = _slow(tmp_path)
    with open(project.run_dir / "lock", "w") as handle:
        fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        result = subprocess.run(_cli(tmp_path, "slow", "report"), capture_output=True, text=True, timeout=30)
    assert result.returncode == 4
    assert "another mutation run" in result.stderr


def test_a_missing_required_env_var_stops_before_anything_runs(tmp_path):
    _slow(tmp_path)
    result = subprocess.run(_cli(tmp_path, "needs", "plan"), capture_output=True, text=True, timeout=30)
    assert result.returncode != 0
    assert "MUTATION_TEST_NEVER_SET" in result.stderr and "xvfb-run -a" in result.stderr


def test_report_writes_the_file(tmp_path):
    _slow(tmp_path)
    out = tmp_path / "r.md"
    result = subprocess.run(_cli(tmp_path, "slow", "report", "--out", str(out)), capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stderr
    assert out.read_text().startswith("# Mutation report: slow")
    assert (tmp_path / "r-kill-matrix.json").exists()
```

- [ ] **Step 2: Run them and watch them fail**

Run: `timeout 540 .venv/bin/python -m pytest tools/mutation/tests/test_cli.py -q -p no:cacheprovider`
Expected: 4 failures. The child exits 2 with `can't open file '.../mutate.py'`.

- [ ] **Step 3: Write `tools/mutation/mutate.py`**

```python
"""Mutation testing for SciQLopPlots and NeoQCP (docs/plans/2026-10-22-mutation-testing-design.md).

    python tools/mutation/mutate.py --config tools/mutation/phase1.toml --project neoqcp <command>

Commands, in order:
    cover [--reset] [--budget S]   per-test coverage of the targets, and the baseline
    plan                           mutants on the covered target lines
    run [--budget S]               apply, build, test, restore; resumable
    report [--out FILE]            survivors to triage, uncovered lines, kill matrix
    restore                        put back a file a killed run left mutated, then rebuild
    try --path P --line N --op OP --tests T [T ...]   one mutant of the current source

Exit codes: 0 done; 10 work left, run the same command again; 3 interrupted,
sources restored, run again; 4 another command holds this build dir's lock.
"""
import argparse
import fcntl
import os
import sys
from contextlib import contextmanager

import config
import covermap
import proc
import report
import runner
import state as st

MORE, INTERRUPTED, LOCKED = 10, 3, 4
DEFAULT_BUDGET = 520  # under `timeout -k 20 570`, which stays under the 600 s shell limit


@contextmanager
def build_dir_lock(project):
    project.run_dir.mkdir(parents=True, exist_ok=True)
    with open(project.run_dir / "lock", "w") as handle:
        try:
            fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            print(f"another mutation run holds {project.build_dir}", file=sys.stderr)
            sys.exit(LOCKED)
        yield


def _left(count, what):
    print(f"{what}: {count} left" + (", run the same command again" if count else ", done"))
    return MORE if count else 0


def _plan(project, args):
    data = runner.plan(project)
    for target in project.targets:
        mine = [m for m in data["mutants"].values() if m["path"] == target.path]
        print(f"{target.path}: {len(mine)} mutants, {len(data['uncovered'].get(target.path, []))} uncovered code lines")
    failing = sorted(n for n, e in data["tests"].items() if e["status"] != "passed")
    print(f"baseline failures, left out of every mutant run: {failing or 'none'}")
    return 0


def _report(project, args):
    print(report.write(project, st.load(project.state_path), args.out))
    return 0


def _restore(project, args):
    data = st.load(project.state_path)
    print("restored:", st.restore_leftovers(project.state_path, data) or "nothing")
    covermap.build_or_die(project)
    return 0


def _try(project, args):
    result = runner.try_mutant(project, args.path, args.line, args.op, args.tests)
    print(f"{result['status']}: {', '.join(result.get('killed_by', [])) or 'no test failed'}")
    return 0


COMMANDS = {
    "cover": lambda project, args: _left(covermap.cover(project, args.budget, reset=args.reset), "cover"),
    "plan": _plan,
    "run": lambda project, args: _left(runner.run(project, args.budget), "run"),
    "report": _report,
    "restore": _restore,
    "try": _try,
}


def _parser():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True)
    parser.add_argument("--project", required=True)
    commands = parser.add_subparsers(dest="command", required=True)
    cover = commands.add_parser("cover")
    cover.add_argument("--reset", action="store_true")
    cover.add_argument("--budget", type=float, default=DEFAULT_BUDGET)
    commands.add_parser("plan")
    run = commands.add_parser("run")
    run.add_argument("--budget", type=float, default=DEFAULT_BUDGET)
    report_cmd = commands.add_parser("report")
    report_cmd.add_argument("--out")
    commands.add_parser("restore")
    trial = commands.add_parser("try")
    trial.add_argument("--path", required=True)
    trial.add_argument("--line", type=int, required=True)
    trial.add_argument("--op", required=True)
    trial.add_argument("--tests", nargs="+", required=True)
    return parser


def _require_env(project):
    missing = [name for name in project.raw.get("require_env", []) if name not in os.environ]
    if missing:
        sys.exit(f"{project.name} needs {', '.join(missing)}: run the command under xvfb-run -a")


def main(argv=None):
    args = _parser().parse_args(argv)
    project = config.load(args.config, args.project)
    _require_env(project)
    proc.install_signal_handlers()
    with build_dir_lock(project):
        try:
            return COMMANDS[args.command](project, args)
        except proc.Interrupted as why:
            print(f"interrupted ({why}); sources restored, run the same command again", file=sys.stderr)
            return INTERRUPTED


if __name__ == "__main__":
    sys.exit(main())
```

`sys.exit("message")` prints the message to stderr and exits 1. That is the `_require_env` path.

- [ ] **Step 4: Run the tests and watch them pass**

Run: `timeout 540 .venv/bin/python -m pytest tools/mutation/tests -q -p no:cacheprovider`
Expected: `58 passed` (54 + 4), exit code 0.

- [ ] **Step 5: Commit**

```bash
git add tools/mutation/mutate.py tools/mutation/tests/test_cli.py
git commit -m "feat(mutation): command line with a build-dir lock, signal-safe exit and resumable exit codes" -m "Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 7: Phase 1 config and the two coverage builds

**Files:**
- Create: `tools/mutation/phase1.toml`
- Test: `tools/mutation/tests/test_phase1_config.py`
- Modify: `.gitignore` (append a `build-cov` line after `build-venv`)
- Modify: `CLAUDE.md`, the one-time venv install line: append ` coverage` after `scipy`

**Interfaces:**
- Consumes: `config.load/target_lines/discover_tests`.
- Produces:
  - projects `neoqcp` and `sciqlopplots` in `tools/mutation/phase1.toml`;
  - built coverage dirs `subprojects/NeoQCP/build-cov` (target `auto-tests`) and `build-cov` (full build);
  - `coverage` installed in `.venv`.

- [ ] **Step 1: Write the failing config test**

`tools/mutation/tests/test_phase1_config.py`:

```python
from pathlib import Path

import pytest

import config

CONFIG = Path(__file__).resolve().parents[1] / "phase1.toml"


@pytest.mark.parametrize("name", ["neoqcp", "sciqlopplots"])
def test_every_target_exists_and_has_lines(name):
    project = config.load(CONFIG, name)
    for target in project.targets:
        assert (project.repo / target.path).is_file(), target.path
        assert config.target_lines(project, target), target.path


def test_neoqcp_discovers_the_qtest_classes():
    names = config.discover_tests(config.load(CONFIG, "neoqcp"))
    assert {"TestMultiGraph", "TestFillGeometry", "TestHistogramPlottable", "TestErrorBarGeometry", "TestPipeline"} <= set(names)
    assert len(names) == len(set(names)) > 40


def test_sciqlopplots_discovers_the_pytest_files_as_absolute_arguments():
    project = config.load(CONFIG, "sciqlopplots")
    names = config.discover_tests(project)
    assert "tests/integration/test_fills_errorbars.py" in names and len(names) > 150
    assert project.command("test_cmd", names[:1])[3] == str(project.root / names[0])


def test_the_python_target_is_the_band_error_bar_and_histogram_helpers():
    project = config.load(CONFIG, "sciqlopplots")
    target = next(t for t in project.targets if t.path.endswith(".py"))
    assert target.deploy == str(project.build_dir / "SciQLopPlots" / "__init__.py")
    assert 0 < len(config.target_lines(project, target)) < 200
```

Run: `timeout 540 .venv/bin/python -m pytest tools/mutation/tests/test_phase1_config.py -q -p no:cacheprovider`
Expected: FAIL, `FileNotFoundError` on `phase1.toml`.

- [ ] **Step 2: Write `tools/mutation/phase1.toml`**

```toml
# Mutation testing, phase 1: the code feat/fills-histogram added (fills, error
# bars, histogram, the histogram2d fixes, the pipeline fix).
# Plan: docs/plans/2026-10-25-mutation-testing-phase1-plan.md
# Strings are templates: {root} {repo} {build_dir} {scratch} {test} {env_NAME};
# a list item "{tests}" becomes one item per test; write a literal brace as {{ or }}.
# A survivor triaged as equivalent gets a [[project.equivalent]] entry under its
# project's targets: path, op, before (the exact original line) and reason.
root = "../.."

[[project]]
name = "neoqcp"
repo = "subprojects/NeoQCP"
base = "main"
build_dir = "subprojects/NeoQCP/build-cov"
require_env = ["DISPLAY"]
env = { PATH = "{root}/.venv/bin:/home/jeandet/Qt/6.11.2/gcc_64/bin:{env_PATH}", PKG_CONFIG_PATH = "/home/jeandet/Qt/6.11.2/gcc_64/lib/pkgconfig" }
build_cmd = ["{root}/.venv/bin/meson", "compile", "-C", "{build_dir}", "-j", "4", "auto-tests"]
build_timeout = 400
test_cwd = "{build_dir}"
test_env = { QT_QPA_PLATFORM = "xcb", QCP_TEST_CLASS = "{test}" }
test_cmd = ["{build_dir}/tests/auto/auto-tests"]
cover_test_cmd = ["{build_dir}/tests/auto/auto-tests"]
cover_keep_gcda = '(multigraph-fill|multigraph-errorbars|histogram-geometry|plottable-histogram|plottable-histogram2d|plottable-multigraph|plottable-draw-utils|async-pipeline|test-histogram1d|test-histogram-plottable)\.cpp\.gcda$'
cover_cmds = [["gcovr", "--root", "{repo}", "--object-directory", "{build_dir}", "--filter", "{repo}/src/", "--json", "{scratch}/gcovr.json", "-j", "4"]]
cover_reports = [{ kind = "gcovr", file = "{scratch}/gcovr.json", root = "{repo}" }]
failure_parser = "qtest"
tests = { regex_file = "subprojects/NeoQCP/tests/auto/autotest.cpp", pattern = 'QCPTEST\((\w+)\);' }

[[project.target]]
path = "src/plottables/multigraph-fill.cpp"

[[project.target]]
path = "src/plottables/multigraph-errorbars.cpp"

[[project.target]]
path = "src/plottables/histogram-geometry.cpp"

[[project.target]]
path = "src/plottables/plottable-histogram.cpp"

[[project.target]]
path = "src/datasource/histogram-binner-1d.h"

[[project.target]]
path = "src/plottables/plottable-multigraph.cpp"
scope = "diff"

[[project.target]]
path = "src/plottables/plottable-histogram2d.cpp"
scope = "diff"

[[project.target]]
path = "src/plottables/plottable-draw-utils.cpp"
scope = "diff"

[[project.target]]
path = "src/datasource/async-pipeline.cpp"
scope = "diff"

[[project]]
name = "sciqlopplots"
repo = "."
base = "main"
build_dir = "build-cov"
env = { PATH = "{root}/.venv/bin:/home/jeandet/Qt/6.11.2/gcc_64/bin:{env_PATH}", PKG_CONFIG_PATH = "/home/jeandet/Qt/6.11.2/gcc_64/lib/pkgconfig", LLVM_INSTALL_DIR = "/usr" }
build_cmd = ["{root}/.venv/bin/meson", "compile", "-C", "{build_dir}", "-j", "4", "SciQLopPlotsBindings"]
build_timeout = 400
test_cwd = "/tmp"
test_env = { QT_QPA_PLATFORM = "offscreen", PYTHONPATH = "{build_dir}:{root}" }
batch_tests = true
stop_on_first_kill = true
max_tests = 25
test_cmd = ["{root}/.venv/bin/python", "-m", "pytest", "{tests}", "-q", "-p", "no:cacheprovider", "-rfE", "-x"]
cover_test_cmd = ["{root}/.venv/bin/python", "-m", "coverage", "run", "--data-file={scratch}/.coverage", "--include={build_dir}/SciQLopPlots/__init__.py", "-m", "pytest", "{tests}", "-q", "-p", "no:cacheprovider"]
cover_keep_gcda = '(SciQLopHistogram|SciQLopHistogram2D|SciQLopMultiGraphBase|SciQLopGraphComponent|SciQLopGraphComponentDelegate)\.cpp\.gcda$'
cover_cmds = [
  ["gcovr", "--root", "{repo}", "--object-directory", "{build_dir}", "--filter", "{repo}/src/", "--json", "{scratch}/gcovr.json", "-j", "4"],
  ["{root}/.venv/bin/python", "-m", "coverage", "json", "--data-file={scratch}/.coverage", "-o", "{scratch}/coveragepy.json"],
]
cover_reports = [
  { kind = "gcovr", file = "{scratch}/gcovr.json", root = "{repo}" },
  { kind = "coveragepy", file = "{scratch}/coveragepy.json", root = "/tmp" },
]
failure_parser = "pytest"
tests = { glob = ["tests/integration/test_*.py", "tests/unit/test_*.py", "tests/fuzzing/test_*.py"], prefix = "{root}/" }

[[project.target]]
path = "src/SciQLopHistogram.cpp"

[[project.target]]
path = "src/SciQLopHistogram2D.cpp"
scope = "diff"

[[project.target]]
path = "src/SciQLopMultiGraphBase.cpp"
scope = "diff"

[[project.target]]
path = "src/SciQLopGraphComponent.cpp"
scope = "diff"

[[project.target]]
path = "src/SciQLopGraphComponentDelegate.cpp"
scope = "diff"

[[project.target]]
path = "SciQLopPlots/__init__.py"
functions = ["_error_bounds", "_band_batch", "_error_batch", "_band", "_error_bars", "_configure_histogram", "_histogram_calling"]
deploy = "{build_dir}/SciQLopPlots/__init__.py"
```

Why the two projects differ:
- SciQLopPlots runs fail-fast (`-x`, `stop_on_first_kill`) and caps at the 25 fastest covering files (`max_tests`). Common lines are covered by most of the 200 test files, and a full run per mutant would not fit in a chunk.
- NeoQCP runs every covering class, so its kill matrix is complete.

- [ ] **Step 3: Run the config tests, then the whole tool suite**

Run: `timeout 540 .venv/bin/python -m pytest tools/mutation/tests -q -p no:cacheprovider`
Expected: `63 passed` (58 + 5), exit code 0.

- [ ] **Step 4: Ignore build-cov and install coverage.py**

1. Edit `.gitignore`: add a line `build-cov` after the `build-venv` line.
2. Edit `CLAUDE.md`, the one-time venv install line: `numpy meson==1.10.2 ninja meson-python pytest pytest-qt scipy` becomes `numpy meson==1.10.2 ninja meson-python pytest pytest-qt scipy coverage`.
3. Run:

```bash
VIRTUAL_ENV=/home/jeandet/Documents/prog/SciQLopPlots/.venv uv pip install coverage
/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m coverage --version
```

Expected: a `Coverage.py, version 7.x` line.

- [ ] **Step 5: Configure and build NeoQCP's coverage dir**

```bash
cd /home/jeandet/Documents/prog/SciQLopPlots/subprojects/NeoQCP
export PATH="/home/jeandet/Documents/prog/SciQLopPlots/.venv/bin:/home/jeandet/Qt/6.11.2/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.2/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
meson setup build-cov -Db_coverage=true -Ddebug=true -Doptimization=g -Dcpp_args=-fprofile-update=atomic -Dwith_tests=true -Dwith_examples=false
```

Why these options:
- `-Og` keeps gcov's line numbers close to the source without `-O0`'s slow tests.
- `-fprofile-update=atomic` keeps the counters right when histogram binning runs on worker threads.

Then build only the test binary. Repeat this one command, each time as its own foreground call, until it exits 0:

```bash
timeout 570 meson compile -C build-cov -j 4 auto-tests; echo "exit=$?"
```

`exit=124` means the time ran out: run it again, and ninja continues. If meson says the target name is ambiguous, use `tests/auto/auto-tests` here and in `phase1.toml`'s `build_cmd`.

- [ ] **Step 6: Smoke-test NeoQCP's coverage build**

```bash
cd /home/jeandet/Documents/prog/SciQLopPlots/subprojects/NeoQCP/build-cov
xvfb-run -a -s "-screen 0 1280x1024x24" timeout 300 env QT_QPA_PLATFORM=xcb QCP_TEST_CLASS=TestFillGeometry ./tests/auto/auto-tests; echo "exit=$?"
find . -name '*multigraph-fill.cpp.gcda'
ps -eo pid,ppid,cmd | awk '/[X]vfb/ && $2==1'
```

Expected:
- `Totals: N passed, 0 failed`, `exit=0`;
- one `.gcda` path whose file name ends in `multigraph-fill.cpp.gcda`, which matches `cover_keep_gcda`;
- no orphan Xvfb.

If the `.gcda` names differ, fix `cover_keep_gcda` in `phase1.toml`.

- [ ] **Step 7: Configure and build SciQLopPlots' coverage dir**

```bash
cd /home/jeandet/Documents/prog/SciQLopPlots
export PATH="$PWD/.venv/bin:/home/jeandet/Qt/6.11.2/gcc_64/bin:$PATH"
export PKG_CONFIG_PATH="/home/jeandet/Qt/6.11.2/gcc_64/lib/pkgconfig:$PKG_CONFIG_PATH"
export LLVM_INSTALL_DIR=/usr
.venv/bin/meson setup build-cov -Db_coverage=true -Ddebug=true -Doptimization=g -Dcpp_args=-fprofile-update=atomic
```

Then the full build. Repeat each time as its own call until it exits 0. The first full build takes several chunks:

```bash
timeout 570 .venv/bin/meson compile -C build-cov -j 4; echo "exit=$?"
```

Then check that the mutation build target resolves, and runs as a no-op:

```bash
timeout 570 .venv/bin/meson compile -C build-cov -j 4 SciQLopPlotsBindings; echo "exit=$?"
```

If the name is ambiguous, use `SciQLopPlots/SciQLopPlotsBindings` here and in `phase1.toml`.

- [ ] **Step 8: Smoke-test SciQLopPlots' coverage build**

```bash
cd /tmp && QT_QPA_PLATFORM=offscreen PYTHONPATH=/home/jeandet/Documents/prog/SciQLopPlots/build-cov:/home/jeandet/Documents/prog/SciQLopPlots timeout 540 /home/jeandet/Documents/prog/SciQLopPlots/.venv/bin/python -m pytest /home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_fills_errorbars.py /home/jeandet/Documents/prog/SciQLopPlots/tests/integration/test_histogram1d.py -q -p no:cacheprovider; echo "exit=$?"
find /home/jeandet/Documents/prog/SciQLopPlots/build-cov -name 'SciQLopHistogram.cpp.gcda'
```

Expected:
- all tests pass, `exit=0`;
- one `.gcda` path, which matches `cover_keep_gcda`.

If the run imported the wrong build, `python -c "import SciQLopPlots; print(SciQLopPlots.__file__)"` with the same PYTHONPATH must print a path under `build-cov`.

- [ ] **Step 9: Commit**

```bash
cd /home/jeandet/Documents/prog/SciQLopPlots
git add tools/mutation/phase1.toml tools/mutation/tests/test_phase1_config.py .gitignore CLAUDE.md
git commit -m "feat(mutation): phase 1 targets (fills, error bars, histogram, pipeline fix) and coverage builds" -m "Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 8: Coverage map, baseline and mutant plan for phase 1

**Files:**
- Modify: `tools/mutation/phase1.toml`, only if a step below finds a wrong regex or target name
- Modify: `docs/plans/2026-10-22-mutation-testing-design.md`, the `Status:` line
- Output (not committed): `$SCRATCH/mutation-phase1-neoqcp-coverage.md`, `$SCRATCH/mutation-phase1-sciqlopplots-coverage.md`

**Interfaces:**
- Consumes: the Task 6 CLI and the Task 7 builds.
- Produces: `subprojects/NeoQCP/build-cov/mutation/state.json` and `build-cov/mutation/state.json`, with every test covered and every mutant planned.

- [ ] **Step 1: NeoQCP coverage, in chunks**

Repeat this as its own foreground call until it prints `exit=0`. `exit=10` or `exit=3` means: run it again.

```bash
cd /home/jeandet/Documents/prog/SciQLopPlots
xvfb-run -a -s "-screen 0 1280x1024x24" timeout -k 20 570 .venv/bin/python tools/mutation/mutate.py --config tools/mutation/phase1.toml --project neoqcp cover; echo "exit=$?"
ps -eo pid,ppid,cmd | awk '/[X]vfb/ && $2==1'
git status --porcelain --untracked-files=no; git -C subprojects/NeoQCP status --porcelain --untracked-files=no
```

There are about 45 classes, so expect two or three chunks.

- [ ] **Step 2: NeoQCP plan, and check it**

```bash
xvfb-run -a .venv/bin/python tools/mutation/mutate.py --config tools/mutation/phase1.toml --project neoqcp plan
```

`plan` does not build, but `require_env` needs DISPLAY, hence `xvfb-run`. Check the output:
- **Every target has mutants.** A target with 0 mutants and many uncovered lines means the counters were pruned away. Fix `cover_keep_gcda` (Task 7 Step 6 shows the real `.gcda` names), then `cover --reset` and redo Step 1.
- **Baseline failures are `none`.** A failing class is a bug found in passing. Do not mutate around it:
  1. stop and report it to the controller;
  2. it gets its own reproducer and fix commit in NeoQCP;
  3. then `cover --reset`.

- [ ] **Step 3: SciQLopPlots coverage, in chunks, then the plan**

No display is needed: the tests run offscreen. Repeat until `exit=0`:

```bash
cd /home/jeandet/Documents/prog/SciQLopPlots
timeout -k 20 570 .venv/bin/python tools/mutation/mutate.py --config tools/mutation/phase1.toml --project sciqlopplots cover; echo "exit=$?"
git status --porcelain --untracked-files=no
```

There are about 200 test files, so expect four to six chunks. Then:

```bash
.venv/bin/python tools/mutation/mutate.py --config tools/mutation/phase1.toml --project sciqlopplots plan
```

Make the same checks as in Step 2. Also check that `SciQLopPlots/__init__.py` has mutants: 0 there means coverage.py's paths did not match the deploy copy.

- [ ] **Step 4: Write the coverage-only reports**

```bash
.venv/bin/python tools/mutation/mutate.py --config tools/mutation/phase1.toml --project sciqlopplots report --out "$SCRATCH/mutation-phase1-sciqlopplots-coverage.md"
xvfb-run -a .venv/bin/python tools/mutation/mutate.py --config tools/mutation/phase1.toml --project neoqcp report --out "$SCRATCH/mutation-phase1-neoqcp-coverage.md"
```

Read both "Uncovered target lines" sections. They are the first finding of phase 1. Each range is a missing test or dead code. Say how many there are in your report to the controller.

- [ ] **Step 5: Run the tool suite, then commit**

Run the tool suite (Global Constraints): `63 passed`, exit 0. Then edit the design doc's `Status:` line. Append, filling in the real numbers: `Phase 1 tool done (tools/mutation/, plan docs/plans/2026-10-25-mutation-testing-phase1-plan.md); coverage mapped 2026-MM-DD: NeoQCP <M> mutants / <U> uncovered target lines, SciQLopPlots <M> / <U>.`

```bash
git add docs/plans/2026-10-22-mutation-testing-design.md
git add tools/mutation/phase1.toml   # only if Steps 2-3 changed it
git commit -m "docs(mutation): phase 1 coverage mapped and mutants planned" -m "Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

---

### Task 9: Run phase 1 in chunks, write the survivor reports, hand off triage

**Files:**
- Modify: `docs/plans/2026-10-22-mutation-testing-design.md`, the `Status:` line
- Modify: `tools/mutation/phase1.toml`, only for `build_timeout`/`max_tests` if a step below says so
- Output (not committed): `$SCRATCH/mutation-phase1-neoqcp.md`, `$SCRATCH/mutation-phase1-sciqlopplots.md`, and their `-kill-matrix.json` files

**Interfaces:**
- Consumes: the Task 8 state files.
- Produces: decided mutants in both state files, the two reports, and the triage hand-off below.

- [ ] **Step 1: NeoQCP mutants, in chunks**

Repeat this as its own foreground call until it prints `run: 0 left` and `exit=0`:

```bash
cd /home/jeandet/Documents/prog/SciQLopPlots
xvfb-run -a -s "-screen 0 1280x1024x24" timeout -k 20 570 .venv/bin/python tools/mutation/mutate.py --config tools/mutation/phase1.toml --project neoqcp run; echo "exit=$?"
ps -eo pid,ppid,cmd | awk '/[X]vfb/ && $2==1'
git status --porcelain --untracked-files=no; git -C subprojects/NeoQCP status --porcelain --untracked-files=no
```

Expect several hundred mutants at roughly a minute each, so many chunks. The work may span sessions: the state file carries it over.

After the first two chunks, read the state:

```bash
.venv/bin/python -c "import json; d=json.load(open('subprojects/NeoQCP/build-cov/mutation/state.json')); import collections; print(collections.Counter(m['status'] for m in d['mutants'].values()))"
```

- If `build_error` is more than about a third of the decided mutants, an operator is making junk. Report which `op` dominates before going on.
- If a mutant reaches `interrupted`, one mutant takes longer than a chunk. Raise `build_timeout` or lower the class count, then report it.

- [ ] **Step 2: NeoQCP report**

```bash
xvfb-run -a .venv/bin/python tools/mutation/mutate.py --config tools/mutation/phase1.toml --project neoqcp report --out "$SCRATCH/mutation-phase1-neoqcp.md"
```

- [ ] **Step 3: SciQLopPlots mutants, in chunks, then the report**

Repeat until `run: 0 left` and `exit=0`:

```bash
timeout -k 20 570 .venv/bin/python tools/mutation/mutate.py --config tools/mutation/phase1.toml --project sciqlopplots run; echo "exit=$?"
git status --porcelain --untracked-files=no
```

Each SciQLopPlots mutant relinks the bindings module, so expect about two minutes per mutant. Then:

```bash
.venv/bin/python tools/mutation/mutate.py --config tools/mutation/phase1.toml --project sciqlopplots report --out "$SCRATCH/mutation-phase1-sciqlopplots.md"
```

- [ ] **Step 4: Status line, tool suite, commit**

Run the tool suite: `63 passed`, exit 0. Append to the design doc's `Status:` line: `Phase 1 run 2026-MM-DD: NeoQCP score <S>% (<K> killed, <V> survived), SciQLopPlots <S>% (<K>/<V>); reports in the session scratchpad; survivors handed off for triage.`

```bash
git add docs/plans/2026-10-22-mutation-testing-design.md
git add tools/mutation/phase1.toml   # only if Step 1 changed it
git commit -m "docs(mutation): phase 1 results" -m "Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>"
```

- [ ] **Step 5: Hand off to the controller**

Report:
- the two report paths;
- the status counts and scores;
- the number of uncovered line ranges;
- the baseline failures;
- anything odd (build_error ratio, interrupted mutants, orphan Xvfb).

Do not triage the survivors yourself.

---

## After Task 9: survivor triage (follow-ups, not part of this plan)

Each survivor row in a report is one follow-up. The controller groups rows by function and dispatches one follow-up per group. Each follow-up fills the triage column with one of four outcomes:

1. **Real bug.** Write a failing test in the owning repo's suite. For NeoQCP that is a QtTest function in `tests/auto/`; for SciQLopPlots it is a pytest test. Fix the code, then commit. NeoQCP commits go in NeoQCP. A SciQLopPlots change that needs a NeoQCP fix waits for the NeoQCP commit first.
2. **Weak test.** Add or tighten a test so it kills the mutant. Check that it does, against the current source:
   ```bash
   xvfb-run -a timeout -k 20 570 .venv/bin/python tools/mutation/mutate.py --config tools/mutation/phase1.toml --project neoqcp try --path <file> --line <n> --op <op> --tests <TestClass>
   ```
   For SciQLopPlots, use `--project sciqlopplots` and `--tests tests/integration/<file>.py`, without `xvfb-run`. Expected output: `killed: ...`. Then commit the test.
3. **Dead or redundant code.** Remove it. Both repos' full suites are the guard.
4. **Equivalent mutant.** Add a `[[project.equivalent]]` entry (path, op, the exact `before` line, reason) under the project's targets in `phase1.toml`, then commit.

Units in the kill-matrix lists ("killed none", "no kill of their own") are removal candidates, not removals. A follow-up removes a test only after checking that it covers nothing unique.

After a batch of follow-ups, re-run the affected project in this order:
1. `cover --reset`;
2. `plan`, which re-makes the mutants of changed files and re-queues survivors a new test covers;
3. `run`;
4. `report`.

Phases 2 and 3 of the design reuse the tool with a new `phaseN.toml`.
