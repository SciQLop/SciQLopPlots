// @snippet plot-multiline
NpArray_view x{pyArgs[0]};
NpArray_view y{pyArgs[1]};
%CPPSELF.plot_ptr(x.data(),y.data(), x.flat_size(), y.flat_size(), cppArg2);
// @snippet plot-multiline


// @snippet plot-line
NpArray_view x{pyArgs[0]};
NpArray_view y{pyArgs[1]};
%CPPSELF.plot_ptr(x.data(),y.data(), x.flat_size(), y.flat_size());
// @snippet plot-iline
