"""Tests for ProductsTreeFilterModel and ProductsFlatFilterModel."""
import threading
import uuid

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
        flush_events()
        assert fm.rowCount() > 0

    def test_free_text_filters(self, qtbot, products_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(products_model)
        q = QueryParser.parse("magnetic")
        fm.set_query(q)
        flush_events()
        assert fm.rowCount() > 0

    def test_field_filter_provider(self, qtbot, products_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(products_model)
        q = QueryParser.parse("provider:other_filter_provider")
        fm.set_query(q)
        flush_events()
        assert fm.rowCount() > 0

    def test_combined_query(self, qtbot, products_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(products_model)
        q = QueryParser.parse("field provider:test_filter_provider")
        fm.set_query(q)
        flush_events()
        assert fm.rowCount() > 0

    def test_no_match_returns_empty(self, qtbot, products_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(products_model)
        q = QueryParser.parse("zzzznonexistent")
        fm.set_query(q)
        flush_events()
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
        flush_events()
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
        flush_events()
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
        flush_events()
        names = collect_visible_names(fm)
        assert "mag_fld_leaf" in names
        assert "field_data" not in names
        assert "unrelated_leaf" not in names

    def test_default_two_tiers_keeps_top_and_mid(self, qtbot, three_tier_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(three_tier_model)
        fm.set_query(QueryParser.parse("mag fld"))
        flush_events()
        names = collect_visible_names(fm)
        assert "mag_fld_leaf" in names
        assert "field_data" in names
        assert "unrelated_leaf" not in names

    def test_enough_tiers_keeps_every_positive_match(self, qtbot, three_tier_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(three_tier_model)
        fm.set_max_score_tiers(10)
        fm.set_query(QueryParser.parse("mag fld"))
        flush_events()
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
        assert scores["field_data"] == 85
        assert scores["unrelated_leaf"] == 30

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
    to match, independent of their children.

    ProductsModel is a process-wide singleton shared by the whole test
    session (see coverage_test_model's docstring for the same issue): the
    default max_score_tiers=2 tier cutoff ranks over the ENTIRE corpus, so
    leftover leaves from other fixtures (e.g. three_tier_model, reused
    across many tests) can out-rank real_leaf and tier-cut it out. A unique
    per-test token, required in the query (AND-semantics zeroes any leaf
    lacking it), guarantees only this fixture's own leaves can ever
    contribute a nonzero score for this query."""
    token = f"foldtok{uuid.uuid4().hex[:8]}"
    model = ProductsModel.instance()
    root = ProductsModelNode(f"Provider_{token}")

    matchy_folder = ProductsModelNode(
        "Component03", {"description": f"{token} magnetic field data component"},
        "folder_open")
    unrelated_leaf = ProductsModelNode(
        "xyz_status", "test", {"description": f"{token} power supply telemetry channel"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
    matchy_folder.add_child(unrelated_leaf)
    root.add_child(matchy_folder)

    other_folder = ProductsModelNode("OtherFolder")
    real_leaf = ProductsModelNode(
        "real_leaf", "test", {"description": f"{token} magnetic field data, clean match"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
    other_folder.add_child(real_leaf)
    root.add_child(other_folder)

    model.add_node([], root)
    yield model, token


class TestFoldersNeverMatchOnTheirOwnText:
    """Only PARAMETER leaves can independently satisfy a query — folders
    must surface exclusively via setRecursiveFilteringEnabled (i.e. because
    a descendant leaf matched), never because their own short path text
    happens to score well. Mirrors ProductsFlatFilterModel, which never
    considers non-PARAMETER nodes as candidates at all."""

    def test_folder_with_no_matching_leaf_is_excluded(
            self, qtbot, folder_matches_but_has_no_matching_leaf_model):
        model, token = folder_matches_but_has_no_matching_leaf_model
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_query(QueryParser.parse(f"{token} mag fld"))
        flush_events()
        names = collect_visible_names(fm)
        assert "Component03" not in names
        assert "xyz_status" not in names

    def test_genuine_leaf_elsewhere_still_surfaces(
            self, qtbot, folder_matches_but_has_no_matching_leaf_model):
        model, token = folder_matches_but_has_no_matching_leaf_model
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_query(QueryParser.parse(f"{token} mag fld"))
        flush_events()
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
        flush_events()
        scores = collect_visible_scores(fm, self.RELEVANCE_ROLE)
        assert scores["mag_fld_leaf"] == 100
        assert scores["field_data"] == 85
        assert scores["unrelated_leaf"] == 30

    def test_folders_have_no_relevance_role(self, qtbot, three_tier_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(three_tier_model)
        fm.set_max_score_tiers(10)
        fm.set_query(QueryParser.parse("mag fld"))
        flush_events()
        scores = collect_visible_scores(fm, self.RELEVANCE_ROLE)
        assert scores["Instrument"] is None


@pytest.fixture
def verbose_metadata_vs_terse_model(qtbot):
    """Mirrors a real report: query "MMS1 fgm b_gse" only matched AMDA, never
    the equivalent CDAWeb (speasy "cda" provider) product, even though the
    CDA leaf's own PATH contains all three tokens cleanly. Root cause: path
    and raw-text metadata used to be concatenated into one candidate string
    before scoring, so CDA's much more verbose ISTP-style metadata (units,
    catdesc, fieldnam, coordinate_system, depend_0, ...) inflated the length
    penalty for the whole blob, burying a clean path match under a poor
    total score. cda_leaf here has the same shape: clean path, heavy
    metadata. amda_leaf has a clean path and terse metadata, for contrast."""
    model = ProductsModel.instance()
    root = ProductsModelNode("Root")

    cda = ProductsModelNode("cda")
    mms = ProductsModelNode("MMS")
    mms1 = ProductsModelNode("MMS1")
    fgm = ProductsModelNode("FGM")
    srvy = ProductsModelNode("MMS1_FGM_SRVY_L2")
    cda_leaf = ProductsModelNode(
        "mms1_fgm_b_gse_srvy_l2", "cda",
        {"description": "Fluxgate Magnetometer Survey mode Level 2 magnetic field in "
                         "GSE coordinates",
         "units": "nT", "catdesc": "MMS1 FGM B GSE SRVY L2 vector", "fieldnam": "B_GSE",
         "var_type": "data", "display_type": "time_series", "depend_0": "Epoch",
         "coordinate_system": "GSE", "instrument_type": "Magnetic Fields (space)",
         "data_type": "survey"},
        ProductsModelNodeType.PARAMETER, ParameterType.Vector)
    srvy.add_child(cda_leaf)
    fgm.add_child(srvy)
    mms1.add_child(fgm)
    mms.add_child(mms1)
    cda.add_child(mms)
    root.add_child(cda)

    amda = ProductsModelNode("amda")
    a_mms1 = ProductsModelNode("MMS1")
    a_fgm = ProductsModelNode("FGM")
    amda_leaf = ProductsModelNode(
        "b_gse", "amda", {"description": "Magnetic field GSE"},
        ProductsModelNodeType.PARAMETER, ParameterType.Vector)
    a_fgm.add_child(amda_leaf)
    a_mms1.add_child(a_fgm)
    amda.add_child(a_mms1)
    root.add_child(amda)

    # Third, intervening-score leaf: a scattered/partial match with moderate
    # metadata. Calibrated (see SubsequenceMatcher.score) so that under the
    # OLD concatenated-scoring algorithm its score (22) lands strictly
    # between amda_leaf (32) and cda_leaf (10) -- with default
    # max_score_tiers=2 the top two distinct scores are {32, 22}, which
    # tier-cuts cda_leaf out of the tree entirely even though it's a clean
    # path match. Under the fixed per-token-max algorithm cda_leaf (37)
    # outranks this decoy (23), flipping who gets tier-cut.
    decoy = ProductsModelNode(
        "decoy_leaf", "other",
        {"description": "some field data referencing MMS1 fgm b_gse loosely "
                         "in unrelated free text padded padded padded padded"},
        ProductsModelNodeType.PARAMETER, ParameterType.Vector)
    other = ProductsModelNode("Other")
    sensors = ProductsModelNode("Sensors")
    mms_payload = ProductsModelNode("MMSPayload")
    mms_payload.add_child(decoy)
    sensors.add_child(mms_payload)
    other.add_child(sensors)
    root.add_child(other)

    model.add_node([], root)
    yield model


class TestPathAndMetadataScoredSeparately:
    """Path and raw-text metadata must be scored independently and combined
    via per-token max, never concatenated into one candidate string --
    otherwise a clean path match gets its score diluted by unrelated (but
    voluminous) metadata."""

    def test_verbose_metadata_leaf_ranks_in_flat_results(
            self, qtbot, verbose_metadata_vs_terse_model):
        fm = ProductsFlatFilterModel(verbose_metadata_vs_terse_model)
        fm.set_query(QueryParser.parse("MMS1 fgm b_gse"))
        flush_events()
        names = [fm.data(fm.index(i, 0)) for i in range(fm.rowCount())]
        assert "mms1_fgm_b_gse_srvy_l2" in names
        assert "b_gse" in names

    def test_verbose_metadata_leaf_survives_default_tree_tier_cutoff(
            self, qtbot, verbose_metadata_vs_terse_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(verbose_metadata_vs_terse_model)
        fm.set_query(QueryParser.parse("MMS1 fgm b_gse"))
        flush_events()
        names = collect_visible_names(fm)
        assert "mms1_fgm_b_gse_srvy_l2" in names


class TestTreeScoringIsAsync:
    """Regression test for the freeze bug: ProductsTreeFilterModel::set_query()
    used to score the entire corpus synchronously -- up to three redundant
    full-corpus DP passes on the UI thread per call (see git history). It
    must now hand off to a chunked background QTimer instead: a query's
    scores only become visible once the batch settles, not immediately on
    return from set_query()."""

    RELEVANCE_ROLE = Qt.UserRole + 10  # ProductsRelevanceScoreRole

    def test_relevance_role_settles_only_after_flush(self, qtbot, three_tier_model):
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(three_tier_model)
        fm.set_max_score_tiers(10)
        flush_events()  # let the initial (query-less) scoring settle first

        fm.set_query(QueryParser.parse("mag fld"))
        # No flush_events() yet: the new query's scores must not be visible
        # synchronously -- committing happens only once the background
        # batch has fully scored the corpus.
        scores_before_flush = collect_visible_scores(fm, self.RELEVANCE_ROLE)
        assert scores_before_flush.get("mag_fld_leaf") is None

        flush_events()
        scores_after_flush = collect_visible_scores(fm, self.RELEVANCE_ROLE)
        assert scores_after_flush["mag_fld_leaf"] == 100


@pytest.fixture
def coverage_test_model(qtbot):
    """Dedicated fixture for TestTreeCoverageRole, kept separate from
    three_tier_model (which Task 1/2's already-approved tests depend on).

    ProductsModel is a process-wide singleton shared by the whole test
    session, so any fixture's top-level node lingers in the corpus for the
    rest of the run (see test_products_model_consistency.py, which solves
    the identical problem the same way). recompute_score_cutoff() ranks
    tiers over the ENTIRE corpus, not just this fixture's leaves -- so an
    unrelated leftover leaf from another test that happens to score between
    our tiers would silently shift our cutoff. Embedding a unique
    per-test token in every leaf's description, and requiring that same
    token in the query (AND-semantics: a token score of 0 zeroes the whole
    leaf, see ProductsTreeFilterModel::free_text_score), guarantees only
    this fixture's own three leaves can ever contribute a nonzero score for
    this query, regardless of whatever else resides in the shared
    singleton at test time.

    Same score-tier shape as three_tier_model (verified empirically with
    SubsequenceMatcher.score): mag_fld_leaf=41 (clean word-boundary match),
    field_data=33 (partial/scattered match), unrelated_leaf=18
    (barely-there incidental match) -- three distinct tiers, same
    2-top-vs-1 split under the default max_score_tiers=2.
    """
    token = f"covtok{uuid.uuid4().hex[:8]}"
    model = ProductsModel.instance()
    root_name = f"Instrument_{token}"
    root = ProductsModelNode(root_name)

    mag = ProductsModelNode("MAG")
    fld = ProductsModelNode("FLD")
    top_leaf = ProductsModelNode(
        "mag_fld_leaf", "test",
        {"dataset": "mag-fld", "description": f"{token} clean match"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
    fld.add_child(top_leaf)
    mag.add_child(fld)
    root.add_child(mag)

    other = ProductsModelNode("Other")
    magnetics = ProductsModelNode("Magnetics")
    mid_leaf = ProductsModelNode(
        "field_data", "test",
        {"dataset": "some-other-set",
         "description": f"{token} has magnetic and field words scattered"},
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
                         f"padded padded padded padded padded padded padded {token}"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
    xyz.add_child(low_leaf)
    whatever.add_child(xyz)
    root.add_child(whatever)

    model.add_node([], root)
    yield model, token, root_name


class TestTreeCoverageRole:
    COVERAGE_ROLE = Qt.UserRole + 11  # ProductsCoverageRole

    def test_folder_coverage_fraction(self, qtbot, coverage_test_model):
        model, token, root_name = coverage_test_model
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        # default max_score_tiers=2: only mag_fld_leaf(41) and field_data(33)
        # pass the cutoff, unrelated_leaf(18) doesn't -> 2 of 3 total match.
        fm.set_query(QueryParser.parse(f"{token} mag fld"))
        flush_events()
        scores = collect_visible_scores(fm, self.COVERAGE_ROLE)
        assert scores[root_name] == "2/3 (67%)"

    def test_leaves_have_no_coverage_role(self, qtbot, coverage_test_model):
        model, token, root_name = coverage_test_model
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_max_score_tiers(10)
        fm.set_query(QueryParser.parse(f"{token} mag fld"))
        flush_events()
        scores = collect_visible_scores(fm, self.COVERAGE_ROLE)
        assert scores["mag_fld_leaf"] is None

    def test_no_coverage_role_without_any_query(self, qtbot, coverage_test_model):
        """Plain browsing (set_query() never called) must show no coverage
        badge at all -- the design's explicit "no visual change without an
        active query" goal."""
        model, token, root_name = coverage_test_model
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        scores = collect_visible_scores(fm, self.COVERAGE_ROLE)
        assert scores[root_name] is None

    def test_no_coverage_role_with_filters_only_query(self, qtbot, coverage_test_model):
        """A filters-only query (no free-text tokens) must not show a
        coverage badge either -- same rule as
        test_no_relevance_role_without_free_text for the sibling role."""
        model, token, root_name = coverage_test_model
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_query(QueryParser.parse("provider:test"))
        flush_events()
        scores = collect_visible_scores(fm, self.COVERAGE_ROLE)
        assert scores[root_name] is None

    def test_total_count_updates_when_new_leaf_added(self, qtbot, coverage_test_model):
        """The `total` half of the coverage fraction must react to structural
        changes on the source model, not just to set_query() -- this is what
        the on_source_structure_changed rewiring in Step 4 is actually for."""
        model, token, root_name = coverage_test_model
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_max_score_tiers(10)  # keep every positive match visible
        fm.set_query(QueryParser.parse(f"{token} mag fld"))
        flush_events()
        scores_before = collect_visible_scores(fm, self.COVERAGE_ROLE)
        assert scores_before[root_name] == "3/3 (100%)"

        new_branch = ProductsModelNode("NewBranch")
        new_leaf = ProductsModelNode(
            "power_monitor", "test", {"description": "power consumption data"},
            ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
        new_branch.add_child(new_leaf)
        model.add_node([root_name], new_branch)
        flush_events()

        scores_after = collect_visible_scores(fm, self.COVERAGE_ROLE)
        assert scores_after[root_name] == "3/4 (75%)"


@pytest.fixture
def overlay_test_model(qtbot):
    """One leaf whose own text has zero relation to the probe query, used to
    prove an external score can surface it when nothing in its own text
    would ever match lexically."""
    model = ProductsModel.instance()
    token = uuid.uuid4().hex[:8]
    root = ProductsModelNode(f"ExternalScoreRoot_{token}")
    leaf = ProductsModelNode(
        "acronym_only", "test",
        {"dataset": "xyz", "description": "totally unrelated text"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
    root.add_child(leaf)
    model.add_node([], root)
    yield model, leaf


class TestExternalScoreOverlay:
    def test_tree_filter_ignores_external_scores_when_disabled(self, qtbot, overlay_test_model):
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_external_scores({path_key: 100.0})
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" not in collect_visible_names(fm)

    def test_tree_filter_blends_external_scores_when_enabled(self, qtbot, overlay_test_model):
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_smart_search_enabled(True)
        fm.set_external_scores({path_key: 100.0})
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" in collect_visible_names(fm)

    def test_flat_filter_blends_external_scores_when_enabled(self, qtbot, overlay_test_model):
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsFlatFilterModel(model)
        fm.set_smart_search_enabled(True)
        fm.set_external_scores({path_key: 100.0})
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" in collect_visible_names(fm)

    def test_smart_search_enabled_defaults_to_false(self, qtbot):
        fm = ProductsTreeFilterModel()
        assert fm.smart_search_enabled() is False

    def test_tree_filter_late_external_scores_trigger_rescoring(self, qtbot, overlay_test_model):
        """Regression test: a slow search method (e.g. Task 4's embedding
        model) calls set_external_scores() long after set_query() already
        committed a DP-only scoring pass. The view must pick up the new
        scores immediately, without the caller re-issuing set_query()."""
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_smart_search_enabled(True)
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" not in collect_visible_names(fm)

        fm.set_external_scores({path_key: 100.0})
        flush_events()
        assert "acronym_only" in collect_visible_names(fm)

    def test_flat_filter_late_external_scores_trigger_rescoring(self, qtbot, overlay_test_model):
        """Same regression as above, for ProductsFlatFilterModel."""
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsFlatFilterModel(model)
        fm.set_smart_search_enabled(True)
        fm.set_query(QueryParser.parse("magnetic field"))
        flush_events()
        assert "acronym_only" not in collect_visible_names(fm)

        fm.set_external_scores({path_key: 100.0})
        flush_events()
        assert "acronym_only" in collect_visible_names(fm)


class TestExternalScoreOverlayConcurrency:
    @pytest.mark.timeout(10)
    def test_concurrent_set_external_scores_does_not_deadlock(self, qtbot, overlay_test_model):
        model, leaf = overlay_test_model
        path_key = ' '.join(leaf.path())
        fm = ProductsTreeFilterModel()
        fm.setSourceModel(model)
        fm.set_smart_search_enabled(True)
        fm.set_query(QueryParser.parse("magnetic field"))

        stop = threading.Event()

        def hammer():
            i = 0
            while not stop.is_set():
                fm.set_external_scores({path_key: float(i % 100)})
                i += 1

        writer = threading.Thread(target=hammer, daemon=True)
        writer.start()
        try:
            for _ in range(200):
                QCoreApplication.processEvents()
        finally:
            stop.set()
            writer.join(timeout=5)
        assert not writer.is_alive()


class TestSmartSearchControllerEndToEnd:
    def test_controller_surfaces_leaf_via_fake_method(self, qtbot):
        from SciQLopPlots.smart_search_controller import SmartSearchController

        model = ProductsModel.instance()
        token = uuid.uuid4().hex[:8]
        root = ProductsModelNode(f"E2ERoot_{token}")
        leaf = ProductsModelNode(
            "acronym_only", "test", {"description": "totally unrelated text"},
            ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
        root.add_child(leaf)
        model.add_node([], root)
        path_key = ' '.join(leaf.path())

        tree_fm = ProductsTreeFilterModel()
        tree_fm.setSourceModel(model)
        corpus_fm = ProductsFlatFilterModel(model)

        class FakeSemanticMethod:
            def index(self, nodes):
                self.nodes = list(nodes)

            def query(self, text):
                if text != "magnetic field":
                    return {}
                return {n.path_key: 100.0 for n in self.nodes if n.path_key == path_key}

        controller = SmartSearchController()
        controller.register_method(FakeSemanticMethod())
        controller.set_enabled(True)
        controller.attach(model, corpus_fm, [tree_fm])

        tree_fm.set_query(QueryParser.parse("magnetic field"))
        controller.on_query_changed("magnetic field")

        qtbot.waitUntil(lambda: "acronym_only" in collect_visible_names(tree_fm), timeout=5000)
