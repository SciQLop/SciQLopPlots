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


# ---------- validate_same_length ----------

def test_validate_same_length_ok():
    x = np.arange(10.0)
    y = np.arange(10.0)
    sqp.validate_same_length(x, "x", y, "y")


def test_validate_same_length_mismatch_raises_valueerror():
    x = np.arange(10.0)
    y = np.arange(5.0)
    with pytest.raises(ValueError, match=r"length"):
        sqp.validate_same_length(x, "x", y, "y")


# ---------- validate_xy ----------

def test_validate_xy_1d_ok():
    x = np.arange(10.0)
    y = np.arange(10.0)
    sqp.validate_xy(x, y)


def test_validate_xy_2d_y_ok():
    x = np.arange(10.0)
    y = np.random.rand(10, 3)
    sqp.validate_xy(x, y)


def test_validate_xy_x_not_1d_raises():
    x = np.random.rand(10, 3)
    y = np.arange(10.0)
    with pytest.raises(ValueError, match=r"x"):
        sqp.validate_xy(x, y)


def test_validate_xy_shape_mismatch_raises():
    x = np.arange(10.0)
    y = np.arange(5.0)
    with pytest.raises(ValueError, match=r"length"):
        sqp.validate_xy(x, y)


def test_validate_xy_y_3d_raises():
    x = np.arange(10.0)
    y = np.random.rand(10, 3, 2)
    with pytest.raises(ValueError, match=r"y"):
        sqp.validate_xy(x, y)


# ---------- validate_xyz ----------

def test_validate_xyz_ok():
    x = np.arange(10.0)
    y = np.arange(5.0)
    z = np.random.rand(10, 5)
    sqp.validate_xyz(x, y, z)


def test_validate_xyz_z_wrong_shape_raises():
    x = np.arange(10.0)
    y = np.arange(5.0)
    z = np.random.rand(9, 5)  # wrong x dimension
    with pytest.raises(ValueError, match=r"z"):
        sqp.validate_xyz(x, y, z)


# ---------- validate_nd_list ----------

def test_validate_nd_list_ok():
    arrs = [np.arange(10.0), np.arange(10.0), np.arange(10.0)]
    sqp.validate_nd_list(arrs, 3)


def test_validate_nd_list_wrong_count_raises():
    arrs = [np.arange(10.0), np.arange(10.0)]
    with pytest.raises(ValueError, match=r"expected 3.*got 2"):
        sqp.validate_nd_list(arrs, 3)


def test_validate_nd_list_length_mismatch_raises():
    arrs = [np.arange(10.0), np.arange(5.0), np.arange(10.0)]
    with pytest.raises(ValueError, match=r"length"):
        sqp.validate_nd_list(arrs, 3)


# ---------- validate_finite ----------

def test_validate_finite_ok():
    sqp.validate_finite(1.0, "v")
    sqp.validate_finite(-1e9, "v")


def test_validate_finite_nan_raises():
    with pytest.raises(ValueError, match=r"v.*finite"):
        sqp.validate_finite(float("nan"), "v")


def test_validate_finite_inf_raises():
    with pytest.raises(ValueError, match=r"v.*finite"):
        sqp.validate_finite(float("inf"), "v")
