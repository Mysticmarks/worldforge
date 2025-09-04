#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef DEBUG
#define DEBUG
#endif


// timestamp_test.cpp - TimeStamp related tests

#include <wfmath/timestamp.h>
#include <stdlib.h>
#include <iostream>
#include <catch2/catch_test_macros.hpp>

using namespace WFMath;

TEST_CASE("timestamp_test")
{
    TimeStamp tsa;
    CHECK_FALSE(tsa.isValid());

    tsa += TimeDiff(50);
    CHECK_FALSE(tsa.isValid());

    tsa = TimeStamp::now();
    CHECK(tsa.isValid());

    tsa += TimeDiff();
    CHECK_FALSE(tsa.isValid());

    TimeDiff tda(1000);
    CHECK(tda.isValid());

    tsa = TimeStamp::now();
    TimeStamp tsb = tsa;
    CHECK(tsa == tsb);

    tsb += tda;
    CHECK(tsa < tsb);
}
