/* clang-format off */
// @snippet QCPGraph-setData
Array_view x{pyArgs[0]};
Array_view y{pyArgs[1]};
Py_BEGIN_ALLOW_THREADS
QVector<QCPGraphData> data(x.flat_size());
const auto x_data = x.data();
const auto y_data = y.data();
for(auto i=0UL;i<x.flat_size();i++)
{
    data[i]={x_data[i],y_data[i]};
}
%CPPSELF.data()->set(std::move(data),true);
Py_END_ALLOW_THREADS
// @snippet QCPGraph-setData

// @snippet QCPGraph-setSelected
if(%1)
{
    %CPPSELF->setSelection(QCPDataSelection(%CPPSELF->data()->dataRange()));
}
// @snippet QCPGraph-setSelected

// @snippet QCPColorMapData-cellToCoord
double a=0;
double b=0;
%CPPSELF.%FUNCTION_NAME (%1,%2,&a,&b);
%PYARG_0 = PyTuple_New(2);
PyTuple_SET_ITEM(%PYARG_0, 0, %CONVERTTOPYTHON[double](a));
PyTuple_SET_ITEM(%PYARG_0, 1, %CONVERTTOPYTHON[double](b));
// @snippet QCPColorMapData-cellToCoord

// @snippet QCPColorMapData-coordToCell
int a=0;
int b=0;
%CPPSELF.%FUNCTION_NAME (%1,%2,&a,&b);
%PYARG_0 = PyTuple_New(2);
PyTuple_SET_ITEM(%PYARG_0, 0, %CONVERTTOPYTHON[int](a));
PyTuple_SET_ITEM(%PYARG_0, 1, %CONVERTTOPYTHON[int](b));
// @snippet QCPColorMapData-coordToCell

// @snippet SciQLopGraph-setData
Py_BEGIN_ALLOW_THREADS
%CPPSELF.setData(Array_view{pyArgs[0]}, Array_view{pyArgs[1]});
Py_END_ALLOW_THREADS
// @snippet SciQLopGraph-setData

// @snippet SciQLopColorMap-setData
Py_BEGIN_ALLOW_THREADS
%CPPSELF.setData(Array_view{pyArgs[0]}, Array_view{pyArgs[1]}, Array_view{pyArgs[2]});
Py_END_ALLOW_THREADS
// @snippet SciQLopColorMap-setData

// @snippet QCPAxis-setTicker
%CPPSELF.setTicker( QSharedPointer<QCPAxisTicker>(%1) );
// @snippet QCPAxis-setTicker

// @snippet QCPAxis-removeTicker
%CPPSELF.ticker().clear();
// @snippet QCPAxis-removeTicker

// @snippet QCPAxis-ticker
%RETURN_TYPE r = cppSelf->ticker().data();
%PYARG_0 = %CONVERTTOPYTHON[%RETURN_TYPE](r);
// @snippet QCPAxis-ticker

// @snippet DataProviderInterface-set_data
Py_BEGIN_ALLOW_THREADS
%CPPSELF.set_data(Array_view{pyArgs[0]}, Array_view{pyArgs[1]}, Array_view{pyArgs[2]});
Py_END_ALLOW_THREADS
// @snippet DataProviderInterface-set_data

// @snippet CheckIsBuffer
static bool CheckIsBuffer(PyObject* pyIn) {
    Py_buffer _buffer = { 0 };
    return PyObject_GetBuffer(pyIn, &_buffer, PyBUF_SIMPLE | PyBUF_READ | PyBUF_C_CONTIGUOUS)==0;
}
// @snippet CheckIsBuffer


