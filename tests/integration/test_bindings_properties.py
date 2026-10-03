"""Every <property> in bindings.xml must reach C++.

A property named like a C++ getter is hidden by that method: `obj.color = x`
then sets a plain Python attribute and C++ never sees it. Rename the getter
(e.g. `color() const` -> `_color`) so the property wins.
"""
import pathlib
import xml.etree.ElementTree as ET

import pytest

import SciQLopPlots.SciQLopPlotsBindings as bindings

_XML = pathlib.Path(__file__).parents[2] / "SciQLopPlots" / "bindings" / "bindings.xml"

# simplify: renaming these getters breaks `span.color()` callers (SciQLop); decide first.
_KNOWN_DEAD = {f"MultiPlotsVerticalSpan.{n}" for n in ("color", "visible", "selected", "read_only", "id")}


def _declared_properties():
    root = ET.parse(_XML).getroot()
    return [(t.get("name"), p.get("name"))
            for t in root.iter() if t.tag in ("object-type", "value-type")
            for p in t.findall("property")]


def _params():
    for cls, prop in _declared_properties():
        marks = [pytest.mark.xfail(reason="getter shadows the property", strict=True)] \
            if f"{cls}.{prop}" in _KNOWN_DEAD else []
        yield pytest.param(cls, prop, marks=marks, id=f"{cls}.{prop}")


@pytest.mark.parametrize("cls,prop", _params())
def test_property_is_a_descriptor(cls, prop):
    klass = getattr(bindings, cls)
    descriptor = next(c.__dict__[prop] for c in klass.__mro__ if prop in c.__dict__)
    assert type(descriptor).__name__ == "getset_descriptor"
