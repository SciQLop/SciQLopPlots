"""ProductsView: filtering must not switch the tree view to the flat list.

The tree is the default presentation; the list is opt-in through the
"List" toggle that appears while a query is active."""
from PySide6.QtWidgets import (
    QListView, QStackedWidget, QTextEdit, QToolButton, QTreeView,
)

from SciQLopPlots import ProductsFlatFilterModel, ProductsView


def _view(qtbot):
    view = ProductsView()
    qtbot.addWidget(view)
    return view


def _stack(view):
    return view.findChild(QStackedWidget)


def _tree(view):
    return view.findChild(QTreeView)


def _list(view):
    return next(lv for lv in view.findChildren(QListView)
                if isinstance(lv.model(), ProductsFlatFilterModel))


def _toggle(view):
    return next(b for b in view.findChildren(QToolButton) if b.text() == "List")


def _set_query(qtbot, view, text):
    view.findChild(QTextEdit).setPlainText(text)
    qtbot.waitUntil(lambda: _toggle(view).isHidden() == (text == ""), timeout=2000)


class TestProductsViewMode:
    def test_tree_is_shown_before_any_query(self, qtbot):
        view = _view(qtbot)
        assert _stack(view).currentWidget() is _tree(view)

    def test_query_keeps_the_tree_view(self, qtbot):
        view = _view(qtbot)
        _set_query(qtbot, view, "magnetic")
        assert _stack(view).currentWidget() is _tree(view)
        assert not _toggle(view).isChecked()

    def test_toggle_opts_into_the_list_and_back(self, qtbot):
        view = _view(qtbot)
        _set_query(qtbot, view, "magnetic")
        _toggle(view).setChecked(True)
        assert _stack(view).currentWidget() is _list(view)
        _toggle(view).setChecked(False)
        assert _stack(view).currentWidget() is _tree(view)

    def test_list_choice_survives_a_new_query(self, qtbot):
        view = _view(qtbot)
        _set_query(qtbot, view, "magnetic")
        _toggle(view).setChecked(True)
        _set_query(qtbot, view, "magnetic field")
        assert _stack(view).currentWidget() is _list(view)

    def test_clearing_the_query_returns_to_the_tree(self, qtbot):
        view = _view(qtbot)
        _set_query(qtbot, view, "magnetic")
        _toggle(view).setChecked(True)
        _set_query(qtbot, view, "")
        assert _stack(view).currentWidget() is _tree(view)
