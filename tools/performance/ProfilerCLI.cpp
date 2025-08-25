#include "apps/ember/src/framework/Profiler.h"
#include <iostream>

int main() {
    std::cout << "terrainUpdate_ms " << Ember::Profiler::getMilliseconds("terrainUpdate") << std::endl;
    return 0;
}
