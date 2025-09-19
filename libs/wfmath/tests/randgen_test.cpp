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
#include <sstream>
#include <string>

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

TEST_CASE("randgen serialization roundtrip preserves state")
{
    MTRand rng(42);
    (void)rng.randInt();

    std::ostringstream out;
    rng.save(out);

    MTRand restored(static_cast<MTRand::uint32>(0));
    std::istringstream in(out.str());
    restored.load(in);
    REQUIRE_FALSE(in.fail());

    MTRand reference = rng;
    for (int i = 0; i < 5; ++i) {
        REQUIRE(restored.randInt() == reference.randInt());
    }
}

TEST_CASE("randgen load rejects invalid index")
{
    MTRand rng(99);
    std::ostringstream out;
    rng.save(out);

    const std::string serialized = out.str();
    const auto last_tab = serialized.find_last_of('\t');
    REQUIRE(last_tab != std::string::npos);
    std::string invalid_serialized = serialized.substr(0, last_tab + 1);
    invalid_serialized += std::to_string(MTRand::state_size);

    std::istringstream in(invalid_serialized);
    MTRand target(static_cast<MTRand::uint32>(1234));
    MTRand reference = target;

    target.load(in);
    REQUIRE(in.fail());

    for (int i = 0; i < 5; ++i) {
        REQUIRE(target.randInt() == reference.randInt());
    }
}
