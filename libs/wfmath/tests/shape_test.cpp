// shape_test.cpp (basic shape test functions)
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

#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef DEBUG
#define DEBUG
#endif

#include "wfmath/const.h"
#include "wfmath/vector.h"
#include "wfmath/rotmatrix.h"
#include "wfmath/point.h"
#include "wfmath/axisbox.h"
#include "wfmath/ball.h"
#include "wfmath/segment.h"
#include "wfmath/rotbox.h"
#include "wfmath/intersect.h"
#include "wfmath/stream.h"
#include <vector>

#include "general_test.h"
#include "shape_test.h"

#include <cmath>

using namespace WFMath;

template<int dim>
void test_shape(const Point<dim>& p1, const Point<dim>& p2)
{
  CoordType sqr_dist = SquaredDistance(p1, p2);

  AxisBox<dim> box(p1, p2), tmp;

  std::cout << "Testing " << box << std::endl;

  test_general(box);
  test_shape_no_rotate(box);

  tmp = Union(box, box);
  REQUIRE(tmp == box);
  REQUIRE(Intersection(box, box, tmp));
  REQUIRE(tmp == box);
  std::vector<AxisBox<dim> > boxvec;
  boxvec.push_back(box);
  REQUIRE(box == BoundingBox(boxvec));

  REQUIRE(Intersect(box, p1, false));
  REQUIRE(!Intersect(box, p1, true));

  REQUIRE(Intersect(box, box, false));
  REQUIRE(Intersect(box, box, true));
  REQUIRE(Contains(box, box, false));
  REQUIRE(!Contains(box, box, true));

  Ball<dim> ball(p1, 1);

  std::cout << "Testing " << ball << std::endl;

  test_general(ball);
  test_shape(ball);

  REQUIRE(Intersect(ball, p1, false));
  REQUIRE(Intersect(ball, p1, true));

  REQUIRE(Intersect(ball, box, false));
  REQUIRE(Intersect(ball, box, true));
  REQUIRE(Contains(ball, box, false) == (sqr_dist <= 1));
  REQUIRE(Contains(ball, box, true) == (sqr_dist < 1));
  REQUIRE(!Contains(box, ball, false));
  REQUIRE(!Contains(box, ball, true));

  REQUIRE(Intersect(ball, ball, false));
  REQUIRE(Intersect(ball, ball, true));
  REQUIRE(Contains(ball, ball, false));
  REQUIRE(!Contains(ball, ball, true));

  Segment<dim> seg(p1, p2);

  std::cout << "Testing " << seg << std::endl;

  test_general(seg);
  test_shape(seg);

  REQUIRE(Intersect(seg, p1, false));
  REQUIRE(!Intersect(seg, p1, true));

  REQUIRE(Intersect(seg, box, false));
  REQUIRE(Intersect(seg, box, true));
  REQUIRE(!Contains(seg, box, false));
  REQUIRE(!Contains(seg, box, true));
  REQUIRE(Contains(box, seg, false));
  REQUIRE(!Contains(box, seg, true));

  REQUIRE(Intersect(seg, ball, false));
  REQUIRE(Intersect(seg, ball, true));
  REQUIRE(!Contains(seg, ball, false));
  REQUIRE(!Contains(seg, ball, true));
  REQUIRE(Contains(ball, seg, false) == (sqr_dist <= 1));
  REQUIRE(Contains(ball, seg, true) == (sqr_dist < 1));

  REQUIRE(Intersect(seg, seg, false));
  REQUIRE(Intersect(seg, seg, true));
  REQUIRE(Contains(seg, seg, false));
  REQUIRE(!Contains(seg, seg, true));

  RotBox<dim> rbox(
      p1,
      p2 - p1,
      RotMatrix<dim>().rotation(0, 1, numeric_constants<CoordType>::pi() / 6));

  std::cout << "Testing " << rbox << std::endl;

  test_general(rbox);
  test_shape(rbox);

  REQUIRE(Intersect(rbox, p1, false));
  REQUIRE(!Intersect(rbox, p1, true));

  REQUIRE(Intersect(rbox, box, false));
  REQUIRE(Intersect(rbox, box, true));
  REQUIRE(!Contains(rbox, box, false));
  REQUIRE(!Contains(rbox, box, true));
  REQUIRE(!Contains(box, rbox, false));
  REQUIRE(!Contains(box, rbox, true));

  REQUIRE(Intersect(rbox, ball, false));
  REQUIRE(Intersect(rbox, ball, true));
  REQUIRE(!Contains(rbox, ball, false));
  REQUIRE(!Contains(rbox, ball, true));
  REQUIRE(Contains(ball, rbox, false) == (sqr_dist <= 1));
  REQUIRE(Contains(ball, rbox, true) == (sqr_dist < 1));

  REQUIRE(Intersect(rbox, seg, false));
  // The next function may either succeed or fail, depending on the points passed.
  Intersect(rbox, seg, true);
  REQUIRE(!Contains(rbox, seg, false));
  REQUIRE(!Contains(rbox, seg, true));
  REQUIRE(!Contains(seg, rbox, false));
  REQUIRE(!Contains(seg, rbox, true));

  REQUIRE(Intersect(rbox, rbox, false));
  REQUIRE(Intersect(rbox, rbox, true));
  REQUIRE(Contains(rbox, rbox, false));
  REQUIRE(!Contains(rbox, rbox, true));

  // Rotated box with no rotation should behave like an axis-aligned box
  RotBox<dim> aligned(p1, p2 - p1, RotMatrix<dim>().identity());
  REQUIRE(Intersect(aligned, box, true));
  REQUIRE(Contains(aligned, box, false));
  REQUIRE(Contains(box, aligned, false));

  // Degenerate segment represented by a single point
  Segment<dim> degenerate(p1, p1);
  REQUIRE(Intersect(degenerate, p1, false));
  REQUIRE(Contains(box, degenerate, false));
  REQUIRE(Contains(ball, degenerate, false));
}

TEST_CASE("shape_test")
{
  test_shape(Point<2>(1, -1),
             Point<2>().setToOrigin());
  test_shape(Point<3>(1, -1, numeric_constants<CoordType>::sqrt2()),
             Point<3>().setToOrigin());

  }
