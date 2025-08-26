#include "squall/core/Generator.h"
#include "squall/core/Repository.h"
#include "squall/core/Iterator.h"
#include "squall/core/Realizer.h"
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace Squall;

TEST_CASE("PBR textures sync to client", "[pbr]") {
    setupEncodings();

    std::filesystem::path sourceDir("PBRSourceDir");
    std::filesystem::remove_all(sourceDir);
    std::filesystem::create_directories(sourceDir);
    std::vector<std::string> textures{"albedo.png", "metallic.png", "roughness.png", "normal.png", "ao.png"};
    for (const auto &tex : textures) {
        std::ofstream(sourceDir / tex) << "data";
    }

    Repository repository("PBRRepository");
    Generator generator(repository, sourceDir);
    auto genResult = generator.process(10);
    REQUIRE(genResult.complete);

    auto rootSignature = genResult.processedFiles.back().fileEntry.signature;
    auto manifestResult = repository.fetchManifest(rootSignature);
    REQUIRE(manifestResult.manifest.has_value());
    Iterator iterator(repository, *manifestResult.manifest);

    std::filesystem::path destDir("PBRDestination");
    std::filesystem::remove_all(destDir);
    std::filesystem::create_directories(destDir);
    Realizer realizer(repository, destDir, iterator);
    RealizeResult result{};
    do {
        result = realizer.poll();
    } while (result.status == RealizeStatus::INPROGRESS);

    for (const auto &tex : textures) {
        REQUIRE(std::filesystem::exists(destDir / tex));
    }
}
