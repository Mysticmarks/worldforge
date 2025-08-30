#include <services/serversettings/ServerSettings.h>
#include <services/serversettings/ServerSettingsCredentials.h>
#include <services/config/ConfigService.h>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <spdlog/logger.h>

std::shared_ptr<spdlog::logger> logger = std::make_shared<spdlog::logger>("test");

TEST_CASE("ServerSettings persist values", "[ServerSettings]") {
    using namespace Ember;
    auto tmpDir = std::filesystem::temp_directory_path() / "serversettings-test";
    std::filesystem::remove_all(tmpDir);
    std::filesystem::create_directories(tmpDir);

    ConfigService config(tmpDir);
    config.setHomeDirectory(tmpDir.string());

    {
        ServerSettings settings;
        ServerSettingsCredentials creds("localhost", "testserver");
        settings.setItem(creds, "key", "value");
        settings.writeToDisk();
    }

    {
        ServerSettings settings;
        ServerSettingsCredentials creds("localhost", "testserver");
        REQUIRE(settings.findItem(creds, "key"));
        auto val = settings.getItem(creds, "key");
        REQUIRE(static_cast<std::string>(val) == "value");
    }

    std::filesystem::remove_all(tmpDir);
}

