"""API review 2.11: plot.axis(type) reaches the secondary axes."""
from SciQLopPlots import AxisType


def test_axis_type_reaches_x2_and_y2(plot):
    assert plot.axis(AxisType.XAxis2) is plot.x2_axis()
    assert plot.axis(AxisType.YAxis2) is plot.y2_axis()
    assert plot.axis(AxisType.YAxis2) is not None


def test_existing_values_keep_their_numbers():
    assert [int(AxisType.NoneAxis), int(AxisType.TimeAxis), int(AxisType.XAxis),
            int(AxisType.YAxis), int(AxisType.ZAxis)] == [0, 1, 2, 3, 4]
