"""A Python function on a bound class, over a C++ virtual, is a user override for objects built
from Python: C++'s own calls to the virtual then land in it. The deprecated set_movable shim on
text items called set_read_only, whose C++ side calls set_movable again: endless recursion.
"""
import inspect
import pathlib
import re
import warnings

import pytest
from PySide6.QtCore import QPointF, QRectF
from PySide6.QtGui import QPixmap

import SciQLopPlots
import SciQLopPlots.SciQLopPlotsBindings as bindings
from SciQLopPlots import SciQLopEllipseItem, SciQLopPixmapItem, SciQLopTextItem

_WRAPPERS = pathlib.Path(SciQLopPlots.__file__).parent / "bindings" / "SciQLopPlotsBindings"


def _virtuals():
    """{class name: names it may override}, from the generated shiboken wrapper headers."""
    for header in _WRAPPERS.glob("*_wrapper.h"):
        text = header.read_text()
        cls = re.search(r"class (\w+)Wrapper\b", text)
        if cls:
            yield cls.group(1), set(re.findall(r"(\w+)\([^;]*\)[^;(]*\boverride;", text))


#: Wrappers C++ may call back: they only unwrap pointer handles, so a C++ call goes through
#: unchanged, and Python panels must keep overriding these (bindings.xml `final` would forbid it).
TRANSPARENT_WRAPPERS = {f"SciQLopMultiPlotPanel.{m}" for m in
                        ("add_plot", "remove_plot", "insert_plot", "move_plot", "index", "contains")}


def _shadowed_virtuals():
    for name, virtuals in _virtuals():
        cls = getattr(bindings, name, None)
        for method in sorted(virtuals) if cls else ():
            if inspect.isfunction(inspect.getattr_static(cls, method, None)):
                yield f"{name}.{method}"


@pytest.mark.skipif(not _WRAPPERS.is_dir(), reason="needs the generated wrappers (build tree)")
def test_no_python_wrapper_replaces_a_cpp_virtual():
    """Mark the virtual access="final" in bindings.xml, or do the work there in inject-code."""
    assert "SciQLopTextItem" in dict(_virtuals())  # the scan sees the wrappers
    assert sorted(set(_shadowed_virtuals()) - TRANSPARENT_WRAPPERS) == []


def _built_from_python(plot):
    with warnings.catch_warnings():
        warnings.simplefilter("ignore", DeprecationWarning)
        return [SciQLopTextItem(plot, "t", QPointF(1, 1)),
                SciQLopPixmapItem(plot, QPixmap(4, 4), QRectF(0, 0, 1, 1)),
                SciQLopEllipseItem(plot, QRectF(0, 0, 1, 1))]


def test_read_only_on_items_built_from_python(plot):
    for item in _built_from_python(plot):
        with warnings.catch_warnings():
            warnings.simplefilter("error")
            item.set_read_only(False)
            assert item.read_only() is False
        with pytest.warns(DeprecationWarning, match="set_read_only"):
            item.set_movable(False)
        assert item.read_only() is True
