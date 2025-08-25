#pragma once

#include <chrono>
#include <string>
#include <unordered_map>

namespace Ember {

/**
 * Minimal profiling helper used to expose performance metrics.
 * The profiler stores accumulated millisecond timings for named scopes.
 */
class Profiler {
public:
    /** Start timing a named scope. */
    static void start(const std::string& name);

    /** Stop timing a named scope. */
    static void stop(const std::string& name);

    /** Retrieve the accumulated milliseconds for a scope. */
    static double getMilliseconds(const std::string& name);

private:
    struct Entry {
        std::chrono::high_resolution_clock::time_point start;
        double total{0.0};
        bool running{false};
    };

    static std::unordered_map<std::string, Entry> sEntries;
};

} // namespace Ember

