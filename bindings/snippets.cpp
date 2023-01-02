// @snippet plot-multiline
NpArray_view x{pyArgs[0]};
NpArray_view y{pyArgs[1]};
%CPPSELF.plot_ptr(x.data(),y.data(), x.flat_size(), y.flat_size(), cppArg2);
// @snippet plot-multiline


// @snippet plot-line
NpArray_view x{pyArgs[0]};
NpArray_view y{pyArgs[1]};
%CPPSELF.plot_ptr(x.data(),y.data(), x.flat_size(), y.flat_size());
// @snippet plot-line


// @snippet plot-colormap
NpArray_view x{pyArgs[0]};
NpArray_view y{pyArgs[1]};
NpArray_view z{pyArgs[2]};
%CPPSELF.plot_ptr(x.data(),y.data(), z.data(), x.flat_size(), y.flat_size(), z.flat_size());
// @snippet plot-colormap

// @snippet QCPGraph-setData
NpArray_view x{pyArgs[0]};
NpArray_view y{pyArgs[1]};
QVector<QCPGraphData> data(x.flat_size());
const auto x_data = x.data();
const auto y_data = y.data();
for(auto i=0UL;i<x.flat_size();i++)
{
    data[i]={x_data[i],y_data[i]};
}
%CPPSELF.data()->set(data,true);
// @snippet QCPGraph-setData
