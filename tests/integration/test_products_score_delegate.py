"""ProductsScoreDelegate: colored relevance badges must actually render."""
from PySide6.QtCore import Qt
from PySide6.QtGui import QImage
from PySide6.QtWidgets import QListView, QTextEdit

from SciQLopPlots import (
    ParameterType, ProductsFlatFilterModel, ProductsModel, ProductsModelNode,
    ProductsModelNodeType, ProductsView,
)


RELEVANCE_ROLE = Qt.UserRole + 10  # ProductsRelevanceScoreRole


def _distinct_colors(pixmap):
    image = pixmap.toImage().convertToFormat(QImage.Format_ARGB32)
    w, h = image.width(), image.height()
    step = max(1, min(w, h) // 100)
    colors = set()
    for y in range(0, h, step):
        for x in range(0, w, step):
            c = image.pixelColor(x, y)
            colors.add((c.red(), c.green(), c.blue(), c.alpha()))
    return len(colors)


class TestScoreDelegateRendersBadges:
    def test_relevance_badges_add_color_variety(self, qtbot):
        provider = "delegate_test_provider"
        top = ProductsModelNode(
            "mag_fld_leaf", provider, {"description": "clean match"},
            ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
        mid = ProductsModelNode(
            "field_data", provider,
            {"description": "has magnetic and field words scattered"},
            ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
        low = ProductsModelNode(
            "unrelated_leaf", provider,
            {"description": "a very long unrelated description that happens to "
                             "contain the letters m a g somewhere and f l d "
                             "somewhere else too purely by accident padded padded "
                             "padded padded padded padded padded padded"},
            ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
        root = ProductsModelNode("ScoreDelegateRoot")
        root.add_child(top)
        root.add_child(mid)
        root.add_child(low)
        ProductsModel.instance().add_node([], root)

        view = ProductsView()
        qtbot.addWidget(view)
        view.resize(400, 300)
        view.show()
        qtbot.waitExposed(view)

        bar = view.findChild(QTextEdit)
        assert bar is not None

        # ProductsView contains two QListViews: the real results list
        # (m_list_view, backed by ProductsFlatFilterModel) and the search
        # bar's own internal completion popup (QueryLineEdit's private
        # QListView, backed by a QStringListModel). findChild(QListView)
        # alone is ambiguous -- disambiguate by model type.
        list_view = next(
            (lv for lv in view.findChildren(QListView)
             if isinstance(lv.model(), ProductsFlatFilterModel)),
            None)
        assert list_view is not None

        # Filters-only query: has_query is True (so the list view is shown)
        # but there are no free-text tokens, so RelevanceScoreRole is
        # deliberately absent for every row (see product_search_scoring_fixes
        # / the design's edge cases) -- a same-view, no-badge baseline.
        bar.setPlainText(f"provider:{provider}")
        qtbot.waitUntil(lambda: list_view.model().rowCount() == 3, timeout=5000)
        qtbot.wait(100)
        baseline_colors = _distinct_colors(list_view.grab())

        # Keep the provider filter alongside the free-text tokens: a bare
        # "mag fld" free-text query is unscoped and (over a long-running
        # session where ProductsModel is a persistent singleton) can pick up
        # unrelated leftover products from other tests that happen to
        # subsequence-match "mag"/"fld" too -- rowCount() would then settle
        # on some N > 3 instead of 3. Scoping to our unique provider keeps
        # this test isolated while still exercising free-text relevance
        # scoring (RelevanceScoreRole requires free_text_tokens non-empty).
        #
        # This also sidesteps the same race as the baseline query: both
        # waits target rowCount() == 3, but here it's genuinely a fresh
        # requery (the previous provider-only query had no free-text tokens,
        # so relevance was absent) -- waiting for the role to appear once
        # rowCount is stable is still the more precise, race-free condition.
        bar.setPlainText(f"provider:{provider} mag fld")

        def _relevance_ready():
            model = list_view.model()
            if model.rowCount() != 3:
                return False
            return model.data(model.index(0, 0), RELEVANCE_ROLE) is not None

        qtbot.waitUntil(_relevance_ready, timeout=5000)
        qtbot.wait(100)
        colored_colors = _distinct_colors(list_view.grab())

        assert colored_colors > baseline_colors
