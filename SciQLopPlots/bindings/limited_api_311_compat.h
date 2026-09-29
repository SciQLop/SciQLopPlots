// Force-included into the shiboken builds when compiling against CPython 3.11.
//
// 3.11 put the buffer protocol in the limited API but left the getbufferproc and
// releasebufferproc typedefs out: they only joined in 3.12 (pybuffer.h). Shiboken
// 6.11's bufferprocs_py37.h uses them whenever Py_LIMITED_API >= 3.11. They are
// type names only, identical to CPython's own, and add nothing to the binary.
#pragma once

#include <Python.h>

#if defined(Py_LIMITED_API) && PY_VERSION_HEX < 0x030C0000
typedef int (*getbufferproc)(PyObject*, Py_buffer*, int);
typedef void (*releasebufferproc)(PyObject*, Py_buffer*);
#endif
