"""Tests for the product search-bar help (auto-generated + override)."""
import pytest
from PySide6.QtWidgets import QTextEdit
from SciQLopPlots import (
    ProductsView, ProductsModel, ProductsModelNode,
    ProductsModelNodeType, ParameterType,
)


def test_auto_help_lists_builtin_fields_and_shortcut(qtbot):
    view = ProductsView()
    qtbot.addWidget(view)
    help_text = view.search_help()
    for field in ("provider", "type", "after", "before"):
        assert field in help_text
    assert "Ctrl+Space" in help_text


def test_set_search_help_overrides_auto(qtbot):
    view = ProductsView()
    qtbot.addWidget(view)
    view.set_search_help("<b>Custom help</b>")
    assert view.search_help() == "<b>Custom help</b>"


def test_empty_override_reverts_to_auto(qtbot):
    view = ProductsView()
    qtbot.addWidget(view)
    view.set_search_help("<b>Custom help</b>")
    view.set_search_help("")
    assert "provider" in view.search_help()
    assert view.search_help() != "<b>Custom help</b>"


def test_search_bar_tooltip_reflects_help(qtbot):
    view = ProductsView()
    qtbot.addWidget(view)
    view.set_search_help("<b>Custom help</b>")
    bar = view.findChild(QTextEdit)
    assert bar is not None
    assert bar.toolTip() == "<b>Custom help</b>"


def test_auto_help_includes_discovered_metadata_key(qtbot):
    model = ProductsModel.instance()
    root = ProductsModelNode("help_test_provider")
    node = ProductsModelNode(
        "HelpProbe", "help_test_provider",
        {"mission": "helptest_mission"},
        ProductsModelNodeType.PARAMETER, ParameterType.Scalar)
    root.add_child(node)
    model.add_node([], root)

    # ProductsView ctor runs refresh_completions() synchronously over the model,
    # so constructing after populating gives deterministic auto-help.
    view = ProductsView()
    qtbot.addWidget(view)
    assert "mission" in view.search_help()
