"""Integration tests for SciQLopOverlay wrapper."""
import pytest
from PySide6.QtGui import QFont

from SciQLopPlots import (
    SciQLopPlot,
    SciQLopOverlay,
    OverlayLevel,
    OverlaySizeMode,
    OverlayPosition,
)


class TestOverlayAccess:
    def test_plot_has_overlay(self, plot):
        ov = plot.overlay()
        assert ov is not None
        assert isinstance(ov, SciQLopOverlay)

    def test_overlay_is_singleton(self, plot):
        assert plot.overlay() is plot.overlay()


class TestOverlayShowClear:
    def test_show_message_default(self, plot):
        ov = plot.overlay()
        ov.show_message("hello")
        assert ov.text() == "hello"
        assert ov.level() == OverlayLevel.Info
        assert ov.size_mode() == OverlaySizeMode.Compact
        assert ov.position() == OverlayPosition.Top

    def test_show_message_with_params(self, plot):
        ov = plot.overlay()
        ov.show_message("warn", OverlayLevel.Warning, OverlaySizeMode.FitContent,
                        OverlayPosition.Bottom)
        assert ov.text() == "warn"
        assert ov.level() == OverlayLevel.Warning
        assert ov.size_mode() == OverlaySizeMode.FitContent
        assert ov.position() == OverlayPosition.Bottom

    def test_show_error(self, plot):
        ov = plot.overlay()
        ov.show_message("error!", OverlayLevel.Error)
        assert ov.level() == OverlayLevel.Error

    def test_show_full_widget(self, plot):
        ov = plot.overlay()
        ov.show_message("big", size_mode=OverlaySizeMode.FullWidget)
        assert ov.size_mode() == OverlaySizeMode.FullWidget

    def test_clear_message(self, plot):
        ov = plot.overlay()
        ov.show_message("text")
        assert ov.text() == "text"
        ov.clear_message()
        assert ov.text() == ""

    def test_position_left_right(self, plot):
        ov = plot.overlay()
        ov.show_message("left", position=OverlayPosition.Left)
        assert ov.position() == OverlayPosition.Left
        ov.show_message("right", position=OverlayPosition.Right)
        assert ov.position() == OverlayPosition.Right


class TestOverlayProperties:
    def test_collapsible_default_false(self, plot):
        ov = plot.overlay()
        assert ov.is_collapsible() is False

    def test_set_collapsible(self, plot):
        ov = plot.overlay()
        ov.set_collapsible(True)
        assert ov.is_collapsible() is True
        ov.set_collapsible(False)
        assert ov.is_collapsible() is False

    def test_collapsed_default_false(self, plot):
        ov = plot.overlay()
        assert ov.is_collapsed() is False

    def test_set_collapsed(self, plot):
        ov = plot.overlay()
        ov.set_collapsible(True)
        ov.set_collapsed(True)
        assert ov.is_collapsed() is True
        ov.set_collapsed(False)
        assert ov.is_collapsed() is False

    def test_opacity_default(self, plot):
        ov = plot.overlay()
        assert ov.opacity() == pytest.approx(1.0)

    def test_set_opacity(self, plot):
        ov = plot.overlay()
        ov.set_opacity(0.5)
        assert ov.opacity() == pytest.approx(0.5)

    def test_set_font(self, plot):
        ov = plot.overlay()
        f = QFont("Monospace", 14)
        ov.set_font(f)
        result = ov.font()
        assert result.pointSize() == 14

    def test_overlay_rect_after_show(self, plot):
        ov = plot.overlay()
        ov.show_message("test")
        rect = ov.overlay_rect()
        assert rect is not None


class TestOverlayReplot:
    def test_replot_with_overlay_no_crash(self, plot):
        ov = plot.overlay()
        ov.show_message("info")
        plot.replot(True)

    def test_replot_after_clear_no_crash(self, plot):
        ov = plot.overlay()
        ov.show_message("info")
        plot.replot(True)
        ov.clear_message()
        plot.replot(True)

    def test_replot_full_widget_no_crash(self, plot):
        ov = plot.overlay()
        ov.show_message("big", size_mode=OverlaySizeMode.FullWidget)
        plot.replot(True)

    def test_delete_plot_with_overlay_no_crash(self, plot):
        ov = plot.overlay()
        ov.show_message("test")
        del plot
