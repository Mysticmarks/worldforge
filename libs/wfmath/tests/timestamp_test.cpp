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
#include <chrono>
#include <sstream>
#include <catch2/catch_test_macros.hpp>

namespace Catch {
template<>
struct StringMaker<WFMath::TimeStamp>
{
    static std::string convert(const WFMath::TimeStamp& ts)
    {
        if (!ts.isValid()) {
            return "TimeStamp(invalid)";
        }

        const auto diff = ts - WFMath::TimeStamp::epochStart();
        const auto [sec, usec] = diff.full_time();
        std::ostringstream oss;
        oss << "TimeStamp(" << sec << "s," << usec << "us)";
        return oss.str();
    }
};
}

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

    SECTION("Now uses the Unix epoch on all platforms")
    {
        using namespace std::chrono;
        const auto before = time_point_cast<microseconds>(system_clock::now()).time_since_epoch();
        const TimeStamp now = TimeStamp::now();
        const auto after = time_point_cast<microseconds>(system_clock::now()).time_since_epoch();

        REQUIRE(now.isValid());

        const auto since_epoch = now - TimeStamp::epochStart();
        const auto [sec, usec] = since_epoch.full_time();
        const auto wf_now = seconds(sec) + microseconds(usec);

        const auto lower_bound = before - seconds(5);
        const auto upper_bound = after + seconds(5);

        CHECK(wf_now >= lower_bound);
        CHECK(wf_now <= upper_bound);
    }
}
