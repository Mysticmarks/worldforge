// Verify that PythonLogGuard prefixes log messages in a thread-safe manner.

#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef DEBUG
#define DEBUG
#endif

#include "pythonbase/Python_API.h"

#include <spdlog/sinks/ostream_sink.h>
#include <cassert>
#include <thread>
#include <sstream>
#include <vector>

int main()
{
    std::ostringstream oss;
    auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(oss);
    auto logger = std::make_shared<spdlog::logger>("test", sink);
    python_log::install_python_log_formatter(logger);
    auto old = spdlog::default_logger();
    spdlog::set_default_logger(logger);

    const int num_threads = 4;
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([i]() {
            PythonLogGuard guard([i]() { return "[T" + std::to_string(i) + "] "; });
            spdlog::info("message {}", i);
        });
    }
    for (auto& t : threads) {
        t.join();
    }

    spdlog::set_default_logger(old);
    auto out = oss.str();
    for (int i = 0; i < num_threads; ++i) {
        std::string expected = "[T" + std::to_string(i) + "] message " + std::to_string(i);
        assert(out.find(expected) != std::string::npos);
    }
    return 0;
}
