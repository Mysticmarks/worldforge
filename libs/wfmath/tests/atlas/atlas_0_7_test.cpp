// atlas_tests.cpp (WFMath/Atlas Message conversion test code)
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
// Created: 2001-12-12

#include <Atlas/Message/Element.h>
#include "wfmath/atlasconv.h"
#include "wfmath/stream.h"
#include <catch2/catch_test_macros.hpp>
using namespace WFMath;

template<class C>
void atlas_test(const C& c)
{
  std::cout << c << std::endl;
  Atlas::Message::Element a = c.toAtlas();
  C out(a);
  REQUIRE(out.isValid());
  std::cout << out << std::endl;
  // Only match to string precision
  REQUIRE(Equal(c, out, FloatMax(numeric_constants<CoordType>::epsilon(),1e-5)));
}

TEST_CASE("atlas_0_7_test")
{
  Point<3> p(1, 0, numeric_constants<CoordType>::sqrt2());
  atlas_test(p);

  Vector<3> v(1, -1, 4);
  atlas_test(v);

  Quaternion q(v, 0.7);
  atlas_test(q);

  AxisBox<3> b1(Point<3>().setToOrigin(), p), b2(p, p + v);
  atlas_test(b1);
  atlas_test(b2);
  
  Ball<3> ball(Point<3>(1, 2, 3), 10);
  atlas_test(ball);
  
  Polygon<2> poly;
  poly.addCorner(0, WFMath::Point<2>(0, 0));
  poly.addCorner(1, WFMath::Point<2>(10, 0));
  poly.addCorner(2, WFMath::Point<2>(10, 10));
  atlas_test(poly);
  
  Line<2> line;
  line.addCorner(0, WFMath::Point<2>(0, 0));
  line.addCorner(1, WFMath::Point<2>(10, 0));
  atlas_test(line);
  
  Line<3> line2;
  line2.addCorner(0, WFMath::Point<3>(0, 0, 0));
  line2.addCorner(1, WFMath::Point<3>(10, 0, 5));
  atlas_test(line2);
  
  RotMatrix<2> rot;
  rot.rotation(numeric_constants<CoordType>::pi() / 4);
  RotBox<2> rotbox(Point<2>(1, 2), Vector<2>(10, 20), rot);
  atlas_test(rotbox);

  }
