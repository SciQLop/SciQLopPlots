"""Bound C++ enums are IntEnums: every member is listed, none are bit flags.

Without a python-type, shiboken exposed them as enum.Flag, so list(ColorGradient)
only held the single-bit values (Hot, Cold, Candy, Polar) and Hot | Cold was Night.
"""
import enum

import pytest

import SciQLopPlots

ENUMS = [
    "GraphType", "WaterfallOffsetMode", "PlotType", "AxisType", "GraphMarkerShape",
    "GraphLineStyle", "ColorGradient", "ProductsModelNodeType", "ParameterType",
    "Coordinates", "LineTermination", "OverlayLevel", "OverlaySizeMode",
    "OverlayPosition", "ScoreMergeStrategy", "QueryTokenKind",
]


@pytest.mark.parametrize("name", ENUMS)
def test_enum_is_an_int_enum_listing_every_member(name):
    e = getattr(SciQLopPlots, name)
    assert issubclass(e, enum.IntEnum)
    assert not issubclass(e, enum.Flag)
    assert len(list(e)) == len(e.__members__)


def test_int_comparisons_keep_working():
    from SciQLopPlots import ColorGradient, GraphType
    assert GraphType.Line == 0
    assert int(ColorGradient.Jet) == 10
    assert ColorGradient(10) is ColorGradient.Jet
    assert ColorGradient.Jet == ColorGradient.Jet
