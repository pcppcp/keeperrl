#pragma once

#ifdef OSX
#include <iomanip>
#else

// Copied from stdc++, because some older platforms don't have this file.
//
// Standard stream manipulators -*- C++ -*-

// Copyright (C) 1997-2015 Free Software Foundation, Inc.
//
// This file is part of the GNU ISO C++ Library.  This library is free
// software; you can redistribute it and/or modify it under the
// terms of the GNU General Public License as published by the
// Free Software Foundation; either version 3, or (at your option)
// any later version.

// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// Under Section 7 of GPL version 3, you are granted additional
// permissions described in the GCC Runtime Library Exception, version
// 3.1, as published by the Free Software Foundation.

// You should have received a copy of the GNU General Public License and
// a copy of the GCC Runtime Library Exception along with this program;
// see the files COPYING3 and COPYING.RUNTIME respectively.  If not, see
// <http://www.gnu.org/licenses/>.


#include "stdafx.h"

namespace std {
/**
 * @brief Manipulator for quoted strings.
 * @param __string String to quote.
 * @param __delim  Character to quote string with.
 * @param __escape Escape character to escape itself or quote character.
 */

namespace __detail {

  /**
   * @brief Struct for delimited strings.
   */
  template<typename _String, typename _CharT>
    struct _Quoted_string
    {
static_assert(is_reference<_String>::value
     || is_pointer<_String>::value,
        "String type must be pointer or reference");

_Quoted_string(_String __str, _CharT __del, _CharT __esc)
: _M_string(__str), _M_delim{__del}, _M_escape{__esc}
{ }

_Quoted_string&
operator=(_Quoted_string&) = delete;

_String _M_string;
_CharT _M_delim;
_CharT _M_escape;
    };

  /**
   * @brief Inserter for quoted strings.
   *
   *  _GLIBCXX_RESOLVE_LIB_DEFECTS
   *  DR 2344 quoted()'s interaction with padding is unclear
   */
  template<typename _CharT, typename _Traits>
    std::basic_ostream<_CharT, _Traits>&
    operator<<(std::basic_ostream<_CharT, _Traits>& __os,
   const _Quoted_string<const _CharT*, _CharT>& __str)
    {
std::basic_ostringstream<_CharT, _Traits> __ostr;
__ostr << __str._M_delim;
for (const _CharT* __c = __str._M_string; *__c; ++__c)
  {
    if (*__c == __str._M_delim || *__c == __str._M_escape)
      __ostr << __str._M_escape;
    __ostr << *__c;
  }
__ostr << __str._M_delim;

return __os << __ostr.str();
    }

  /**
   * @brief Inserter for quoted strings.
   *
   *  _GLIBCXX_RESOLVE_LIB_DEFECTS
   *  DR 2344 quoted()'s interaction with padding is unclear
   */
  template<typename _CharT, typename _Traits, typename _String>
    std::basic_ostream<_CharT, _Traits>&
    operator<<(std::basic_ostream<_CharT, _Traits>& __os,
   const _Quoted_string<_String, _CharT>& __str)
    {
std::basic_ostringstream<_CharT, _Traits> __ostr;
__ostr << __str._M_delim;
for (auto& __c : __str._M_string)
  {
    if (__c == __str._M_delim || __c == __str._M_escape)
      __ostr << __str._M_escape;
    __ostr << __c;
  }
__ostr << __str._M_delim;

return __os << __ostr.str();
    }

  /**
   * @brief Extractor for delimited strings.
   *        The left and right delimiters can be different.
   */
  template<typename _CharT, typename _Traits, typename _Alloc>
    std::basic_istream<_CharT, _Traits>&
    operator>>(std::basic_istream<_CharT, _Traits>& __is,
   const _Quoted_string<basic_string<_CharT, _Traits, _Alloc>&,
            _CharT>& __str)
    {
_CharT __c;
__is >> __c;
if (!__is.good())
  return __is;
if (__c != __str._M_delim)
  {
    __is.unget();
    __is >> __str._M_string;
    return __is;
  }
__str._M_string.clear();
std::ios_base::fmtflags __flags
  = __is.flags(__is.flags() & ~std::ios_base::skipws);
do
  {
    __is >> __c;
    if (!__is.good())
      break;
    if (__c == __str._M_escape)
      {
  __is >> __c;
  if (!__is.good())
    break;
      }
    else if (__c == __str._M_delim)
      break;
    __str._M_string += __c;
  }
while (true);
__is.setf(__flags);

return __is;
    }

_GLIBCXX_END_NAMESPACE_VERSION
} // namespace __detail

template<typename _CharT>
  inline auto
  quoted(const _CharT* __string,
   _CharT __delim = _CharT('"'), _CharT __escape = _CharT('\\'))
  {
    return __detail::_Quoted_string<const _CharT*, _CharT>(__string, __delim,
                 __escape);
  }

template<typename _CharT, typename _Traits, typename _Alloc>
  inline auto
  quoted(const basic_string<_CharT, _Traits, _Alloc>& __string,
   _CharT __delim = _CharT('"'), _CharT __escape = _CharT('\\'))
  {
    return __detail::_Quoted_string<
    const basic_string<_CharT, _Traits, _Alloc>&, _CharT>(
      __string, __delim, __escape);
  }

template<typename _CharT, typename _Traits, typename _Alloc>
  inline auto
  quoted(basic_string<_CharT, _Traits, _Alloc>& __string,
   _CharT __delim = _CharT('"'), _CharT __escape = _CharT('\\'))
  {
    return __detail::_Quoted_string<
    basic_string<_CharT, _Traits, _Alloc>&, _CharT>(
      __string, __delim, __escape);
  }
}
#endif
