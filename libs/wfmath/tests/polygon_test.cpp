// polygon_test.cpp (Polygon<> test functions)
//
//  The WorldForge Project
//  Copyright (C) 2002  The WorldForge Project
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
// Created: 2002-1-20

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
#include "wfmath/polygon.h"
#include "wfmath/polygon_intersect.h"
#include "wfmath/stream.h"
#include <vector>

#include "general_test.h"
#include "shape_test.h"

using namespace WFMath;

template<int dim>
void test_polygon(const Polygon<dim>& p)
{
  std::cout << "Testing " << p << std::endl;

  test_general(p);
  test_shape(p);

  // Just check that these compile
  Point<dim> point;
  point.setToOrigin();
  AxisBox<dim> a(point, point, true);
  Vector<dim> vec;
  vec.zero();
  RotMatrix<dim> mat;
  mat.identity();
  RotBox<dim> r(point, vec, mat);

  Intersect(p, a, false);
  Intersect(p, r, false);
}

/**
 * Test intersection between a square polygon and axis boxes at each of its four sides.
 */
void test_intersect()
{

  Polygon<2> p;

  p.addCorner(0, Point<2>(0, 0));
  p.addCorner(0, Point<2>(4, 0));
  p.addCorner(0, Point<2>(4, -4));
  p.addCorner(0, Point<2>(0, -4));
  p.isValid();

  AxisBox<2> a1(Point<2>(-1, -1), Point<2>(1, -3));
  std::cout << "Testing intersection of " << p << " and " << a1 << std::endl;
  assert(Intersect(p, a1, false));

  AxisBox<2> a2(Point<2>(1, -5), Point<2>(2, -3));
  std::cout << "Testing intersection of " << p << " and " << a2 << std::endl;
  assert(Intersect(p, a2, false));

  AxisBox<2> a3(Point<2>(5, -1), Point<2>(3, -3));
  std::cout << "Testing intersection of " << p << " and " << a3 << std::endl;
  assert(Intersect(p, a3, false));

  AxisBox<2> a4(Point<2>(1, 1), Point<2>(2, -1));
  std::cout << "Testing intersection of " << p << " and " << a4 << std::endl;
  assert(Intersect(p, a4, false));


  RotBox<2> r1(Point<2>(-1, -1), Vector<2>(2, -2), RotMatrix<2>().identity());
  std::cout << "Testing intersection of " << p << " and " << r1 << std::endl;
  assert(Intersect(p, r1, false));

  RotBox<2> r2(Point<2>(1, -5), Vector<2>(1, 2), RotMatrix<2>().identity());
  std::cout << "Testing intersection of " << p << " and " << r2 << std::endl;
  assert(Intersect(p, r2, false));

  RotBox<2> r3(Point<2>(5, -1), Vector<2>(-2, -2), RotMatrix<2>().identity());
  std::cout << "Testing intersection of " << p << " and " << r3 << std::endl;
  assert(Intersect(p, r3, false));

  RotBox<2> r4(Point<2>(1, 1), Vector<2>(1, -2), RotMatrix<2>().identity());
  std::cout << "Testing intersection of " << p << " and " << r4 << std::endl;
  assert(Intersect(p, r4, false));


}

/**
 * Test contains between a square polygon and axis boxes at each of its four sides.
 */
void test_contains()
{

  Polygon<2> p;

  p.addCorner(0, Point<2>(0, 0));
  p.addCorner(0, Point<2>(4, 0));
  p.addCorner(0, Point<2>(4, -4));
  p.addCorner(0, Point<2>(0, -4));
  p.isValid();

  AxisBox<2> a1(Point<2>(0.1f, -3.9f), Point<2>(0.2f, -3.8f));
  std::cout << "Testing " << p << " contains " << a1 << std::endl;
  assert(Contains(p, a1, false));

  AxisBox<2> a2(Point<2>(3.8f, -3.9f), Point<2>(3.9f, -3.8f));
  std::cout << "Testing " << p << " contains " << a2 << std::endl;
  assert(Contains(p, a2, false));

  AxisBox<2> a3(Point<2>(0.1f, -0.2f), Point<2>(0.2f, -0.1f));
  std::cout << "Testing " << p << " contains " << a3 << std::endl;
  assert(Contains(p, a3, false));

  AxisBox<2> a4(Point<2>(3.8f, -0.2f), Point<2>(3.9f, -0.1f));
  std::cout << "Testing " << p << " contains " << a4 << std::endl;
  assert(Contains(p, a4, false));


  RotBox<2> r1(Point<2>(0.1f, -3.9f), Vector<2>(0.1f, 0.1f), RotMatrix<2>().identity());
  std::cout << "Testing " << p << " contains " << r1 << std::endl;
  assert(Contains(p, r1, false));

  RotBox<2> r2(Point<2>(3.8f, -3.9f), Vector<2>(0.1f, 0.1f), RotMatrix<2>().identity());
  std::cout << "Testing " << p << " contains " << r2 << std::endl;
  assert(Contains(p, r2, false));

  RotBox<2> r3(Point<2>(0.1f, -0.2f), Vector<2>(0.1f, 0.1f), RotMatrix<2>().identity());
  std::cout << "Testing " << p << " contains " << r3 << std::endl;
  assert(Contains(p, r3, false));

  RotBox<2> r4(Point<2>(3.8f, -0.2f), Vector<2>(0.1f, 0.1f), RotMatrix<2>().identity());
  std::cout << "Testing " << p << " contains " << r4 << std::endl;
  assert(Contains(p, r4, false));

}

/**
 * Test behavior with a concave polygon.
 */
void test_concave_polygon()
{
  Polygon<2> p;

  p.addCorner(0, Point<2>(0, 0));
  p.addCorner(1, Point<2>(2, 0));
  p.addCorner(2, Point<2>(2, 2));
  p.addCorner(3, Point<2>(1, 2));
  p.addCorner(4, Point<2>(1, 1));
  p.addCorner(5, Point<2>(0, 1));
  p.isValid();

  assert(Intersect(p, Point<2>(0.5, 0.5), false));
  assert(!Intersect(p, Point<2>(1.5, 1.5), false));
}

/**
 * Test behavior with a self-intersecting polygon.
 */
void test_self_intersection()
{
  Polygon<2> p;

  p.addCorner(0, Point<2>(0, 0));
  p.addCorner(1, Point<2>(2, 2));
  p.addCorner(2, Point<2>(0, 2));
  p.addCorner(3, Point<2>(2, 0));
  p.isValid();

  assert(Intersect(p, Point<2>(0.5, 0.5), false));
  assert(Intersect(p, Point<2>(1.5, 1.5), false));
}

/**
 * Test robustness against round-off errors on nearly parallel edges.
 */
void test_roundoff()
{
  Polygon<2> near_vertical;
  near_vertical.addCorner(0, Point<2>(0, 0));
  near_vertical.addCorner(1, Point<2>(1e-9, 1));
  near_vertical.addCorner(2, Point<2>(1, 1));
  near_vertical.addCorner(3, Point<2>(1, 0));
  near_vertical.isValid();

  AxisBox<2> box_v(Point<2>(5e-10, 0.5), Point<2>(0.6, 0.6));
  assert(Intersect(near_vertical, box_v, false));
  assert(Intersect(near_vertical, Point<2>(5e-10, 0.5), false));
  AxisBox<2> box_v_out(Point<2>(-1e-9, 0.5), Point<2>(-5e-10, 0.6));
  assert(!Intersect(near_vertical, box_v_out, false));
  assert(!Intersect(near_vertical, Point<2>(-5e-10, 0.5), false));

  Polygon<2> near_horizontal;
  near_horizontal.addCorner(0, Point<2>(0, 0));
  near_horizontal.addCorner(1, Point<2>(1, 1e-9));
  near_horizontal.addCorner(2, Point<2>(1, 1));
  near_horizontal.addCorner(3, Point<2>(0, 1));
  near_horizontal.isValid();

  AxisBox<2> box_h(Point<2>(0.5, 5e-10), Point<2>(0.6, 0.6));
  assert(Intersect(near_horizontal, box_h, false));
  assert(Intersect(near_horizontal, Point<2>(0.5, 5e-10), false));
  AxisBox<2> box_h_out(Point<2>(0.5, -1e-9), Point<2>(0.6, -5e-10));
  assert(!Intersect(near_horizontal, box_h_out, false));
  assert(!Intersect(near_horizontal, Point<2>(0.5, -5e-10), false));
}

/**
 * Test rotation and translation of polygons.
 */
void test_transformations()
{
  Polygon<2> p;

  p.addCorner(0, Point<2>(0, 0));
  p.addCorner(1, Point<2>(2, 0));
  p.addCorner(2, Point<2>(2, 2));
  p.addCorner(3, Point<2>(0, 2));
  p.isValid();

  Polygon<2> shifted = p;
  shifted.shift(Vector<2>(3, 4));
  assert(Intersect(shifted, Point<2>(3.5, 4.5), false));
  assert(!Intersect(shifted, Point<2>(0.5, 0.5), false));

  Polygon<2> rotated = p;
  RotMatrix<2> rot;
  rot.rotation(0, 1, numeric_constants<CoordType>::pi() / 2);
  rotated.rotatePoint(rot, Point<2>(0, 0));
  assert(Intersect(rotated, Point<2>(-0.5, 0.5), false));
  assert(!Intersect(rotated, Point<2>(0.5, 0.5), false));
}

/**
 * Test behavior of an outer polygon with an inner "hole" polygon.
 */
void test_polygon_with_hole()
{
  Polygon<2> outer;
  outer.addCorner(0, Point<2>(0, 0));
  outer.addCorner(1, Point<2>(5, 0));
  outer.addCorner(2, Point<2>(5, -5));
  outer.addCorner(3, Point<2>(0, -5));
  outer.isValid();

  Polygon<2> hole;
  hole.addCorner(0, Point<2>(1, -1));
  hole.addCorner(1, Point<2>(4, -1));
  hole.addCorner(2, Point<2>(4, -4));
  hole.addCorner(3, Point<2>(1, -4));
  hole.isValid();

  assert(Contains(outer, hole, false));

  Point<2> outsideHole(0.5f, -0.5f);
  assert(Intersect(outer, outsideHole, false));
  assert(!Intersect(hole, outsideHole, false));

  Point<2> insideHole(2.f, -2.f);
  assert(Intersect(outer, insideHole, false));
  assert(Intersect(hole, insideHole, false));

  bool compositeContains = Intersect(outer, insideHole, false) && !Intersect(hole, insideHole, false);
  assert(!compositeContains);
}

int main()
{
  bool succ;

  Polygon<2> p2;

  succ = p2.addCorner(0, Point<2>(1, -1));
  assert(succ);
  succ = p2.addCorner(0, Point<2>(2, -1));
  assert(succ);
  succ = p2.addCorner(0, Point<2>(1, -3));
  assert(succ);

  test_polygon(p2);

  Polygon<3> p3;

  succ = p3.addCorner(0, Point<3>(1, -1, 5));
  assert(succ);
  succ = p3.addCorner(0, Point<3>(2, -1, std::sqrt(3.0f/2)));
  assert(succ);
  succ = p3.addCorner(0, Point<3>(1, -3, 2.0f/3));
  assert(succ);

  test_polygon(p3);

  test_intersect();

  test_contains();

  test_concave_polygon();
  test_self_intersection();
  test_roundoff();
  test_transformations();
  test_polygon_with_hole();

  return 0;
}
