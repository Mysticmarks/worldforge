// timestamp.cpp (time and random number implementation)
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
// Created: 2002-5-23

 #if defined _WIN32
#include <sys/timeb.h>
#else

#include <unistd.h>
#include <sys/time.h>

#endif

#include <chrono>
#include <cstdint>
#include "timestamp.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

static void regularize(std::int64_t& sec, std::int64_t& usec) {
using namespace std::chrono;
auto total = seconds(sec) + microseconds(usec);
sec = duration_cast<seconds>(total).count();
usec = duration_cast<microseconds>(total - seconds(sec)).count();
}

namespace WFMath {

TimeDiff::TimeDiff(std::int64_t sec, std::int64_t usec, bool is_valid)
    : m_isvalid(is_valid), m_sec(sec), m_usec(usec) {
    if (m_isvalid)
        regularize(m_sec, m_usec);
}

TimeDiff::TimeDiff(std::int64_t msec)
    : m_isvalid(true) {
    using namespace std::chrono;
    auto dur = milliseconds(msec);
    m_sec = duration_cast<seconds>(dur).count();
    m_usec = duration_cast<microseconds>(dur - seconds(m_sec)).count();
}

std::int64_t TimeDiff::milliseconds() const {
    using namespace std::chrono;
    return duration_cast<milliseconds>(seconds(m_sec) + microseconds(m_usec)).count();
}

TimeDiff& operator+=(TimeDiff& val, const TimeDiff& d) {
    using namespace std::chrono;
    auto total = seconds(val.m_sec) + microseconds(val.m_usec)
               + seconds(d.m_sec) + microseconds(d.m_usec);
    val.m_sec = duration_cast<seconds>(total).count();
    val.m_usec = duration_cast<microseconds>(total - seconds(val.m_sec)).count();
    val.m_isvalid = val.m_isvalid && d.m_isvalid;
    return val;
}

TimeDiff& operator-=(TimeDiff& val, const TimeDiff& d) {
    using namespace std::chrono;
    auto total = seconds(val.m_sec) + microseconds(val.m_usec)
               - (seconds(d.m_sec) + microseconds(d.m_usec));
    val.m_sec = duration_cast<seconds>(total).count();
    val.m_usec = duration_cast<microseconds>(total - seconds(val.m_sec)).count();
    val.m_isvalid = val.m_isvalid && d.m_isvalid;
    return val;
}

TimeDiff operator+(const TimeDiff& a, const TimeDiff& b) {
    using namespace std::chrono;
    auto total = seconds(a.m_sec) + microseconds(a.m_usec)
               + seconds(b.m_sec) + microseconds(b.m_usec);
    auto sec = duration_cast<seconds>(total).count();
    auto usec = duration_cast<microseconds>(total - seconds(sec)).count();
    return TimeDiff(sec, usec, a.m_isvalid && b.m_isvalid);
}

TimeDiff operator-(const TimeDiff& a, const TimeDiff& b) {
    using namespace std::chrono;
    auto total = seconds(a.m_sec) + microseconds(a.m_usec)
               - (seconds(b.m_sec) + microseconds(b.m_usec));
    auto sec = duration_cast<seconds>(total).count();
    auto usec = duration_cast<microseconds>(total - seconds(sec)).count();
    return TimeDiff(sec, usec, a.m_isvalid && b.m_isvalid);
}

bool operator<(const TimeDiff& a, const TimeDiff& b) {
	return (a.m_sec < b.m_sec) || ((a.m_sec == b.m_sec) && (a.m_usec < b.m_usec));
}

bool operator==(const TimeDiff& a, const TimeDiff& b) {
	return (a.m_sec == b.m_sec) && (a.m_usec == b.m_usec);
}

TimeStamp TimeStamp::now() {
    TimeStamp ret;
#ifndef _WIN32
    timeval tv{};
    gettimeofday(&tv, nullptr);
    ret._val.tv_sec = static_cast<std::int64_t>(tv.tv_sec);
    ret._val.tv_usec = static_cast<std::int64_t>(tv.tv_usec);
#else
    SYSTEMTIME sysTime;
    FILETIME fileTime = {0};  /* 100ns == 1 */
    LARGE_INTEGER i;

    GetSystemTime(&sysTime);
    SystemTimeToFileTime(&sysTime, &fileTime);
    /* Documented as the way to get a 64 bit from a
     * FILETIME. */
    memcpy(&i, &fileTime, sizeof(LARGE_INTEGER));

    ret._val.tv_sec = i.QuadPart / 10000000; /*10e7*/
    ret._val.tv_usec = (i.QuadPart / 10) % 1000000;  /*10e6*/
#endif
    ret._isvalid = true;
    return ret;
}

TimeStamp TimeStamp::epochStart() {
    TimeStamp ret;
    ret._val.tv_sec = 0;
    ret._val.tv_usec = 0;
    ret._isvalid = true;
    return ret;
}

TimeStamp::TimeStamp(std::int64_t sec, std::int64_t usec, bool isvalid)
    : _val{sec, usec}, _isvalid(isvalid) {
    if (_isvalid)
        regularize(_val.tv_sec, _val.tv_usec);
}

bool operator<(const TimeStamp& a, const TimeStamp& b) {
    if (a._val.tv_sec == b._val.tv_sec)
        return (a._val.tv_usec < b._val.tv_usec);
    else
        return a._val.tv_sec < b._val.tv_sec;
}

bool operator==(const TimeStamp& a, const TimeStamp& b) {
    return (a._val.tv_sec == b._val.tv_sec)
               && (a._val.tv_usec == b._val.tv_usec);
}

TimeStamp& operator+=(TimeStamp& a, const TimeDiff& d) {
    using namespace std::chrono;
    auto total = seconds(a._val.tv_sec) + microseconds(a._val.tv_usec)
               + seconds(d.m_sec) + microseconds(d.m_usec);
    a._val.tv_sec = duration_cast<seconds>(total).count();
    a._val.tv_usec = duration_cast<microseconds>(total - seconds(a._val.tv_sec)).count();
    a._isvalid = a._isvalid && d.m_isvalid;
    return a;
}

TimeStamp& operator-=(TimeStamp& a, const TimeDiff& d) {
    using namespace std::chrono;
    auto total = seconds(a._val.tv_sec) + microseconds(a._val.tv_usec)
               - (seconds(d.m_sec) + microseconds(d.m_usec));
    a._val.tv_sec = duration_cast<seconds>(total).count();
    a._val.tv_usec = duration_cast<microseconds>(total - seconds(a._val.tv_sec)).count();
    a._isvalid = a._isvalid && d.m_isvalid;
    return a;
}

TimeStamp operator+(const TimeStamp& a, const TimeDiff& d) {
    using namespace std::chrono;
    auto total = seconds(a._val.tv_sec) + microseconds(a._val.tv_usec)
               + seconds(d.m_sec) + microseconds(d.m_usec);
    auto sec = duration_cast<seconds>(total).count();
    auto usec = duration_cast<microseconds>(total - seconds(sec)).count();
    return TimeStamp(sec, usec, a._isvalid && d.m_isvalid);
}

TimeStamp operator-(const TimeStamp& a, const TimeDiff& d) {
    using namespace std::chrono;
    auto total = seconds(a._val.tv_sec) + microseconds(a._val.tv_usec)
               - (seconds(d.m_sec) + microseconds(d.m_usec));
    auto sec = duration_cast<seconds>(total).count();
    auto usec = duration_cast<microseconds>(total - seconds(sec)).count();
    return TimeStamp(sec, usec, a._isvalid && d.m_isvalid);
}

TimeDiff operator-(const TimeStamp& a, const TimeStamp& b) {
    using namespace std::chrono;
    auto total = seconds(a._val.tv_sec) + microseconds(a._val.tv_usec)
               - (seconds(b._val.tv_sec) + microseconds(b._val.tv_usec));
    auto sec = duration_cast<seconds>(total).count();
    auto usec = duration_cast<microseconds>(total - seconds(sec)).count();
    return TimeDiff(sec, usec, a._isvalid && b._isvalid);
}

}
