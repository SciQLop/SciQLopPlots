"""Tests for the real fastembed-backed SemanticSearchMethod -- require the
`smart_search` extra installed. Download the default model on first run
(network access required then), cached locally afterward.

Two tests, two different jobs:

* ``test_semantic_method_ranks_related_text_above_unrelated`` is the
  primary proof that fastembed's embeddings do something semantically
  meaningful. It talks to ``SemanticSearchMethod`` directly -- no
  ``ProductsModel``, no filter models, no ``SmartSearchController`` -- so
  nothing else (in particular the lexical DP matcher in
  ``ProductsTreeFilterModel``) can be responsible for the result.

* ``test_magnetic_field_query_surfaces_fgm_and_scm_by_description`` proves
  the wiring end-to-end through ``ProductsTreeFilterModel``: it first
  empirically confirms the target leaves are genuinely invisible under
  DP-only scoring (no smart search enabled yet), then confirms enabling
  smart search with the real ``SemanticSearchMethod`` makes them appear.
  An earlier version of this test used a fixture whose description
  literally contained the query's words ("Magnetic Field"), so it passed
  via the pre-existing lexical matcher alone regardless of whether
  ``SemanticSearchMethod`` did anything at all. The query here includes a
  throwaway token ("quantify") that cannot subsequence-match either
  target leaf's path or metadata text -- the DP scorer zeroes a node's
  whole score if *any* single query token fails to match at all -- which
  deterministically forces the DP-only score to exactly 0, independent of
  wording quirks elsewhere in the query or description.
"""
import uuid

import pytest
from SciQLopPlots import (
    ProductsModel, ProductsModelNode, ProductsModelNodeType, ParameterType,
    ProductsTreeFilterModel, ProductsFlatFilterModel, QueryParser
)
from SciQLopPlots.smart_search_controller import SmartSearchController
from SciQLopPlots.smart_search_semantic import SemanticSearchMethod
from SciQLopPlots.smart_search_method import NodeSnapshot
from test_products_filter import collect_visible_names, flush_events

pytest.importorskip("fastembed")


def test_semantic_method_ranks_related_text_above_unrelated():
    """Pure method-level proof: index a few texts, query with wording that
    shares (almost) no vocabulary with the semantically related entry, and
    check its cosine-similarity score clearly beats unrelated entries.

    Scores observed while writing this test (deterministic, same model,
    same inputs): related=79.2, unrelated_camera=61.6, unrelated_clock=66.2
    -- margins of ~17.6 and ~13.1 points on a 0-100 scale. The assertion
    below uses an 8-point margin, well under both observed gaps, so it has
    real headroom without being a guess.
    """
    method = SemanticSearchMethod()

    query = ("instrument that senses the strength and orientation of the "
             "ambient magnetic field around the spacecraft")
    related = NodeSnapshot(
        "related",
        "Fluxgate sensor package delivering tri-axial vector B readings of "
        "the local magnetostatic environment surrounding the spacecraft")
    unrelated_camera = NodeSnapshot(
        "unrelated_camera",
        "High-resolution optical camera capturing visible-light imagery of "
        "the planetary surface for terrain mapping")
    unrelated_clock = NodeSnapshot(
        "unrelated_clock",
        "Onboard clock synchronizing telemetry timestamps to a ground "
        "reference oscillator")

    method.index([related, unrelated_camera, unrelated_clock])
    scores = method.query(query)

    margin = 8.0
    assert scores["related"] - scores["unrelated_camera"] > margin
    assert scores["related"] - scores["unrelated_clock"] > margin


@pytest.mark.timeout(120)
def test_magnetic_field_query_surfaces_fgm_and_scm_by_description(qtbot):
    model = ProductsModel.instance()
    token = uuid.uuid4().hex[:8]
    root = ProductsModelNode(f"MMS1_{token}")

    fgm = ProductsModelNode(
        "mms1_fgm_b_gse_srvy_l2", "test",
        {"description": "Fluxgate coil package delivering tri-axial vector B "
                         "measurements around a spacecraft, calibrated Level "
                         "2 combined fast and slow survey cadence for MMS "
                         "Satellite Number 1"},
        ProductsModelNodeType.PARAMETER, ParameterType.Vector)
    scm = ProductsModelNode(
        "mms1_scm_acb_gse_scsrvy_srvy_l2", "test",
        {"description": "Search coil package delivering AC tri-axial vector "
                         "B measurements around a spacecraft at high "
                         "cadence, calibrated Level 2 data for MMS "
                         "Satellite Number 1"},
        ProductsModelNodeType.PARAMETER, ParameterType.Vector)
    unrelated = ProductsModelNode(
        "mms1_edp_dce_gse_fast_l2", "test",
        {"description": "Level 2 Electric Field Double Probe Fast Survey Data"},
        ProductsModelNodeType.PARAMETER, ParameterType.Vector)
    root.add_child(fgm)
    root.add_child(scm)
    root.add_child(unrelated)
    model.add_node([], root)

    tree_fm = ProductsTreeFilterModel()
    tree_fm.setSourceModel(model)
    corpus_fm = ProductsFlatFilterModel(model)
    # Headroom for the shared ProductsModel singleton's corpus, which keeps
    # growing across the whole test session (other files add plenty of
    # leaves whose descriptions literally say "magnetic field" and would
    # otherwise crowd our two targets out of a tight top-2 cutoff).
    tree_fm.set_max_score_tiers(8)

    # "quantify" cannot subsequence-match either target's path or metadata
    # text (neither contains the letter 'q'), so free_text_score() -- which
    # returns 0 for the whole node the instant any single token fails to
    # match at all -- deterministically zeroes both fgm and scm here,
    # regardless of how the rest of the query happens to be worded.
    query = ("device used to quantify the ambient magnetic environment "
             "surrounding a spacecraft")

    controller = SmartSearchController()
    controller.register_method(SemanticSearchMethod())
    controller.attach(model, corpus_fm, [tree_fm])
    try:
        controller.set_enabled(True)

        tree_fm.set_query(QueryParser.parse(query))
        flush_events()
        visible_before = collect_visible_names(tree_fm)
        assert "mms1_fgm_b_gse_srvy_l2" not in visible_before
        assert "mms1_scm_acb_gse_scsrvy_srvy_l2" not in visible_before

        def fgm_and_scm_surfaced():
            controller.on_query_changed(query)
            qtbot.wait(50)
            names = collect_visible_names(tree_fm)
            return ("mms1_fgm_b_gse_srvy_l2" in names
                    and "mms1_scm_acb_gse_scsrvy_srvy_l2" in names)

        qtbot.waitUntil(fgm_and_scm_surfaced, timeout=60000)
    finally:
        model.rowsInserted.disconnect(controller._reindex)
        model.rowsRemoved.disconnect(controller._reindex)
        model.modelReset.disconnect(controller._reindex)
