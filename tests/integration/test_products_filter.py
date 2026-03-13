"""Tests for ProductsTreeFilterModel and ProductsFlatFilterModel."""
import pytest
from PySide6.QtCore import QCoreApplication
from SciQLopPlots import (
    ProductsModel, ProductsModelNode, ProductsModelNodeType, ParameterType,
    ProductsTreeFilterModel, ProductsFlatFilterModel, QueryParser
)


def flush_events():
    """Process pending events so batched flat model results appear."""
    for _ in range(10):
        QCoreApplication.processEvents()


@pytest.fixture
def products_model(qtbot):
    """Populate ProductsModel with test data."""
    model = ProductsModel.instance()

    root = ProductsModelNode("test_filter_provider")
    folder = ProductsModelNode("Magnetic")

    node_a = ProductsModelNode(
        "MagneticField", "test_filter_provider",
        {"uid": "mf001", "components": "x,y,z",
         "start_date": "2020-01-01T00:00:00Z", "stop_date": "2024-12-31T23:59:59Z"},
        ProductsModelNodeType.PARAMETER, ParameterType.Vector)

    node_b = ProductsModelNode(
        "Density", "test_filter_provider",
        {"uid": "d001",
         "start_date": "2018-06-01T00:00:00Z", "stop_date": "2023-06-30T23:59:59Z"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)

    node_c = ProductsModelNode(
        "EnergySpectrum", "other_filter_provider",
        {"uid": "es001",
         "start_date": "2015-01-01T00:00:00Z", "stop_date": "2019-12-31T23:59:59Z"},
        ProductsModelNodeType.PARAMETER, ParameterType.Multicomponents)

    folder.add_child(node_a)
    root.add_child(folder)
    root.add_child(node_b)
    root.add_child(node_c)
    model.add_node([], root)

    yield model


class TestTreeFilterModel:
    def test_empty_query_shows_all(self, qtbot, products_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(products_model)
        q = QueryParser.parse("")
        fm.set_query(q)
        assert fm.rowCount() > 0

    def test_free_text_filters(self, qtbot, products_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(products_model)
        q = QueryParser.parse("magnetic")
        fm.set_query(q)
        assert fm.rowCount() > 0

    def test_field_filter_provider(self, qtbot, products_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(products_model)
        q = QueryParser.parse("provider:other_filter_provider")
        fm.set_query(q)
        assert fm.rowCount() > 0

    def test_combined_query(self, qtbot, products_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(products_model)
        q = QueryParser.parse("field provider:test_filter_provider")
        fm.set_query(q)
        assert fm.rowCount() > 0

    def test_no_match_returns_empty(self, qtbot, products_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(products_model)
        q = QueryParser.parse("zzzznonexistent")
        fm.set_query(q)
        assert fm.rowCount() == 0


class TestFlatFilterModel:
    def test_empty_query_shows_all_parameters(self, qtbot, products_model):
        fm = ProductsFlatFilterModel(products_model)
        q = QueryParser.parse("")
        fm.set_query(q)
        flush_events()
        assert fm.rowCount() >= 3

    def test_results_are_flat(self, qtbot, products_model):
        fm = ProductsFlatFilterModel(products_model)
        q = QueryParser.parse("")
        fm.set_query(q)
        flush_events()
        for i in range(fm.rowCount()):
            idx = fm.index(i, 0)
            assert not idx.parent().isValid()

    def test_sorted_by_score(self, qtbot, products_model):
        fm = ProductsFlatFilterModel(products_model)
        q = QueryParser.parse("magnetic")
        fm.set_query(q)
        flush_events()
        if fm.rowCount() > 0:
            first_name = fm.data(fm.index(0, 0))
            assert "magnetic" in first_name.lower()

    def test_field_filter_applied(self, qtbot, products_model):
        fm = ProductsFlatFilterModel(products_model)
        q = QueryParser.parse("type:scalar")
        fm.set_query(q)
        flush_events()
        assert fm.rowCount() >= 1

    def test_drag_mime_data(self, qtbot, products_model):
        fm = ProductsFlatFilterModel(products_model)
        q = QueryParser.parse("")
        fm.set_query(q)
        flush_events()
        if fm.rowCount() > 0:
            idx = fm.index(0, 0)
            mime = fm.mimeData([idx])
            assert mime is not None
            assert ProductsModel.mime_type() in mime.formats()


class TestDateRangeFilters:
    def test_after_filter(self, qtbot, products_model):
        fm = ProductsFlatFilterModel(products_model)
        q = QueryParser.parse("after:2020-06-01")
        fm.set_query(q)
        flush_events()
        names = [fm.data(fm.index(i, 0)) for i in range(fm.rowCount())]
        assert "EnergySpectrum" not in names
        assert "MagneticField" in names

    def test_before_filter(self, qtbot, products_model):
        fm = ProductsFlatFilterModel(products_model)
        q = QueryParser.parse("before:2019-01-01")
        fm.set_query(q)
        flush_events()
        names = [fm.data(fm.index(i, 0)) for i in range(fm.rowCount())]
        assert "MagneticField" not in names
        assert "EnergySpectrum" in names

    def test_date_range(self, qtbot, products_model):
        fm = ProductsFlatFilterModel(products_model)
        q = QueryParser.parse("after:2020-01-01 before:2023-12-31 provider:test_filter_provider")
        fm.set_query(q)
        flush_events()
        names = [fm.data(fm.index(i, 0)) for i in range(fm.rowCount())]
        assert "MagneticField" in names
        assert "Density" in names
        assert "EnergySpectrum" not in names

    def test_tree_filter_after(self, qtbot, products_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(products_model)
        q = QueryParser.parse("after:2020-06-01")
        fm.set_query(q)
        assert fm.rowCount() > 0

    def test_partial_year(self, qtbot, products_model):
        fm = ProductsFlatFilterModel(products_model)
        q = QueryParser.parse("after:2020 provider:test_filter_provider")
        fm.set_query(q)
        flush_events()
        names = [fm.data(fm.index(i, 0)) for i in range(fm.rowCount())]
        assert "EnergySpectrum" not in names
        assert "MagneticField" in names

    def test_partial_year_month(self, qtbot, products_model):
        fm = ProductsFlatFilterModel(products_model)
        q = QueryParser.parse("before:2016-06 provider:other_filter_provider")
        fm.set_query(q)
        flush_events()
        names = [fm.data(fm.index(i, 0)) for i in range(fm.rowCount())]
        assert "EnergySpectrum" in names
        assert "MagneticField" not in names
