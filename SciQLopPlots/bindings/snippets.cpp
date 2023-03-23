// @snippet QCPGraph-setData
NpArray_view x{pyArgs[0]};
NpArray_view y{pyArgs[1]};
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
%CPPSELF.setData(NpArray_view{pyArgs[0]}, NpArray_view{pyArgs[1]});
// @snippet SciQLopGraph-setData

// @snippet SciQLopColorMap-setData
%CPPSELF.setData(NpArray_view{pyArgs[0]}, NpArray_view{pyArgs[1]}, NpArray_view{pyArgs[2]});
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
