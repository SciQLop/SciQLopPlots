"""A strided or reversed view is plotted as is (final review, Task 10); a broadcast view says why not.

These views used to be refused with "must be contiguous" (#115). The converter now works on a contiguous
copy, as for datetime64. A broadcast view (a zero stride) can be tiny in memory and huge once copied, so
it is a ValueError asking for an explicit copy.
"""
import numpy as np
import pytest


@pytest.mark.parametrize("view", [lambda a: a[::2], lambda a: a[::-1]], ids=["strided", "reversed"])
def test_a_non_contiguous_array_is_plotted(plot, view):
    x, y = np.linspace(0.0, 10.0, 200), np.sin(np.linspace(0.0, 10.0, 200))
    graph = plot.plot(view(x), view(y))
    np.testing.assert_allclose(np.asarray(graph.data()[1]).ravel(), view(y))


def test_a_broadcast_view_is_a_value_error(plot):
    with pytest.raises(ValueError, match="broadcast"):
        plot.plot(np.broadcast_to(np.array([1.0]), (200,)), np.zeros(200))
