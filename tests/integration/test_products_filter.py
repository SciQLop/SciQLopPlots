"""Tests for ProductsTreeFilterModel and ProductsFlatFilterModel."""
import pytest
from PySide6.QtCore import QCoreApplication, Qt
from SciQLopPlots import (
    ProductsModel, ProductsModelNode, ProductsModelNodeType, ParameterType,
    ProductsTreeFilterModel, ProductsFlatFilterModel, QueryParser
)


def flush_events():
    """Process pending events so batched flat model results appear."""
    for _ in range(10):
        QCoreApplication.processEvents()


def collect_visible_names(model, parent=None):
    from PySide6.QtCore import QModelIndex, Qt
    if parent is None:
        parent = QModelIndex()
    names = []
    for row in range(model.rowCount(parent)):
        idx = model.index(row, 0, parent)
        names.append(model.data(idx, Qt.DisplayRole))
        names.extend(collect_visible_names(model, idx))
    return names


def collect_visible_scores(model, role, parent=None):
    from PySide6.QtCore import QModelIndex
    if parent is None:
        parent = QModelIndex()
    result = {}
    for row in range(model.rowCount(parent)):
        idx = model.index(row, 0, parent)
        result[model.data(idx, Qt.DisplayRole)] = model.data(idx, role)
        result.update(collect_visible_scores(model, role, idx))
    return result


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


@pytest.fixture
def deep_tree_model(qtbot):
    """Tree: amda -> ACE -> MFI -> b_gse (leaf), mimicking real product paths."""
    model = ProductsModel.instance()

    amda = ProductsModelNode("amda")
    ace = ProductsModelNode("ACE")
    mfi = ProductsModelNode("MFI")
    b_gse = ProductsModelNode(
        "b_gse", "amda", {},
        ProductsModelNodeType.PARAMETER, ParameterType.Vector)

    mfi.add_child(b_gse)
    ace.add_child(mfi)
    amda.add_child(ace)
    model.add_node([], amda)

    yield model


class TestPathScoring:
    """Search tokens matching ancestor folder names must surface the leaf."""

    def test_flat_ancestor_query_matches_leaf(self, qtbot, deep_tree_model):
        fm = ProductsFlatFilterModel(deep_tree_model)
        q = QueryParser.parse("amdaacemfi")
        fm.set_query(q)
        flush_events()
        names = [fm.data(fm.index(i, 0)) for i in range(fm.rowCount())]
        assert "b_gse" in names

    def test_flat_mixed_path_and_leaf_query(self, qtbot, deep_tree_model):
        fm = ProductsFlatFilterModel(deep_tree_model)
        q = QueryParser.parse("mfi b_gse")
        fm.set_query(q)
        flush_events()
        names = [fm.data(fm.index(i, 0)) for i in range(fm.rowCount())]
        assert "b_gse" in names

    def test_tree_ancestor_query_matches_leaf(self, qtbot, deep_tree_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(deep_tree_model)
        q = QueryParser.parse("amdaacemfi")
        fm.set_query(q)
        assert fm.rowCount() > 0


@pytest.fixture
def ace_mfi_vs_unrelated_mission_model(qtbot):
    """Real-world-shaped fixture (mirrors an actual AMDA inventory export):
    one genuine ACE/MFI leaf under a clean `Parameters/ACE/MFI/...` path, and
    one unrelated SolarOrbiter leaf whose bulky metadata happens to contain
    "ace"/"mfi" only as scattered, non-contiguous characters."""
    model = ProductsModel.instance()
    root = ProductsModelNode("Parameters")

    solo = ProductsModelNode("SolarOrbiter")
    swapas = ProductsModelNode("SWAPAS")
    l2 = ProductsModelNode("L2")
    so_pas = ProductsModelNode("so_pas_l2_3d")
    decoy = ProductsModelNode(
        "pas_l2_3d_e0", "amda",
        {"dataset": "so-pas-l2.3d", "is_public": "True", "name": "energy : first",
         "start_date": "2020-04-14T15:03:06Z", "stop_date": "2024-08-30T09:38:43Z",
         "xmlid": "pas_l2_3d_e0"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
    so_pas.add_child(decoy)
    l2.add_child(so_pas)
    swapas.add_child(l2)
    solo.add_child(swapas)
    root.add_child(solo)

    ace = ProductsModelNode("ACE")
    mfi = ProductsModelNode("MFI")
    ace_imf_all = ProductsModelNode("ace_imf_all")
    genuine = ProductsModelNode(
        "imf_gsm", "amda",
        {"dataset": "ace-imf-all",
         "description": "Magnetic field vector in GSM Cartesian coordinates (16 sec)",
         "display_type": "timeseries", "is_public": "True", "name": "b_gsm",
         "size": "3", "start_date": "1997-09-02T00:00:12Z",
         "stop_date": "2024-09-28T23:59:53Z", "ucd": "phys.magField",
         "units": "nT", "xmlid": "imf_gsm"},
        ProductsModelNodeType.PARAMETER, ParameterType.Vector)
    ace_imf_all.add_child(genuine)
    mfi.add_child(ace_imf_all)
    ace.add_child(mfi)
    root.add_child(ace)

    model.add_node([], root)
    yield model


class TestScoringFindsBestAlignment:
    """A clean, whole-path match must outrank a scattered incidental one,
    even though the scattered match's text happens to be shorter."""

    def test_genuine_mission_leaf_outranks_unrelated_scattered_match(
            self, qtbot, ace_mfi_vs_unrelated_mission_model):
        fm = ProductsFlatFilterModel(ace_mfi_vs_unrelated_mission_model)
        fm.set_query(QueryParser.parse("ACE mfi"))
        flush_events()
        names = [fm.data(fm.index(i, 0)) for i in range(fm.rowCount())]
        assert "imf_gsm" in names
        assert "pas_l2_3d_e0" in names
        assert names.index("imf_gsm") < names.index("pas_l2_3d_e0")


@pytest.fixture
def three_tier_model(qtbot):
    """Three leaves at empirically distinct free-text score tiers for query
    "mag fld": mag_fld_leaf=14 (clean word-boundary match), field_data=10
    (partial/scattered match), unrelated_leaf=6 (barely-there incidental
    match buried in a long unrelated description)."""
    model = ProductsModel.instance()
    root = ProductsModelNode("Instrument")

    mag = ProductsModelNode("MAG")
    fld = ProductsModelNode("FLD")
    top_leaf = ProductsModelNode(
        "mag_fld_leaf", "test",
        {"dataset": "mag-fld", "description": "clean match"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
    fld.add_child(top_leaf)
    mag.add_child(fld)
    root.add_child(mag)

    other = ProductsModelNode("Other")
    magnetics = ProductsModelNode("Magnetics")
    mid_leaf = ProductsModelNode(
        "field_data", "test",
        {"dataset": "some-other-set", "description": "has magnetic and field words scattered"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
    magnetics.add_child(mid_leaf)
    other.add_child(magnetics)
    root.add_child(other)

    whatever = ProductsModelNode("Whatever")
    xyz = ProductsModelNode("xyz123")
    low_leaf = ProductsModelNode(
        "unrelated_leaf", "test",
        {"dataset": "totally-unrelated",
         "description": "a very long unrelated description that happens to contain the letters "
                         "m a g somewhere and f l d somewhere else too purely by accident padded "
                         "padded padded padded padded padded padded padded"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
    xyz.add_child(low_leaf)
    whatever.add_child(xyz)
    root.add_child(whatever)

    model.add_node([], root)
    yield model


class TestTreeScoreTiers:
    """The tree view has no ranking of its own — filterAcceptsRow is a bare
    score > 0 check — so it can't just show "the best" matches; it needs an
    explicit, tunable notion of how many score tiers count as a match."""

    def test_default_max_score_tiers_is_two(self, qtbot):
        fm = ProductsTreeFilterModel()
        assert fm.max_score_tiers() == 2

    def test_top_tier_only_keeps_best_leaf(self, qtbot, three_tier_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(three_tier_model)
        fm.set_max_score_tiers(1)
        fm.set_query(QueryParser.parse("mag fld"))
        names = collect_visible_names(fm)
        assert "mag_fld_leaf" in names
        assert "field_data" not in names
        assert "unrelated_leaf" not in names

    def test_default_two_tiers_keeps_top_and_mid(self, qtbot, three_tier_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(three_tier_model)
        fm.set_query(QueryParser.parse("mag fld"))
        names = collect_visible_names(fm)
        assert "mag_fld_leaf" in names
        assert "field_data" in names
        assert "unrelated_leaf" not in names

    def test_enough_tiers_keeps_every_positive_match(self, qtbot, three_tier_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(three_tier_model)
        fm.set_max_score_tiers(10)
        fm.set_query(QueryParser.parse("mag fld"))
        names = collect_visible_names(fm)
        assert "mag_fld_leaf" in names
        assert "field_data" in names
        assert "unrelated_leaf" in names


class TestFlatRelevanceScoreRole:
    """Relevance % is relative to the best match in the current result set,
    not an absolute score (see product_search_scoring_fixes memory — raw
    scores are a coarse, query/corpus-dependent scale)."""

    RELEVANCE_ROLE = Qt.UserRole + 10  # ProductsRelevanceScoreRole

    def test_relevance_relative_to_best_match(self, qtbot, three_tier_model):
        fm = ProductsFlatFilterModel(three_tier_model)
        fm.set_query(QueryParser.parse("mag fld"))
        flush_events()
        scores = {}
        for i in range(fm.rowCount()):
            idx = fm.index(i, 0)
            scores[fm.data(idx)] = fm.data(idx, self.RELEVANCE_ROLE)
        assert scores["mag_fld_leaf"] == 100
        assert scores["field_data"] == 71
        assert scores["unrelated_leaf"] == 43

    def test_no_relevance_role_without_free_text(self, qtbot, three_tier_model):
        fm = ProductsFlatFilterModel(three_tier_model)
        fm.set_query(QueryParser.parse("provider:test"))
        flush_events()
        assert fm.rowCount() == 3
        for i in range(fm.rowCount()):
            assert fm.data(fm.index(i, 0), self.RELEVANCE_ROLE) is None


@pytest.fixture
def folder_matches_but_has_no_matching_leaf_model(qtbot):
    """A folder whose own metadata text scores well for the query, but whose
    name (the only part inherited by descendants via path()) does not — so
    it has no PARAMETER descendant that matches at all. Mirrors real AMDA
    data where folder/component-index-type entries scored higher than
    genuine parameter leaves purely because their own short text was cheap
    to match, independent of their children."""
    model = ProductsModel.instance()
    root = ProductsModelNode("Provider")

    matchy_folder = ProductsModelNode(
        "Component03", {"description": "magnetic field data component"}, "folder_open")
    unrelated_leaf = ProductsModelNode(
        "xyz_status", "test", {"description": "power supply telemetry channel"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
    matchy_folder.add_child(unrelated_leaf)
    root.add_child(matchy_folder)

    other_folder = ProductsModelNode("OtherFolder")
    real_leaf = ProductsModelNode(
        "real_leaf", "test", {"description": "magnetic field data, clean match"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
    other_folder.add_child(real_leaf)
    root.add_child(other_folder)

    model.add_node([], root)
    yield model


class TestFoldersNeverMatchOnTheirOwnText:
    """Only PARAMETER leaves can independently satisfy a query — folders
    must surface exclusively via setRecursiveFilteringEnabled (i.e. because
    a descendant leaf matched), never because their own short path text
    happens to score well. Mirrors ProductsFlatFilterModel, which never
    considers non-PARAMETER nodes as candidates at all."""

    def test_folder_with_no_matching_leaf_is_excluded(
            self, qtbot, folder_matches_but_has_no_matching_leaf_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(folder_matches_but_has_no_matching_leaf_model)
        fm.set_query(QueryParser.parse("mag fld"))
        names = collect_visible_names(fm)
        assert "Component03" not in names
        assert "xyz_status" not in names

    def test_genuine_leaf_elsewhere_still_surfaces(
            self, qtbot, folder_matches_but_has_no_matching_leaf_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(folder_matches_but_has_no_matching_leaf_model)
        fm.set_query(QueryParser.parse("mag fld"))
        names = collect_visible_names(fm)
        assert "real_leaf" in names
        assert "OtherFolder" in names


class TestRawTextSeparators:
    """raw_text() must separate name/metadata fields, or unrelated adjacent
    fields fuse into bogus subsequence matches (e.g. "...timeseries" +
    "is_public..." reads as "timeseriesis_public", spuriously matching short
    tokens that span the seam)."""

    def test_metadata_fields_are_separated(self, qtbot):
        node = ProductsModelNode(
            "cass_l", "amda",
            {"dataset": "cass-ephem-polar", "display_type": "timeseries",
             "is_public": "True"},
            ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
        raw = node.raw_text()
        assert "polardisplay_type" not in raw
        assert "timeseriesis_public" not in raw

    def test_name_and_first_metadata_field_are_separated(self, qtbot):
        node = ProductsModelNode(
            "cass_l", "amda", {"dataset": "cass-ephem-polar"},
            ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
        raw = node.raw_text()
        assert "cass_ldataset" not in raw


class TestTreeRelevanceScoreRole:
    RELEVANCE_ROLE = Qt.UserRole + 10  # ProductsRelevanceScoreRole

    def test_relevance_relative_to_best_match(self, qtbot, three_tier_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(three_tier_model)
        fm.set_max_score_tiers(10)  # show all three leaves for this check
        fm.set_query(QueryParser.parse("mag fld"))
        scores = collect_visible_scores(fm, self.RELEVANCE_ROLE)
        assert scores["mag_fld_leaf"] == 100
        assert scores["field_data"] == 71
        assert scores["unrelated_leaf"] == 43

    def test_folders_have_no_relevance_role(self, qtbot, three_tier_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(three_tier_model)
        fm.set_max_score_tiers(10)
        fm.set_query(QueryParser.parse("mag fld"))
        scores = collect_visible_scores(fm, self.RELEVANCE_ROLE)
        assert scores["Instrument"] is None
