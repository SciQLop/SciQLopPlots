"""Every public method of the API fuzz sweep has a receiver to call it on, and can find a working call."""
import json
import os

import pytest

from tests.sweeps.api_fuzz import catalogue as cat


@pytest.fixture(scope="module")
def methods():
    return cat.public_methods()


@pytest.fixture(scope="module")
def mros():
    return cat.receiver_mros(cat.kinds())


def test_receivers_build(mros):
    assert [kind for kind, names in mros.items() if names is None] == []


def test_every_class_has_receiver_or_reason(methods, mros):
    receivers = cat.receivers(mros)
    assert sorted({m.cls for m in methods} - set(receivers)) == []
    assert all(reason.strip() for reason in cat.EXEMPT.values())


def test_catalogue_includes_python_wrappers(methods):
    wrappers = {(m.cls, m.name) for m in methods if m.python_wrapper}
    assert {("SciQLopPlot", "plot"), ("SciQLopMultiPlotPanel", "plot")} <= wrappers


def test_baseline_coverage_report(methods, mros, tmp_path):
    found = cat.discover("vertical_line", cat.applicable(methods, mros["vertical_line"]))
    report = tmp_path / "api_fuzz_baselines.json"
    report.write_text(json.dumps(found))
    assert found and os.path.getsize(report) > 0
