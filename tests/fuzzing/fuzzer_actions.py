"""The actions the Hypothesis fuzzer draws from, in one list the tests can inspect."""
from tests.fuzzing.plot_actions import (
    add_plot, remove_last_plot, remove_random_plot, insert_plot_at_random,
)
from tests.fuzzing.panel_actions import add_nested_panel, remove_nested_panel
from tests.fuzzing.graph_actions import add_line_graph, add_colormap, set_graph_data
from tests.fuzzing.timeline_actions import add_timeline_strip
from tests.fuzzing.item_actions import (
    add_vertical_span, add_multi_plot_span, add_span_burst, add_per_plot_span_burst,
)
from tests.fuzzing.axis_actions import set_axis_range, toggle_y_log_scale
from tests.fuzzing.callback_actions import (
    add_callback_line_3c, add_callback_line_1c, add_callback_spectrogram,
    add_slow_callback_line, add_flaky_callback_line, add_flaky_callback_spectrogram,
    swap_callback,
)
from tests.fuzzing.torture_actions import (
    rapid_zoom_burst, rapid_pan_burst, rescale_y, rapid_log_toggle,
    set_panel_time_range, zoom_pan_zoom, resize_panel, rapid_resize_burst,
)

FUZZER_ACTIONS = [
    # Plot lifecycle
    add_plot, remove_last_plot, remove_random_plot, insert_plot_at_random,
    # Panel lifecycle
    add_nested_panel, remove_nested_panel,
    # Static graphs
    add_line_graph, add_colormap, set_graph_data, add_timeline_strip,
    add_vertical_span, add_multi_plot_span, add_span_burst, add_per_plot_span_burst,
    set_axis_range, toggle_y_log_scale,
    # Callback-based graphs (SciQLop-style)
    add_callback_line_3c, add_callback_line_1c, add_callback_spectrogram,
    add_slow_callback_line, add_flaky_callback_line, add_flaky_callback_spectrogram,
    swap_callback,
    # Viewport torture
    rapid_zoom_burst, rapid_pan_burst, rescale_y, rapid_log_toggle,
    set_panel_time_range, zoom_pan_zoom, resize_panel, rapid_resize_burst,
]
