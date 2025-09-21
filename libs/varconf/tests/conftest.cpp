// This test writes a configuration file to a temporary directory
// and cleans up afterwards so no artifacts remain in the working tree.
#include <varconf/varconf.h>

#include <sigc++/slot.h>

#include <catch2/catch_test_macros.hpp>

#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <chrono>

struct TempDir {
        std::filesystem::path path;
        TempDir() {
                auto stamp = std::chrono::high_resolution_clock::now().time_since_epoch().count();
                path = std::filesystem::temp_directory_path() / ("varconf-test-" + std::to_string(stamp));
                std::filesystem::create_directories(path);
        }
        ~TempDir() { std::filesystem::remove_all(path); }
};

void callback(const std::string& section,
                          const std::string& key,
                          varconf::Config& conf) {
        std::cout << "\nConfig Change: item " << key << " under section " << section
                          << " has changed to " << conf.getItem(section, key) << ".\n";
}

void error(const char* message) {
        std::cerr << message;
}

TEST_CASE("Configuration file operations", "[varconf]") {
        varconf::Config config;

        config.sige.connect(sigc::ptr_fun(error));
        config.sigsv.connect(sigc::ptr_fun(callback));

        config.setParameterLookup('f', "foo", true);
        config.setParameterLookup('b', "bar", false);

        // Simulate minimal command line for getCmdline
        char arg0[] = "conftest";
        char* argv[] = {arg0};
        config.getCmdline(1, argv);
        config.getEnv("TEST_");

        std::filesystem::path exePath = std::filesystem::current_path();
        REQUIRE(config.readFromFile((exePath / "conf.cfg").string()));
        config.setItem("tcp", "port", 6700, varconf::GLOBAL);
        config.setItem("tcp", "v6port", 6700, varconf::USER);
        config.setItem("console", "colours", "plenty", varconf::INSTANCE);
        config.setItem("console", "speed", "fast", varconf::USER);

        REQUIRE(config.find("tcp", "port"));
        REQUIRE(config.find("console", "enabled"));
        REQUIRE(config.getItem("tcp", "port")->scope() == varconf::GLOBAL);
        // Default scope for read files are USER
        REQUIRE(config.getItem("console", "enabled")->scope() == varconf::USER);

        SECTION("Parse configuration stream") {
                std::cout << "\nEnter sample configuration data to test parseStream() method.\n";

                std::stringstream ss;
                ss << "[general]" << std::endl;
                ss << "setting = true" << std::endl;
                ss << "emptyrightbeforeeof = ";

                try {
                        config.parseStream(ss, varconf::USER);
                }
                catch (const varconf::ParseError& p) {
                        std::cout << "\nError while parsing from input stream.\n";
                        std::cout << p.what();
                }

                REQUIRE(config.find("general", "setting"));
                REQUIRE(config.find("general", "emptyrightbeforeeof"));
        }

        SECTION("Command line parsing handles scoped long options") {
                varconf::Config cli;

                char arg0[] = "conftest";
                char arg1[] = "--network:port=7777";
                char arg2[] = "--graphics:quality=high";
                char arg3[] = "--console:logging";
                char* argvScoped[] = {arg0, arg1, arg2, arg3};
                const int argcScoped = sizeof(argvScoped) / sizeof(argvScoped[0]);

                REQUIRE(cli.getCmdline(argcScoped, argvScoped, varconf::GLOBAL) == argcScoped);

                REQUIRE(cli.find("network", "port"));
                auto port = cli.getItem("network", "port");
                REQUIRE(port->scope() == varconf::GLOBAL);
                REQUIRE(static_cast<std::string>(port.elem()) == "7777");

                REQUIRE(cli.find("graphics", "quality"));
                auto quality = cli.getItem("graphics", "quality");
                REQUIRE(static_cast<std::string>(quality.elem()) == "high");

                REQUIRE(cli.find("console", "logging"));
                auto logging = cli.getItem("console", "logging");
                REQUIRE(static_cast<std::string>(logging.elem()).empty());
        }

        SECTION("Command line parsing ignores dangling section delimiters") {
                varconf::Config cli;

                char arg0[] = "conftest";
                char arg1[] = "--orphan:";
                char* argvDangling[] = {arg0, arg1};
                const int argcDangling = sizeof(argvDangling) / sizeof(argvDangling[0]);

                REQUIRE(cli.getCmdline(argcDangling, argvDangling) == 1);
                REQUIRE_FALSE(cli.find("orphan"));
        }

        SECTION("Write configuration to file and read back") {
                std::filesystem::path conf_file;
                {
                        TempDir tmp;
                        conf_file = tmp.path / "conf2.cfg";
                        config.writeToFile(conf_file.string());
                        REQUIRE(std::filesystem::exists(conf_file));

                        varconf::Config file_conf;
                        REQUIRE(file_conf.readFromFile(conf_file.string()));
                        REQUIRE(file_conf.find("general", "setting"));
                        REQUIRE(file_conf.find("general", "emptyrightbeforeeof"));
                }
                REQUIRE(!std::filesystem::exists(conf_file));
        }

        SECTION("Output configuration data") {
                std::cout << "\nFile configuration data:\n"
                                  << "--------------------------\n"
                                  << config;

                std::cout << "\nUSER configuration data:\n"
                                  << "--------------------------\n";

                config.writeToStream(std::cout, varconf::USER);

                std::cout << "\nINSTANCE configuration data:\n"
                                  << "--------------------------\n";

                config.writeToStream(std::cout, varconf::INSTANCE);

                std::cout << "\nGLOBAL configuration data:\n"
                                  << "--------------------------\n";

                config.writeToStream(std::cout, varconf::GLOBAL);

                std::cout << "\nGLOBAL & USER configuration data:\n"
                                  << "--------------------------\n";

                config.writeToStream(std::cout, (varconf::Scope) (varconf::GLOBAL | varconf::USER));

                std::cout << "\nINSTANCE & USER configuration data:\n"
                                  << "--------------------------\n";

                config.writeToStream(std::cout, (varconf::Scope) (varconf::INSTANCE | varconf::USER));

                std::cout << "\nINSTANCE & GLOBAL configuration data:\n"
                                  << "--------------------------\n";

                config.writeToStream(std::cout, (varconf::Scope) (varconf::GLOBAL | varconf::INSTANCE));

                std::cout << "\nINSTANCE, USER & GLOBAL configuration data:\n"
                                  << "--------------------------\n";

                config.writeToStream(std::cout, (varconf::Scope) (varconf::GLOBAL | varconf::INSTANCE | varconf::USER));
        }
}
