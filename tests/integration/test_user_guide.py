"""Every Python snippet of docs/user-guide.md runs against the installed library.

Snippets are standalone scripts: each one runs in a fresh namespace. The test
already has a QApplication and must not block, so a snippet's
`QApplication(sys.argv)` becomes the existing instance and `app.exec()` is dropped.
"""
import pathlib
import re

import pytest
from conftest import process_events
from PySide6.QtWidgets import QWidget

GUIDE = pathlib.Path(__file__).resolve().parents[2] / "docs" / "user-guide.md"
SNIPPETS = re.findall(r"```python\n(.*?)```", GUIDE.read_text(), flags=re.S)


def _runnable_in_tests(code):
    return (code.replace("QApplication(sys.argv)", "QApplication.instance()")
                .replace("app.exec()", "None"))


def _first_line(code):
    return next(line for line in code.splitlines() if line.strip() and not line.startswith(("import", "from")))


def test_the_guide_has_snippets():
    assert len(SNIPPETS) >= 15


@pytest.mark.parametrize("code", SNIPPETS, ids=[_first_line(c)[:40] for c in SNIPPETS])
def test_snippet_runs(code, qtbot, tmp_path, monkeypatch):
    monkeypatch.chdir(tmp_path)
    namespace = {"__name__": "__snippet__"}
    exec(compile(_runnable_in_tests(code), "user-guide snippet", "exec"), namespace)
    for _ in range(10):
        process_events()
    for widget in [v for v in namespace.values() if isinstance(v, QWidget)]:
        widget.close()
