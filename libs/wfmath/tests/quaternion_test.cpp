// quaternion_test.cpp (Quaternion test functions)
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
// Created: 2002-1-20

#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef DEBUG
#define DEBUG
#endif

#include "wfmath/const.h"
#include "wfmath/vector.h"
#include "wfmath/rotmatrix_funcs.h"
#include "wfmath/quaternion.h"
#include "wfmath/stream.h"
#include <vector>

#include "general_test.h"

#include <cmath>

using namespace WFMath;

void test_quaternion(const Quaternion& q)
{
  std::cout << "Testing " << q << std::endl;

  test_general(q);

  Quaternion q2;

  q2.identity();

  std::cout << "q = " << q << std::endl;
  std::cout << "q2 = " << q2 << std::endl;
  std::cout << "q  * q2= " << q * q2 << std::endl;
  std::cout << "q2 * q = " << q2 * q << std::endl;


  REQUIRE(q == q * q2);
  REQUIRE(q == q2 * q);
  REQUIRE(q == q / q2);

  q2 /= q;

  std::cout << "q = " << q << std::endl;
  std::cout << "q2 = " << q2 << std::endl;

  REQUIRE(q * q2 == Quaternion().identity());

  q2 *= q;

  std::cout << "q2 = " << q2 << std::endl;

  REQUIRE(q2 == Quaternion().identity());

  Quaternion qi((Quaternion::Identity()));

  std::cout << "qi = " << qi << std::endl;

  REQUIRE(qi == Quaternion().identity());

  Quaternion q3(0, 5.0f/12);

  RotMatrix<3> m(q);

  // Check orthogonality of created matrix

  for(int i = 0; i < 3; ++i) {
    for(int j = i; j < 3; ++j) {
      CoordType dot_sum = 0;
      for(int k = 0; k < 3; ++k)
        dot_sum += m.elem(i, k) * m.elem(j, k);
//      std::cout << '(' << i << ',' << j << ") dot_sum == " << dot_sum << std::endl;
      REQUIRE(Equal(dot_sum, (i == j) ? 1 : 0));
    }
  }

  Quaternion q4;

  REQUIRE(q4.fromRotMatrix(m));

//  std::cout << m << std::endl << q4 << std::endl;

  // Converting via a rotation matrix may flip quaternion sign.
  CoordType dot = q4.scalar() * q.scalar() + Dot(q4.vector(), q.vector());
  REQUIRE(Equal(std::fabs(dot), 1));

  Vector<3> v(1, 2, 3), v2(v);

  v.rotate(q);
  v.rotate(q2 / q);

  REQUIRE(v2 == v);

//  std::cout << v << std::endl << v2 << std::endl;

  v.rotate(q);
  v2 = ProdInv(v2, m);

//  std::cout << v << std::endl << v2 << std::endl;

  REQUIRE(v == v2);

  CoordType s(q.scalar());

  REQUIRE(Equal(s * s + q.vector().sqrMag(), 1));

  Quaternion q_other(1, 2, 3, 4);

  v.rotate(q).rotate(q_other);
  v2.rotate(q * q_other);

  REQUIRE(v == v2);

  //Check that an invalid quaternion isn't equal to a valid quaternion
  Quaternion invalid_1;
  Quaternion invalid_2;
  REQUIRE(invalid_1 != Quaternion::Identity());

  //Two invalid points are never equal
  REQUIRE(invalid_1 != invalid_2);

  const Quaternion& identity = Quaternion::IDENTITY();
  REQUIRE(identity.isValid());
  REQUIRE(identity.scalar() == 1.0f);
  REQUIRE(identity.vector() == WFMath::Vector<3>::ZERO());
}

TEST_CASE("quaternion_test")
{
  Quaternion q(Vector<3>(1, 3, -std::sqrt(0.7f)), .3f);

  test_quaternion(q);

  // Edge case: rotation of 180 degrees around the x-axis
  Quaternion q180(Vector<3>(1, 0, 0), numeric_constants<CoordType>::pi());
  test_quaternion(q180);

  }
