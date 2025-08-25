#include "Profiler.h"

namespace Ember {

std::unordered_map<std::string, Profiler::Entry> Profiler::sEntries;

void Profiler::start(const std::string& name) {
    auto& entry = sEntries[name];
    entry.start = std::chrono::high_resolution_clock::now();
    entry.running = true;
}

void Profiler::stop(const std::string& name) {
    auto now = std::chrono::high_resolution_clock::now();
    auto& entry = sEntries[name];
    if (entry.running) {
        entry.total +=
            std::chrono::duration<double, std::milli>(now - entry.start).count();
        entry.running = false;
    }
}

double Profiler::getMilliseconds(const std::string& name) {
    auto it = sEntries.find(name);
    if (it == sEntries.end()) {
        return 0.0;
    }
    return it->second.total;
}

} // namespace Ember

