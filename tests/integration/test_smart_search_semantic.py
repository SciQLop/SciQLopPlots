"""End-to-end test for the real fastembed-backed SemanticSearchMethod --
requires the `smart_search` extra installed. Downloads the default model on
first run (network access required then), cached locally afterward."""
import uuid

import pytest
from SciQLopPlots import (
    ProductsModel, ProductsModelNode, ProductsModelNodeType, ParameterType,
    ProductsTreeFilterModel, ProductsFlatFilterModel, QueryParser
)
from SciQLopPlots.smart_search_controller import SmartSearchController
from SciQLopPlots.smart_search_semantic import SemanticSearchMethod
from test_products_filter import collect_visible_names

pytest.importorskip("fastembed")


@pytest.mark.timeout(120)
def test_magnetic_field_query_surfaces_fgm_and_scm_by_description(qtbot):
    model = ProductsModel.instance()
    token = uuid.uuid4().hex[:8]
    root = ProductsModelNode(f"MMS1_{token}")

    fgm = ProductsModelNode(
        "mms1_fgm_b_gse_srvy_l2", "test",
        {"description": "Level2 Flux Gate Magnetometer Combined Fast/Slow "
                         "Survey DC Magnetic Field for MMS Satellite Number 1"},
        ProductsModelNodeType.PARAMETER, ParameterType.Vector)
    scm = ProductsModelNode(
        "mms1_scm_acb_gse_scsrvy_srvy_l2", "test",
        {"description": "Level 2 Search Coil Magnetometer AC Magnetic Field "
                         "Survey (32S/s) Data"},
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

    controller = SmartSearchController()
    controller.register_method(SemanticSearchMethod())
    controller.set_enabled(True)
    controller.attach(model, corpus_fm, [tree_fm])

    tree_fm.set_query(QueryParser.parse("magnetic field"))
    controller.on_query_changed("magnetic field")

    def fgm_and_scm_visible():
        names = collect_visible_names(tree_fm)
        return "mms1_fgm_b_gse_srvy_l2" in names and "mms1_scm_acb_gse_scsrvy_srvy_l2" in names

    qtbot.waitUntil(fgm_and_scm_visible, timeout=60000)
