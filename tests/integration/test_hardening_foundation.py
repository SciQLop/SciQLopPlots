"""Tests for Sub-effort 1 (Foundation) of python-hardening."""
import numpy as np
import pytest
import SciQLopPlots as sqp


# ---------- validate_buffer ----------

def test_validate_buffer_none_raises_typeerror():
    # Task 11 will pin exception taxonomy precisely (TypeError vs ValueError).
    with pytest.raises((TypeError, ValueError)):
        sqp.validate_buffer(None, "x")


def test_validate_buffer_non_numeric_raises_typeerror():
    arr = np.array(["a", "b"], dtype=object)
    # Task 11 will pin exception taxonomy precisely (TypeError vs ValueError).
    with pytest.raises((TypeError, ValueError)):
        sqp.validate_buffer(arr, "x")


def test_validate_buffer_ok():
    arr = np.arange(10.0)
    sqp.validate_buffer(arr, "x")  # no exception


def test_validate_buffer_wrong_ndim_raises_valueerror():
    arr = np.arange(10.0)
    with pytest.raises(ValueError, match=r"x.*ndim"):
        sqp.validate_buffer(arr, "x", 2)


def test_validate_buffer_wrong_dtype_raises_typeerror():
    arr = np.arange(10, dtype=np.int32)
    # Task 11 will pin exception taxonomy precisely (TypeError vs ValueError).
    with pytest.raises((TypeError, ValueError), match=r"x.*dtype"):
        sqp.validate_buffer(arr, "x", -1, ord('d'))


# ---------- validate_index ----------

def test_validate_index_ok():
    sqp.validate_index(0, 10, "row")
    sqp.validate_index(9, 10, "row")


def test_validate_index_negative_raises_indexerror():
    with pytest.raises(IndexError, match=r"row"):
        sqp.validate_index(-1, 10, "row")


def test_validate_index_out_of_range_raises_indexerror():
    with pytest.raises(IndexError, match=r"row"):
        sqp.validate_index(10, 10, "row")


def test_validate_index_empty_container_always_raises():
    with pytest.raises(IndexError):
        sqp.validate_index(0, 0, "row")
