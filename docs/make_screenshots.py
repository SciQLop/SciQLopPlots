"""Regenerate the user guide's screenshots from its own snippets.

A snippet gets a screenshot when an image link follows its code block:

    ```python
    ...
    ```

    ![First plot](images/first-plot.png)

Each snippet runs in a fresh namespace, like in tests/integration/test_user_guide.py.
Its first top-level widget is exported with save_png. Run it headless, then look at
every picture before committing it:

    QT_QPA_PLATFORM=offscreen PYTHONPATH=build-venv python docs/make_screenshots.py
"""
import pathlib
import re
import time

from PySide6.QtGui import QImage
from PySide6.QtWidgets import QApplication, QWidget

from SciQLopPlots import SciQLopGraphInterface

DOCS = pathlib.Path(__file__).resolve().parent
GUIDE = DOCS / "user-guide.md"
SHOT = re.compile(r"```python\n((?:(?!```).)*)```\s*\n!\[[^\]]*\]\(([^)]+)\)", flags=re.S)
SIZES = {"SciQLopMultiPlotPanel": (800, 600)}
DEFAULT_SIZE = (800, 400)


def runnable(code):
    return (code.replace("QApplication(sys.argv)", "QApplication.instance()")
                .replace("app.exec()", "None"))


def settle(app, namespace, seconds=1.0):
    graphs = [v for v in namespace.values() if isinstance(v, SciQLopGraphInterface)]
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline or any(g.busy() for g in graphs):
        app.processEvents()


def subject(namespace):
    return next(v for v in namespace.values()
                if isinstance(v, QWidget) and v.parent() is None and hasattr(v, "save_png"))


def is_blank(path):
    image = QImage(str(path))
    first = image.pixel(0, 0)
    return all(image.pixel(x, y) == first
               for x in range(0, image.width(), 7) for y in range(0, image.height(), 7))


def screenshot(app, code, path):
    namespace = {"__name__": "__snippet__"}
    exec(compile(runnable(code), str(path), "exec"), namespace)
    widget = subject(namespace)
    width, height = SIZES.get(type(widget).__name__, DEFAULT_SIZE)
    widget.resize(width, height)
    widget.show()
    settle(app, namespace)
    path.parent.mkdir(exist_ok=True)
    if not widget.save_png(str(path), width, height) or is_blank(path):
        raise RuntimeError(f"{path}: export failed or blank")
    widget.close()
    namespace.clear()


def main():
    app = QApplication.instance() or QApplication([])
    for code, image in SHOT.findall(GUIDE.read_text()):
        screenshot(app, code, DOCS / image)
        print("wrote", image)


if __name__ == "__main__":
    main()
