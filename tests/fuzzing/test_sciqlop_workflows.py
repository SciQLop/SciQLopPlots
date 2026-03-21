"""Scripted stories that reproduce typical SciQLop sessions.

Each test mimics a real SciQLop workflow: create panels, add/remove plots,
populate them with callback-based graphs (time series + spectrograms),
then torture the viewport to shake out race conditions and crashes.
"""
from tests.fuzzing.plot_actions import (
    add_plot, remove_last_plot, remove_random_plot, insert_plot_at_random,
)
from tests.fuzzing.panel_actions import add_nested_panel, remove_nested_panel
from tests.fuzzing.callback_actions import (
    add_callback_line_3c, add_callback_line_1c, add_callback_spectrogram,
    add_slow_callback_line, add_flaky_callback_line, add_flaky_callback_spectrogram,
)
from tests.fuzzing.item_actions import (
    add_vertical_span, add_multi_plot_span, add_span_burst, add_per_plot_span_burst,
)
from tests.fuzzing.torture_actions import (
    rapid_zoom_burst, rapid_pan_burst, rescale_y, rapid_log_toggle,
    set_panel_time_range, zoom_pan_zoom, resize_panel, rapid_resize_burst,
)


def test_basic_timeseries_session(story_runner):
    """One plot with a 3-component line graph, then zoom and pan rapidly."""
    p = story_runner.run(add_plot)
    story_runner.run(add_callback_line_3c, plot_index=p)
    story_runner.run(set_panel_time_range)
    story_runner.run(rapid_zoom_burst, plot_index=p)
    story_runner.run(rapid_pan_burst, plot_index=p)


def test_spectrogram_with_log_toggle(story_runner):
    """Spectrogram on a plot, then toggle log scale rapidly — known crash trigger."""
    p = story_runner.run(add_plot)
    story_runner.run(add_callback_spectrogram, plot_index=p)
    story_runner.run(set_panel_time_range)
    story_runner.run(rapid_log_toggle, plot_index=p)
    story_runner.run(rescale_y, plot_index=p)


def test_multi_plot_mixed_session(story_runner):
    """Two plots: one with line graphs, one with a spectrogram, then torture both."""
    p0 = story_runner.run(add_plot)
    p1 = story_runner.run(add_plot)
    story_runner.run(add_callback_line_3c, plot_index=p0)
    story_runner.run(add_callback_line_1c, plot_index=p0)
    story_runner.run(add_callback_spectrogram, plot_index=p1)
    story_runner.run(set_panel_time_range)
    story_runner.run(rapid_zoom_burst, plot_index=p0)
    story_runner.run(zoom_pan_zoom, plot_index=p1)
    story_runner.run(rapid_pan_burst, plot_index=p0)
    story_runner.run(rapid_log_toggle, plot_index=p1)


def test_slow_provider_under_rapid_zoom(story_runner):
    """Slow provider (50ms latency) with rapid viewport changes — stresses async pipeline."""
    p = story_runner.run(add_plot)
    story_runner.run(add_slow_callback_line, plot_index=p)
    story_runner.run(set_panel_time_range)
    story_runner.run(rapid_zoom_burst, plot_index=p)
    story_runner.run(zoom_pan_zoom, plot_index=p)
    story_runner.run(rapid_pan_burst, plot_index=p)


def test_flaky_providers_session(story_runner):
    """Flaky providers that randomly return None — should not crash."""
    p0 = story_runner.run(add_plot)
    p1 = story_runner.run(add_plot)
    story_runner.run(add_flaky_callback_line, plot_index=p0)
    story_runner.run(add_flaky_callback_spectrogram, plot_index=p1)
    story_runner.run(set_panel_time_range)
    story_runner.run(rapid_zoom_burst, plot_index=p0)
    story_runner.run(rapid_zoom_burst, plot_index=p1)
    story_runner.run(set_panel_time_range)
    story_runner.run(rapid_pan_burst, plot_index=p0)


def test_dense_panel_torture(story_runner):
    """Three plots with multiple graphs each, then set time range — max callback pressure."""
    plots = [story_runner.run(add_plot) for _ in range(3)]
    story_runner.run(add_callback_line_3c, plot_index=plots[0])
    story_runner.run(add_callback_line_1c, plot_index=plots[0])
    story_runner.run(add_callback_spectrogram, plot_index=plots[1])
    story_runner.run(add_callback_line_3c, plot_index=plots[1])
    story_runner.run(add_flaky_callback_line, plot_index=plots[2])
    story_runner.run(add_flaky_callback_spectrogram, plot_index=plots[2])
    story_runner.run(set_panel_time_range)
    for p in plots:
        story_runner.run(rapid_zoom_burst, plot_index=p)
    story_runner.run(set_panel_time_range)


def test_zoom_pan_zoom_all_graph_types(story_runner):
    """Every graph type gets the zoom-pan-zoom sequence."""
    p0 = story_runner.run(add_plot)
    p1 = story_runner.run(add_plot)
    story_runner.run(add_callback_line_3c, plot_index=p0)
    story_runner.run(add_callback_spectrogram, plot_index=p1)
    story_runner.run(set_panel_time_range)
    story_runner.run(zoom_pan_zoom, plot_index=p0)
    story_runner.run(zoom_pan_zoom, plot_index=p1)
    story_runner.run(rescale_y, plot_index=p0)
    story_runner.run(rescale_y, plot_index=p1)


# --- Plot and panel lifecycle stress tests ---


def test_add_remove_plots_with_active_callbacks(story_runner):
    """Add plots with callbacks, remove some, add more — stresses lifecycle management."""
    p0 = story_runner.run(add_plot)
    p1 = story_runner.run(add_plot)
    p2 = story_runner.run(add_plot)
    story_runner.run(add_callback_line_3c, plot_index=p0)
    story_runner.run(add_callback_spectrogram, plot_index=p1)
    story_runner.run(add_flaky_callback_line, plot_index=p2)
    story_runner.run(set_panel_time_range)
    # Remove the middle plot while callbacks may still be in flight
    story_runner.run(remove_random_plot)
    story_runner.run(set_panel_time_range)
    # Add a new plot into the gap
    story_runner.run(insert_plot_at_random)
    story_runner.run(set_panel_time_range)
    # Remove everything
    story_runner.run(remove_last_plot)
    story_runner.run(remove_last_plot)
    story_runner.run(remove_last_plot)


def test_rapid_plot_churn(story_runner):
    """Rapidly create and destroy plots — tests cleanup paths."""
    for _ in range(5):
        p = story_runner.run(add_plot)
        story_runner.run(add_callback_line_1c, plot_index=p)
    story_runner.run(set_panel_time_range)
    for _ in range(3):
        story_runner.run(remove_random_plot)
    story_runner.run(set_panel_time_range)
    for _ in range(3):
        story_runner.run(insert_plot_at_random)
    story_runner.run(remove_last_plot)
    story_runner.run(remove_last_plot)


def test_nested_panel_session(story_runner):
    """Add nested panels, populate, torture, then remove panels."""
    story_runner.run(add_nested_panel)
    p0 = story_runner.run(add_plot)
    story_runner.run(add_callback_line_3c, plot_index=p0)
    story_runner.run(add_nested_panel)
    p1 = story_runner.run(add_plot)
    story_runner.run(add_callback_spectrogram, plot_index=p1)
    story_runner.run(set_panel_time_range)
    story_runner.run(rapid_zoom_burst, plot_index=p0)
    story_runner.run(remove_nested_panel)
    story_runner.run(set_panel_time_range)


def test_remove_plot_during_zoom(story_runner):
    """Remove a plot while rapid zooming is happening on another — race condition probe."""
    p0 = story_runner.run(add_plot)
    p1 = story_runner.run(add_plot)
    story_runner.run(add_callback_line_3c, plot_index=p0)
    story_runner.run(add_slow_callback_line, plot_index=p1)
    story_runner.run(set_panel_time_range)
    story_runner.run(rapid_zoom_burst, plot_index=p0)
    # Remove p1 while p0's callbacks are still in flight
    story_runner.run(remove_last_plot)
    story_runner.run(set_panel_time_range)
    story_runner.run(rapid_pan_burst, plot_index=p0)


# --- Resize stress tests ---


def test_resize_with_active_callbacks(story_runner):
    """Resize panel while callback graphs are active — stresses paint buffer reallocation."""
    p0 = story_runner.run(add_plot)
    p1 = story_runner.run(add_plot)
    story_runner.run(add_callback_line_3c, plot_index=p0)
    story_runner.run(add_callback_spectrogram, plot_index=p1)
    story_runner.run(set_panel_time_range)
    story_runner.run(rapid_resize_burst)
    story_runner.run(set_panel_time_range)
    story_runner.run(resize_panel, width=1920, height=1080)
    story_runner.run(rapid_zoom_burst, plot_index=p0)
    story_runner.run(resize_panel, width=100, height=50)
    story_runner.run(rapid_pan_burst, plot_index=p1)


def test_tiny_widget_with_data(story_runner):
    """Shrink to near-zero size with data — edge case for paint buffers and text layout."""
    p = story_runner.run(add_plot)
    story_runner.run(add_callback_line_3c, plot_index=p)
    story_runner.run(set_panel_time_range)
    story_runner.run(resize_panel, width=50, height=50)
    story_runner.run(rapid_zoom_burst, plot_index=p)
    story_runner.run(resize_panel, width=1, height=1)
    story_runner.run(set_panel_time_range)
    story_runner.run(resize_panel, width=800, height=600)


def test_resize_during_plot_churn(story_runner):
    """Add/remove plots while resizing — combined lifecycle + layout stress."""
    story_runner.run(resize_panel, width=400, height=300)
    p0 = story_runner.run(add_plot)
    story_runner.run(add_callback_line_1c, plot_index=p0)
    story_runner.run(resize_panel, width=1200, height=800)
    p1 = story_runner.run(add_plot)
    story_runner.run(add_callback_spectrogram, plot_index=p1)
    story_runner.run(rapid_resize_burst)
    story_runner.run(remove_last_plot)
    story_runner.run(resize_panel, width=50, height=50)
    story_runner.run(set_panel_time_range)
    story_runner.run(resize_panel, width=800, height=600)


# --- Span / catalog stress tests ---


def test_catalog_load_with_zoom(story_runner):
    """Load a burst of multi-plot spans (catalog events), then zoom/pan — mimics SciQLop catalog view."""
    p0 = story_runner.run(add_plot)
    p1 = story_runner.run(add_plot)
    story_runner.run(add_callback_line_3c, plot_index=p0)
    story_runner.run(add_callback_spectrogram, plot_index=p1)
    story_runner.run(set_panel_time_range)
    story_runner.run(add_span_burst)
    story_runner.run(rapid_zoom_burst, plot_index=p0)
    story_runner.run(rapid_pan_burst, plot_index=p1)
    story_runner.run(set_panel_time_range)


def test_per_plot_spans_with_zoom(story_runner):
    """Per-plot vertical spans on multiple plots, then torture viewport."""
    p0 = story_runner.run(add_plot)
    p1 = story_runner.run(add_plot)
    story_runner.run(add_callback_line_3c, plot_index=p0)
    story_runner.run(add_callback_line_1c, plot_index=p1)
    story_runner.run(set_panel_time_range)
    story_runner.run(add_per_plot_span_burst, plot_index=p0)
    story_runner.run(add_per_plot_span_burst, plot_index=p1)
    story_runner.run(rapid_zoom_burst, plot_index=p0)
    story_runner.run(zoom_pan_zoom, plot_index=p1)


def test_spans_with_plot_removal(story_runner):
    """Add spans, then remove plots — tests span cleanup when parent plot dies."""
    p0 = story_runner.run(add_plot)
    p1 = story_runner.run(add_plot)
    story_runner.run(add_callback_line_3c, plot_index=p0)
    story_runner.run(set_panel_time_range)
    story_runner.run(add_vertical_span, plot_index=p0)
    story_runner.run(add_multi_plot_span)
    story_runner.run(add_span_burst)
    story_runner.run(remove_last_plot)
    story_runner.run(set_panel_time_range)
    story_runner.run(rapid_zoom_burst, plot_index=p0)


def test_spans_with_resize(story_runner):
    """Catalog spans + rapid resize — stresses paint buffer with many overlay items."""
    p0 = story_runner.run(add_plot)
    story_runner.run(add_callback_line_3c, plot_index=p0)
    story_runner.run(set_panel_time_range)
    story_runner.run(add_span_burst)
    story_runner.run(add_multi_plot_span)
    story_runner.run(add_multi_plot_span)
    story_runner.run(rapid_resize_burst)
    story_runner.run(resize_panel, width=50, height=50)
    story_runner.run(set_panel_time_range)
    story_runner.run(resize_panel, width=1920, height=1080)


def test_spans_with_log_toggle(story_runner):
    """Spans + rapid log scale toggling — known crash trigger in SciQLop."""
    p0 = story_runner.run(add_plot)
    story_runner.run(add_callback_spectrogram, plot_index=p0)
    story_runner.run(set_panel_time_range)
    story_runner.run(add_per_plot_span_burst, plot_index=p0)
    story_runner.run(add_multi_plot_span)
    story_runner.run(rapid_log_toggle, plot_index=p0)
    story_runner.run(rapid_zoom_burst, plot_index=p0)
