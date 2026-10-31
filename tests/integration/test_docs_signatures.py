"""The API page shows every overload of a method, also when a Python wrapper sits on it."""
import importlib.util
import inspect
from pathlib import Path

import pytest

from SciQLopPlots import SciQLopPlot, SciQLopPlotInterface


def _docs_conf():
    spec = importlib.util.spec_from_file_location("docs_conf", Path(__file__).parents[2] / "docs" / "conf.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def test_a_wrapped_overloaded_method_lists_its_overloads():
    signature, _ = _docs_conf().shiboken_signature(None, "method", "SciQLopPlot.ellipse", SciQLopPlot.ellipse,
                                                   None, None, None)
    assert len(signature.splitlines()) == 3 and "line_color" in signature


@pytest.mark.parametrize("method", [SciQLopPlot.ellipse, SciQLopPlotInterface.ellipse, SciQLopPlot.add_waterfall])
def test_a_wrapped_overloaded_method_is_inspectable_like_a_compiled_one(method):
    with pytest.raises(ValueError):  # what inspect says for any compiled method; a TypeError breaks help() and Sphinx
        inspect.signature(method)
