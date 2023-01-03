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
%CPPSELF.data()->set(std::move(data),true);
// @snippet QCPGraph-setData
