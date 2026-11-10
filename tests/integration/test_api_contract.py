"""The API contract: every public call works, or raises the right exception with a message that says why.

test_contract runs the full bad-argument sweep (tests/sweeps/api_fuzz) for each class in CI_CLASSES: the
classes the campaign has made clean. SCIQLOP_PLOTS_DEEP_FUZZ=1 sweeps every class.
"""
import os

import pytest

from tests.sweeps.api_fuzz.driver import run_folder, sweep

# Core classes the campaign made clean, swept in one call (building the receivers costs ~45 s).
CI_CLASSES = ["SciQLopPlotRange", "SciQLopLineGraph", "SciQLopColorMap", "SciQLopHistogram",
              "SciQLopVerticalSpan", "SciQLopPlotGroup", "MatchedXY"]


def _failures(rows):
    return [f"{r['id']}: {r['status']}: {r['detail'][:300]}" for r in rows if r["status"] != "pass"]


# The sweep has its own per-case watchdog, so the CI-wide 60 s per-test limit only cuts it short on slow runners.
@pytest.mark.timeout(600)
def test_contract():
    assert _failures(sweep(classes=CI_CLASSES)) == []


@pytest.mark.skipif(os.environ.get("SCIQLOP_PLOTS_DEEP_FUZZ") != "1", reason="deep sweep: SCIQLOP_PLOTS_DEEP_FUZZ=1")
def test_deep_sweep():
    assert _failures(sweep()) == []


def test_folder_mode(tmp_path):
    (tmp_path / "a_ok.py").write_text("print('ok')\n")
    (tmp_path / "b_abort.py").write_text("import os\nos.abort()\n")
    (tmp_path / "c_noise.py").write_text("import sys\nsys.stderr.write('x\\n')\nprint('ok')\n")
    assert [r["status"] for r in run_folder(str(tmp_path))] == ["pass", "crash", "swallowed"]
