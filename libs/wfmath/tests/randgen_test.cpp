// randgen.cpp (time and random number implementation)
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

// Author: Alistair Riddoch
// Created: 2011-2-14

#include <catch2/catch_test_macros.hpp>
#include "wfmath/randgen.h"
#include <cstdio>
#include <iostream>

using WFMath::MTRand;

void test_known_sequence()
{
    static WFMath::MTRand::uint32 expected_results[] = {
      2221777491u,
      2873750246u,
      4067173416u,
      794519497u,
      3287624630u,
      3357287912u,
      1212880927u,
      2464917741u,
      949382604u,
      1898004827u
    };
    WFMath::MTRand rng;

    rng.seed(23);

    for (int i = 0; i < 10; ++i)
    {
        WFMath::MTRand::uint32 rnd = rng.randInt();
        REQUIRE(rnd == expected_results[i]);
    }
}

void test_generator_instances()
{
    MTRand one(23);

    printf("%.16f %.16f\n", one.rand(), one.rand());
    float oneres = static_cast<float>(one.rand());

    MTRand two(23);

    printf("%.16f %.16f\n", two.rand<float>(), two.rand<float>());
    float twores = two.rand<float>();

    printf("%.16f %.16f\n", oneres, twores);
    REQUIRE(oneres == twores);
}

TEST_CASE("randgen_test")
{
    test_known_sequence();
    test_generator_instances();
}
