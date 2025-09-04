// general_test.h (generic class interface test functions)
//
//  The WorldForge Project
//  Copyright (C) 2001  The WorldForge Project
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
//  For information about WorldForge and its authors, please contact
//  the Worldforge Web Site at http://www.worldforge.org.

// Author: Ron Steinke
// Created: 2001-1-6

#ifndef WFMATH_GENERAL_TEST_H
#define WFMATH_GENERAL_TEST_H

#include "wfmath/const.h"
#include "wfmath/stream.h"
#include <string>
#include <iostream>

#include <cstdlib>
#include <catch2/catch_test_macros.hpp>

namespace WFMath {

template<class C>
void test_general(const C& c)
{
  C c1, c2(c); // Generic and copy constructors

  c1 = c; // operator=()

  REQUIRE(Equal(c1, c2));
  REQUIRE(c1 == c2);
  REQUIRE(!(c1 != c2));

  std::string s = ToString(c); // Uses operator<<() implicitly
  C c3;
  try {
    FromString(c3, s); // Uses operator>>() implicitly
  }
  catch(const ParseError&) {
    std::cerr << "Couldn't parse generated string: " << s << std::endl;
    abort();
  }

  // We lose precision in string conversion
  REQUIRE(Equal(c3, c, FloatMax(numeric_constants<CoordType>::epsilon(), 1e-5F)));
}

} // namespace WFMath

#endif // WFMATH_GENERAL_TEST_H
