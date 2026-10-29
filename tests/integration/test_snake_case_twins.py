"""API review 1.5: camelCase names have a snake_case twin, the same attribute under a second name."""
import pytest

from SciQLopPlots.SciQLopPlotsBindings import PlotsModel, PlotsModelNode

TWINS = [(PlotsModel, "add_node", "addNode"),
         (PlotsModelNode, "set_name", "setName"),
         (PlotsModelNode, "name_changed", "nameChanged")]


@pytest.mark.parametrize("cls, twin, native", TWINS, ids=[t[1] for t in TWINS])
def test_the_twin_is_the_native_attribute(cls, twin, native):
    assert getattr(cls, twin) is getattr(cls, native)
