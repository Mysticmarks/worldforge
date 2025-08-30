// This test writes a configuration file to a temporary directory
// and cleans up afterwards so no artifacts remain in the working tree.
#include <varconf/varconf.h>

#include <sigc++/slot.h>

#include <iostream>
#include <string>
#include <sstream>
#include <filesystem>
#include <chrono>

#include <cassert>

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

int main(int argc, char** argv) {
	varconf::Config config;

	config.sige.connect(sigc::ptr_fun(error));
	config.sigsv.connect(sigc::ptr_fun(callback));

	config.setParameterLookup('f', "foo", true);
	config.setParameterLookup('b', "bar", false);

        config.getCmdline(argc, argv);
        config.getEnv("TEST_");
        std::filesystem::path exePath = std::filesystem::absolute(argv[0]).parent_path();
        assert(config.readFromFile((exePath / "conf.cfg").string()));
	config.setItem("tcp", "port", 6700, varconf::GLOBAL);
	config.setItem("tcp", "v6port", 6700, varconf::USER);
	config.setItem("console", "colours", "plenty", varconf::INSTANCE);
	config.setItem("console", "speed", "fast", varconf::USER);

	assert(config.find("tcp", "port"));
	assert(config.find("console", "enabled"));
	assert(config.getItem("tcp", "port")->scope() == varconf::GLOBAL);
	//Default scope for read files are USER
	assert(config.getItem("console", "enabled")->scope() == varconf::USER);

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

        assert(config.find("general", "setting"));
        assert(config.find("general", "emptyrightbeforeeof"));

        std::filesystem::path conf_file;
        {
                TempDir tmp;
                conf_file = tmp.path / "conf2.cfg";
                config.writeToFile(conf_file.string());
                assert(std::filesystem::exists(conf_file));

                varconf::Config file_conf;
                assert(file_conf.readFromFile(conf_file.string()));
                assert(file_conf.find("general", "setting"));
                assert(file_conf.find("general", "emptyrightbeforeeof"));
        }
        assert(!std::filesystem::exists(conf_file));

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

	return 0;
}
