"""The signatures shiboken generates parse: an enum default must name its enum.

A default spelled ``= ::GraphMarkerShape::NoMarker`` in C++ rendered as
``.GraphMarkerShape.NoMarker`` (the leading ``::`` became a dot the parser cannot resolve), and
every signature holding one warned "UNRECOGNIZED" when read (help(), .pyi stubs, error
messages).
"""
import re
import warnings

from shibokensupport.signature import get_signature

from SciQLopPlots import SciQLopPlotsBindings


def _bound_callables():
    for obj in vars(SciQLopPlotsBindings).values():
        yield obj
        if isinstance(obj, type):
            yield from vars(obj).values()


def _unrecognized_values():
    with warnings.catch_warnings(record=True) as caught:
        warnings.simplefilter("always")
        for obj in _bound_callables():
            try:
                get_signature(obj)
            except Exception:  # noqa: BLE001 - not every attribute has a signature
                pass
    found = (re.search(r"UNRECOGNIZED:\s+'([^']*)'", str(w.message)) for w in caught)
    return {m.group(1) for m in found if m}


def test_no_enum_default_loses_its_enum():
    assert {v for v in _unrecognized_values() if v.startswith(".")} == set()
