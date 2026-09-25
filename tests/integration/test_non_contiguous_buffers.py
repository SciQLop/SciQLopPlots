"""A non-contiguous array says what is wrong with it (#115).

numpy refuses a contiguous buffer for a strided or reversed view, and the error
used to be a bare 'Failed to get buffer from object'.
"""
import numpy as np
import pytest


@pytest.mark.parametrize("view", [lambda a: a[::2], lambda a: a[::-1]], ids=["strided", "reversed"])
def test_a_non_contiguous_array_says_it_must_be_contiguous(plot, view):
    y = np.sin(np.linspace(0.0, 10.0, 200))
    with pytest.raises(TypeError, match="contiguous"):
        plot.plot(view(np.linspace(0.0, 10.0, 200)), view(y))
