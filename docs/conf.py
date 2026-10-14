import inspect
import os
import re
from pathlib import Path

project = "SciQLopPlots"
author = "Alexis Jeandet"
copyright = "SciQLop contributors"
version = release = re.search(
    r"__version__\s*=\s*['\"]([^'\"]+)",
    (Path(__file__).parent.parent / "SciQLopPlots" / "__init__.py").read_text(),
).group(1)

extensions = ["myst_parser", "sphinx.ext.autodoc", "sphinx.ext.napoleon"]
source_suffix = {".md": "markdown"}
myst_heading_anchors = 3

# docs/ also holds internal notes (plans, backlogs, reviews): publish only the pages below.
_published = {"index.md", "user-guide.md", "api.md", "javascript.md"}
exclude_patterns = ["_build"] + [
    str(p.relative_to(Path(__file__).parent))
    for p in Path(__file__).parent.rglob("*.md")
    if p.name not in _published
]

html_theme = "sphinx_rtd_theme"
html_title = f"SciQLopPlots {release}"

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
autodoc_member_order = "groupwise"


def _qt_names():
    """Every attribute name of a Qt class (getattr: PySide creates its classes lazily, vars() misses them)."""
    from PySide6 import QtCore, QtGui, QtWidgets

    classes = (getattr(module, name) for module in (QtCore, QtGui, QtWidgets) for name in dir(module))
    return {name for cls in classes if isinstance(cls, type) for name in dir(cls)}


_QT_NAMES = _qt_names()


def _without_self(sig):
    params = list(sig.parameters.values())
    return sig.replace(parameters=params[1:]) if params and params[0].name == "self" else sig


def _format(sig, what):
    sig = _without_self(sig)
    if what == "class":
        sig = sig.replace(return_annotation=inspect.Signature.empty)
    return str(sig).replace("SciQLopPlotsBindings.", "")


def shiboken_signature(app, what, name, obj, options, signature, return_annotation):
    """Compiled methods hide their signatures from inspect; shiboken keeps them, overloads included."""
    if what not in ("method", "class") or inspect.isfunction(obj):
        return None
    from shibokensupport.signature import get_signature

    sigs = get_signature(obj)
    if not sigs:
        return None
    return "\n".join(_format(s, what) for s in (sigs if isinstance(sigs, list) else [sigs])), ""


def _from_qt(name, obj):
    """A method knows the class that defines it: skip it only when that is a Qt class, so a
    SciQLopPlots method named like a Qt one (clear, size, name...) stays. The Python functions
    SciQLopPlots/__init__.py adds are never Qt's. Signals and properties don't know their
    class: those go by name."""
    if inspect.isfunction(obj):
        return False
    owner = getattr(obj, "__objclass__", None)
    if owner is not None:
        return owner.__module__.startswith("PySide6")
    return name in _QT_NAMES


def skip_qt_and_internal_members(app, what, name, obj, skip, options):
    if what == "class" and _from_qt(name, obj):
        return True
    if what == "module" and name.startswith("tracing_"):
        return True  # raw bindings behind SciQLopPlots.tracing, documented there
    return None


def setup(app):
    app.connect("autodoc-process-signature", shiboken_signature)
    app.connect("autodoc-skip-member", skip_qt_and_internal_members)
