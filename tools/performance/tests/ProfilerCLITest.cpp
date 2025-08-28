#include <cassert>
#include <fstream>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>
#include "../../../apps/ember/src/framework/Profiler.h"

int main() {
    using namespace Ember;
    Profiler::start("terrainUpdate");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Profiler::stop("terrainUpdate");

    std::system("./ProfilerCLI > out1.txt");
    {
        std::ifstream in("out1.txt");
        std::string label; double val;
        in >> label >> val;
        assert(label == "terrainUpdate_ms");
        assert(val > 0.0);
    }

    std::system("./ProfilerCLI extra > out2.txt");
    {
        std::ifstream in("out2.txt");
        std::string label; double val;
        in >> label >> val;
        assert(label == "terrainUpdate_ms");
        assert(val > 0.0);
    }
    return 0;
}
