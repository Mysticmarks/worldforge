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

    SECTION("Normalizes negative durations")
    {
        TimeDiff negative(-1500);
        auto [sec, usec] = negative.full_time();
        CHECK(sec == -2);
        CHECK(usec == 500000);

        TimeDiff delta = TimeDiff(500) - TimeDiff(1200);
        auto [delta_sec, delta_usec] = delta.full_time();
        CHECK(delta_sec == -1);
        CHECK(delta_usec == 300000);

        TimeStamp ts = TimeStamp::epochStart();
        ts += TimeDiff(1000);
        ts -= TimeDiff(2500);
        auto diff = ts - TimeStamp::epochStart();
        auto [stamp_sec, stamp_usec] = diff.full_time();
        CHECK(stamp_sec == -2);
        CHECK(stamp_usec == 500000);
    }
}
