/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2026, Plasma Physics Laboratory - CNRS
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/
// Python first: Qt's `slots` macro breaks its headers.
#if __has_include(<sbkerrors.h>)
#include <Python.h>
#include <sbkerrors.h>
#define SCIQLOP_RAISE_IN_PYTHON 1
#endif

#include "SciQLopPlots/Debug.hpp"
#include <cstdlib>
#include <memory>
#if defined(__GNUG__)
#include <cxxabi.h>
#endif

namespace
{
std::string readable(const char* type_name)
{
#if defined(__GNUG__)
    int status = 0;
    std::unique_ptr<char, void (*)(void*)> demangled {
        abi::__cxa_demangle(type_name, nullptr, nullptr, &status), std::free
    };
    if (status == 0)
        return demangled.get();
#endif
    return type_name;
}
}

std::string unsupported_message(const char* function, const char* concrete_class)
{
    return std::string(function) + " is not supported by " + readable(concrete_class);
}

void report_unsupported_call(const char* function, const char* concrete_class)
{
    const auto name = readable(concrete_class);
#if SCIQLOP_RAISE_IN_PYTHON
    if (Py_IsInitialized())
    {
        // storeErrorOrPrint keeps the error for the Python call in progress to raise, and prints
        // it when C++ alone made the call. Safe from any thread, and in noexcept stubs.
        const auto gil = PyGILState_Ensure();
        PyErr_SetString(PyExc_NotImplementedError,
                        unsupported_message(function, concrete_class).c_str());
        Shiboken::Errors::storeErrorOrPrint();
        PyGILState_Release(gil);
        return;
    }
#endif
    qWarning() << "Abstract method called:" << function << "from concrete class:" << name.c_str();
}
