"""Every Python snippet of docs/user-guide.md runs against the installed library.

Snippets are standalone scripts: each one runs in a fresh namespace. The test
already has a QApplication and must not block, so a snippet's
`QApplication(sys.argv)` becomes the existing instance and `app.exec()` is dropped.
"""
import gc
import pathlib
import re

import pytest
from conftest import process_events
from PySide6.QtWidgets import QWidget
from SciQLopPlots import SciQLopGraphInterface

GUIDE = pathlib.Path(__file__).resolve().parents[2] / "docs" / "user-guide.md"
SNIPPETS = re.findall(r"```python\n(.*?)```", GUIDE.read_text(), flags=re.S)


def _runnable_in_tests(code):
    return (code.replace("QApplication(sys.argv)", "QApplication.instance()")
                .replace("app.exec()", "None"))


def _first_line(code):
    return next(line for line in code.splitlines() if line.strip() and not line.startswith(("import", "from")))


def test_the_guide_has_snippets():
    assert len(SNIPPETS) >= 15


def test_every_screenshot_exists():
    images = re.findall(r"!\[[^\]]*\]\(([^)]+)\)", GUIDE.read_text())
    assert images
    assert [i for i in images if not (GUIDE.parent / i).is_file()] == []


@pytest.mark.parametrize("code", SNIPPETS, ids=[_first_line(c)[:40] for c in SNIPPETS])
def test_snippet_runs(code, qtbot, tmp_path, monkeypatch):
    monkeypatch.chdir(tmp_path)
    namespace = {"__name__": "__snippet__"}
    exec(compile(_runnable_in_tests(code), "user-guide snippet", "exec"), namespace)
    for _ in range(10):
        process_events()
    # Data functions run on a worker thread with the namespace as their globals:
    # let them finish before it is cleared below.
    graphs = [v for v in namespace.values() if isinstance(v, SciQLopGraphInterface)]
    qtbot.waitUntil(lambda: not any(g.busy() for g in graphs), timeout=5000)
    for widget in [v for v in namespace.values() if isinstance(v, QWidget)]:
        widget.close()
    # A snippet's functions and lambdas hold the namespace through their globals,
    # and a Qt connection keeps them alive from C++: a cycle the GC cannot see.
    # Left alone, its plots survive to interpreter exit and die in PySide's
    # teardown, where they crashed the test process (SIGBUS/SIGSEGV after all
    # tests had passed). Release them here, like any fixture would.
    namespace.clear()
    gc.collect()
    for _ in range(10):
        process_events()
