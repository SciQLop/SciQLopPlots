"""Scripted story tests — human-readable workflows with automatic narrative on failure."""
from tests.fuzzing.plot_actions import add_plot, remove_last_plot, remove_random_plot
from tests.fuzzing.graph_actions import add_line_graph, add_colormap, set_graph_data
from tests.fuzzing.item_actions import add_vertical_span


def test_create_plot_and_add_line(story_runner):
    plot_idx = story_runner.run(add_plot)
    story_runner.run(add_line_graph, plot_index=plot_idx)
    assert story_runner.model.graph_counts[plot_idx] == 1


def test_create_plot_add_colormap(story_runner):
    plot_idx = story_runner.run(add_plot)
    story_runner.run(add_colormap, plot_index=plot_idx)
    assert story_runner.model.graph_counts[plot_idx] == 1


def test_create_remove_plot(story_runner):
    story_runner.run(add_plot)
    story_runner.run(remove_last_plot)
    assert story_runner.model.plot_count == 0


def test_multiple_plots_with_graphs(story_runner):
    p0 = story_runner.run(add_plot)
    p1 = story_runner.run(add_plot)
    story_runner.run(add_line_graph, plot_index=p0)
    story_runner.run(add_line_graph, plot_index=p1)
    story_runner.run(add_colormap, plot_index=p0)
    assert story_runner.model.plot_count == 2
    assert story_runner.model.graph_counts[p0] == 2
    assert story_runner.model.graph_counts[p1] == 1


def test_add_span_to_plot(story_runner):
    plot_idx = story_runner.run(add_plot)
    story_runner.run(add_line_graph, plot_index=plot_idx)
    story_runner.run(add_vertical_span, plot_index=plot_idx)
    assert story_runner.model.span_counts[plot_idx] == 1


def test_set_data_on_graph(story_runner):
    plot_idx = story_runner.run(add_plot)
    story_runner.run(add_line_graph, plot_index=plot_idx)
    story_runner.run(set_graph_data, plot_index=plot_idx)


def test_add_and_remove_multiple_plots(story_runner):
    for _ in range(5):
        story_runner.run(add_plot)
    for _ in range(3):
        story_runner.run(remove_last_plot)
    assert story_runner.model.plot_count == 2
