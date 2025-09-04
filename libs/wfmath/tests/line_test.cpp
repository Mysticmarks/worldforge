// line_test.cpp (Line<> test functions)
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
// Created: 2012-01-26

#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef DEBUG
#define DEBUG
#endif

#include "wfmath/const.h"
#include "wfmath/vector.h"
#include "wfmath/point.h"
#include "wfmath/axisbox.h"
#include "wfmath/ball.h"
#include "wfmath/segment.h"
#include "wfmath/rotmatrix.h"
#include "wfmath/intersect.h"
#include "wfmath/stream.h"
#include "wfmath/line.h"

#include "general_test.h"
#include "shape_test.h"

#include <vector>
#include <list>
#include <catch2/catch_test_macros.hpp>

using namespace WFMath;

template<int dim>
void test_line(const Line<dim>& p)
{
  std::cout << "Testing line: " << p << std::endl;

  test_general(p);
  test_shape(p);

  REQUIRE(p == p);

  // Intersection and distance tests operate on the first segment when available
  if (p.numCorners() >= 2) {
    Segment<dim> seg(p.getCorner(0), p.getCorner(1));

    // The segment should intersect its own bounding box
    AxisBox<dim> box = seg.boundingBox();
    REQUIRE(Intersect(seg, box, false));

    // A distant box should not intersect
    Point<dim> far_low, far_high;
    far_low.setToOrigin();
    far_high.setToOrigin();
    for (int i = 0; i < dim; ++i) {
      far_low[i] = 100;
      far_high[i] = 101;
    }
    AxisBox<dim> far_box(far_low, far_high);
    REQUIRE(!Intersect(seg, far_box, false));

    // Boundary condition: touching at endpoint counts as intersection when not proper
    AxisBox<dim> end_box(seg.endpoint(0), seg.endpoint(0));
    REQUIRE(Intersect(seg, end_box, false));
    REQUIRE(!Intersect(seg, end_box, true));

    // Distance checks
    CoordType dist = Distance(seg.endpoint(0), seg.endpoint(1));
    REQUIRE(dist >= 0);

    Segment<dim> zero(seg.endpoint(0), seg.endpoint(0));
    REQUIRE(Distance(zero.endpoint(0), zero.endpoint(1)) == 0);
  }

  // Orientation tests using rotation about the first corner in 2D
  if (dim == 2 && p.numCorners() >= 2) {
    Line<dim> rotated = p;
    RotMatrix<dim> m;
    m.rotation(0, 1, numeric_constants<CoordType>::pi() / 2);
    rotated.rotateCorner(m, 0);

    Vector<dim> orig = p.getCorner(1) - p.getCorner(0);
    Vector<dim> expect;
    expect.zero();
    expect[0] = -orig[1];
    expect[1] = orig[0];
    Vector<dim> after = rotated.getCorner(1) - rotated.getCorner(0);
    REQUIRE(Equal(after, expect));
  }
}

void test_modify()
{
  Line<2> line;
  line.addCorner(0, Point<2>(2, 2));
  line.addCorner(0, Point<2>(0, 0));
  
  REQUIRE(line.getCorner(0) == Point<2>(0, 0));

  line.moveCorner(0, Point<2>(1, 1));
  REQUIRE(line.getCorner(0) == Point<2>(1, 1));
}

TEST_CASE("line_test")
{
  Line<2> line2_1;
  REQUIRE(!line2_1.isValid());
  line2_1.addCorner(0, Point<2>(0, 0));
  line2_1.addCorner(0, Point<2>(4, 0));
  line2_1.addCorner(0, Point<2>(4, -4));
  line2_1.addCorner(0, Point<2>(0, -4));
  REQUIRE(line2_1.isValid());

  test_line(line2_1);

  REQUIRE(line2_1 == line2_1);

  Line<2> line2_2;
  REQUIRE(!line2_2.isValid());
  line2_2.addCorner(0, Point<2>(0, 0));
  line2_2.addCorner(0, Point<2>(4, -4));
  line2_2.addCorner(0, Point<2>(4, 0));
  line2_2.addCorner(0, Point<2>(0, -4));
  REQUIRE(line2_2.isValid());

  // Check in-equality
  REQUIRE(line2_1 != line2_2);

  // Check assignment
  line2_2 = line2_1;

  // Check equality
  REQUIRE(line2_1 == line2_2);

  Line<2> line2_3;
  REQUIRE(!line2_3.isValid());
  REQUIRE(line2_3 != line2_1);
  line2_3 = line2_1;
  REQUIRE(line2_3 == line2_1);

  // Check bounding box calculation
  REQUIRE(Equal(line2_1.boundingBox(),
               AxisBox<2>(Point<2>(0, -4), Point<2>(4, 0))));

  Line<2> line2_4(line2_1);

  REQUIRE(line2_1 == line2_4);

  Line<3> line3;
  line3.addCorner(0, Point<3>(0, 0, 0));
  line3.addCorner(0, Point<3>(1, 0, 0));
  REQUIRE(line3.isValid());

  test_line(line3);

  test_modify();

  }
