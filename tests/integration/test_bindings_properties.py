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


def _declared_properties():
    root = ET.parse(_XML).getroot()
    return [(t.get("name"), p.get("name"))
            for t in root.iter() if t.tag in ("object-type", "value-type")
            for p in t.findall("property")]


@pytest.mark.parametrize("cls,prop", _declared_properties(),
                         ids=[f"{c}.{p}" for c, p in _declared_properties()])
def test_property_is_a_descriptor(cls, prop):
    klass = getattr(bindings, cls)
    descriptor = next(c.__dict__[prop] for c in klass.__mro__ if prop in c.__dict__)
    assert type(descriptor).__name__ == "getset_descriptor"
