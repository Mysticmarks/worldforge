// point_test.cpp (Point<> test functions)
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
#include "wfmath/point.h"
#include "wfmath/axisbox.h"
#include "wfmath/ball.h"
#include "wfmath/rotmatrix.h"
#include "wfmath/stream.h"

#include "general_test.h"
#include "shape_test.h"

#include <vector>
#include <list>

using namespace WFMath;

template<int dim>
void test_point(const Point<dim>& p)
{
  std::cout << "Testing point: " << p << std::endl;

  test_general(p);
  test_shape(p);

  std::vector<Point<dim> > pvec;
  std::list<CoordType> clist;

  REQUIRE(!Barycenter(pvec).isValid());

  REQUIRE(!Barycenter(pvec, clist).isValid());

  pvec.push_back(p);
  REQUIRE(p == Barycenter(pvec));
  clist.push_back(5);
  REQUIRE(p == Barycenter(pvec, clist));

  // Barycenter fails if sum of weights is 0
  pvec.push_back(p);
  REQUIRE(Barycenter(pvec).isValid());
  clist.push_back(-5);
  REQUIRE(!Barycenter(pvec, clist).isValid());

  REQUIRE(p == p + (p - p));

  //Check that an invalid point isn't equal to a valid point, even if the values are equal
  Point<dim> invalid_point_1;
  Point<dim> invalid_point_2;
  for (size_t i = 0; i < dim; ++i) {
      invalid_point_1[i] = 0.0;
      invalid_point_2[i] = 0.0;
  }
  REQUIRE(invalid_point_1 != Point<dim>::ZERO());

  //Two invalid points are never equal
  REQUIRE(invalid_point_1 != invalid_point_2);

  Vector<dim> zero_vec;
  zero_vec.zero();
  Point<dim> shifted = p;
  shifted += zero_vec;
  REQUIRE(shifted.isEqualTo(p));

  if constexpr (dim >= 2) {
    RotMatrix<dim> full_rot;
    full_rot.rotation(0, 1, numeric_constants<CoordType>::pi() * 2);
    Point<dim> rotated = p;
    rotated.rotate(full_rot, Point<dim>::ZERO());
    REQUIRE(rotated.isEqualTo(p));
  }
}

TEST_CASE("point_test")
{
	Point<2> p{};
	REQUIRE(!p.isValid());

  test_point(Point<2>(1, -1));
  test_point(Point<3>(1, -1, numeric_constants<CoordType>::sqrt2()));

  Point<2> zero2 = Point<2>::ZERO();
  REQUIRE(zero2.x() == 0 && zero2.y() == 0);
  Point<3> zero3 = Point<3>::ZERO();
  REQUIRE(zero3.x() == 0 && zero3.y() == 0 && zero3.z() == 0);

    REQUIRE(Point<3>(1,2,3).isEqualTo(Point<3>(1,2,3)));
    REQUIRE(!Point<3>(1,2,3).isEqualTo(Point<3>(3,2,1)));
    REQUIRE(!Point<3>(1,2,3).isEqualTo(Point<3>()));
    REQUIRE(!Point<3>().isEqualTo(Point<3>(3,2,1)));
    REQUIRE(!Point<3>().isEqualTo(Point<3>())); //invalid points are never equal

  // Verify rotateInverse undoes rotate
  RotMatrix<3> rot;
  rot.rotation(0, 1, numeric_constants<CoordType>::pi() / 4);
  Point<3> orig(1,2,3);
  Point<3> rotated = orig;
  rotated.rotate(rot, Point<3>::ZERO());
  rotated.rotateInverse(rot, Point<3>::ZERO());
  REQUIRE(rotated.isEqualTo(orig));

  }
