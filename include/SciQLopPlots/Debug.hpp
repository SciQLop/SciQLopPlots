/*------------------------------------------------------------------------------
-- This file is a part of the SciQLop Software
-- Copyright (C) 2024, Plasma Physics Laboratory - CNRS
--
-- This program is free software; you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation; either version 2 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program; if not, write to the Free Software
-- Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
-------------------------------------------------------------------------------*/
/*-- Author : Alexis Jeandet
-- Mail : alexis.jeandet@member.fsf.org
----------------------------------------------------------------------------*/
#pragma once
#include <QDebug>
#include <stdexcept>
#include <string>
#include <typeinfo>

// These guard "silently empty plot" failures (wrong set_data overload, wrong
// callable arity, invalid buffer list): all cold paths, so they warn in every
// build — compiled-out diagnostics left users with no way to see why nothing
// was drawn in release.

//! A method this concrete class doesn't implement. Called from Python, it raises
//! NotImplementedError (a pending Python error, never a C++ throw: many stubs are noexcept);
//! called from C++, it only warns. Each build's Python interface defines it.
void report_unsupported_call(const char* function, const char* concrete_class);

//! "<function> is not supported by <class>", with the class name demangled.
std::string unsupported_message(const char* function, const char* concrete_class);

namespace sqp
{
//! A factory this concrete class can't serve. Bindings turn it into NotImplementedError.
//! Derived from runtime_error so C++ catch sites for the old "Not implemented" keep working.
struct unsupported_call : std::runtime_error
{
    using std::runtime_error::runtime_error;
};
}

#define WARN_ABSTRACT_METHOD                                                                       \
    {                                                                                              \
        report_unsupported_call(Q_FUNC_INFO, typeid(*this).name());                                \
    }

#define DEBUG_MESSAGE(message)                                                                     \
    {                                                                                              \
        qWarning() << message;                                                                     \
    }

#define WARN_UNSUPPORTED_FUNCTIONALITY                                                             \
    {                                                                                              \
        report_unsupported_call(Q_FUNC_INFO, typeid(*this).name());                                \
    }
