/**
 Worldforge Next Generation MetaServer

 Copyright (C) 2011 Sean Ryan <sryan@evercrack.com>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 */

/*
 * Local Includes
 */
#include "MetaServerProtocol.hpp"
#include "MetaServer.hpp"
#include "MetaServerHandlerUDP.hpp"
#include "ShutdownException.hpp"

/*
 * System Includes
 */
#include <unistd.h> /* daemon(), getpid() */
#include <csignal>
#include <cstdlib> /* getenv() because boost po env parsing sucks */
#include <fstream>
#include <filesystem>
#include <MetaServerVersion.hpp>
#include <boost/exception/all.hpp>
#include <spdlog/spdlog.h>

/*
	Entry point
*/
int main(int argc, char** argv) {

	boost::asio::io_context io_service;
	boost::program_options::variables_map vm;

	int port = 8453;
	std::string ip = "0.0.0.0";

	/**
	 * Argument Wrangling
	 *
	 * Note: not exactly the most friendly option parsing I've seen
	 */
	boost::program_options::options_description desc("MetaServer Configuration");

	/**
	 * Note: options inside the configuration file that are NOT listed here
	 *       become ignored and are not accessible.
	 */
	desc.add_options()
			("help,h", "Display help message")
			("server.port,p", boost::program_options::value<int>(), "Server bind port. \nDefault:8543")
			("server.ip", boost::program_options::value<std::string>(), "Server bind IP. \nDefault:0.0.0.0")
			("server.daemon", boost::program_options::value<std::string>(), "Daemonize after startup [true|false].\nDefault: true")
			("server.logfile", boost::program_options::value<std::string>(), "Server logfile location.\nDefault: logdir/metaserver-ng.log")
			("server.pidfile", boost::program_options::value<std::string>(), "Server pidfile location.\nDefault: rundir/metaserver-ng.pid")
			("server.client_stats", boost::program_options::value<std::string>(), "Keep internals stats [true|false].  This can affect performance.\nDefault: false")
			("server.server_stats", boost::program_options::value<std::string>(), "Keep internals stats [true|false].  This can affect performance.\nDefault: false")
			("server.domain", boost::program_options::value<std::string>()->default_value("ms.worldforge.org"), "DNS Publishing Domain.\nDefault: ms.worldforge.org")
			("scoreboard.server", boost::program_options::value<std::string>(), "Scoreboard for the registered servers.")
			("scoreboard.client", boost::program_options::value<std::string>(), "Scoreboard for the registered clients.")
			("scoreboard.stats", boost::program_options::value<std::string>(), "Scoreboard for metaserver stats.")
			("scoreboard.ccache", boost::program_options::value<std::string>(), "Scoreboard for client cache")
			("logging.server_sessions", boost::program_options::value<std::string>(), "Output server sessions to logfile [true|false].  \nDefault: false")
			("logging.client_sessions", boost::program_options::value<std::string>(), "Output client sessions to logfile [true|false].  \nDefault: false")
			("logging.packet_logging", boost::program_options::value<std::string>(), "Output all packets to logfile [true|false].  \nDefault: false")
			("logging.packet_logfile", boost::program_options::value<std::string>(), "Packet logfile location.\nDefault: ~/.metaserver-ng/packetlog.bin")
			("logging.packet_allow", boost::program_options::value<std::string>(), "Output Firewall Allow Hits [true|false].  This can affect performance\nDefault: false")
			("logging.packet_deny", boost::program_options::value<std::string>(), "Output Firewall Deny Hits [true|false].  This can affect performance\nDefault: false")
			("security.auth_scheme", boost::program_options::value<std::string>(), "What method of authentication to use [none|server|delegate|both].\nDefault: none")
			("security.global_firewall_allow", boost::program_options::value<std::string>(), "Global Allow List [any|iplist].\nDefault: any")
			("security.global_firewall_deny", boost::program_options::value<std::string>(), "Global Deny List [any|none|iplist].\nDefault: none")
			("security.admin_firewall_allow", boost::program_options::value<std::string>(), "Admin Interface Allow List [any|none|iplist].\nDefault: any")
			("security.monitor_firewall_allow", boost::program_options::value<std::string>(), "Monitor Interface Allow List [any|none|iplist].\nDefault: any")
			("performance.max_server_sessions", boost::program_options::value<int>(), "Max number of server sessions [1-32768].\nDefault: 1024")
			("performance.max_client_sessions", boost::program_options::value<int>(), "Max number of client sessions [1-32768].\nDefault: 4096")
			("performance.server_session_expiry_seconds", boost::program_options::value<int>(), "Expiry in seconds for server sessions [300-3600].\nDefault: 300")
			("performance.client_session_expiry_seconds", boost::program_options::value<int>(), "Expiry in seconds for client sessions [300-3600].\nDefault: 300")
			("performance.tick_expiry_milliseconds", boost::program_options::value<int>()->default_value(3000), "Expiry Timer Interval.\nDefault: 3000")
			("performance.tick_update_milliseconds", boost::program_options::value<int>()->default_value(5000), "Update Timer Interval.\nDefault: 5000")
			("performance.tick_score_milliseconds", boost::program_options::value<int>()->default_value(60000), "Scoreboard Timer Interval.\nDefault: 60000");


	/*
	 * Environment Wrangling
	 */
	auto wfenv = std::getenv("WFHOME");
	auto home = std::getenv("HOME");
	bool config_found = false;
	std::filesystem::path config_file_path;

	/*
	 * Create a 'special' directory
	 */
	if (home != nullptr) {

		std::filesystem::path home_dot_dir = home;
		home_dot_dir /= ".metaserver-ng";

		if (!std::filesystem::exists(home_dot_dir)) {
			spdlog::info("Will create directory '{}'", home_dot_dir.string());
			/*
			 * NB: this throws, may need to fix that later.
			 * We don't care about the result though, either it made it or it
			 * didn't.  Any process that depends on the directory will check for it.
			 */
			std::filesystem::create_directory(home_dot_dir);
		}

	}

	/*
	 * HOME/.metaserver-ng.conf
	 *
	 * Try for local configuration first
	 */
	if (home != nullptr) {
		std::cout << "HOME : " << home << std::endl;
		config_file_path = home;
		config_file_path /= ".metaserver-ng.conf";
		std::cout << "Searching configuration : " << config_file_path.string() << " ... ";

		if (std::filesystem::is_regular_file(config_file_path)) {
			std::cout << " Accepted.";
			config_found = true;
		}
		std::cout << std::endl;
	}

	/*
	 * If WFHOME is setup, try to load distribution configuration
	 */
	if (!config_found && wfenv != nullptr) {
		std::cout << "WFHOME: " << wfenv << std::endl;
		/*
		 * PREFIX/etc/metaserver-ng.conf
		 */
		config_file_path /= wfenv;
		config_file_path /= "etc";
		config_file_path /= "metaserver-ng.conf";
		std::cout << "Searching configuration : " << config_file_path.string();

		if (std::filesystem::is_regular_file(config_file_path)) {
			std::cout << " Accepted.";
			config_found = true;
		}
		std::cout << std::endl;

	}

	/*
	 * Check in the $sysconfdir location (usually prefix/etc)
	 * This would be the "system" install.
	 */
	if (!config_found && std::filesystem::exists(DEFAULT_CONF)) {
		std::cout << "SYSTEM: " << DEFAULT_CONF << std::endl;

		config_file_path = DEFAULT_CONF;
		config_file_path /= "metaserver-ng.conf";
		std::cout << "Searching configuration : " << config_file_path.string() << " ... ";
		if (std::filesystem::is_regular_file(config_file_path)) {
			std::cout << " Accepted.";
			config_found = true;
		}
		std::cout << std::endl;
	}

	/*
	 * Last resort
	 */
	if (!config_found) {
		config_file_path = "metaserver-ng.conf";
		std::cout << "Default configuration : " << config_file_path.string() << std::endl;
		if (std::filesystem::is_regular_file(config_file_path)) {
			std::cout << "Using CWD Path: " << config_file_path.string() << std::endl;
		} else {
			std::cout << "No configuration file found!  All critical options are required to be passed via the command-line";
		}
	}

	/**
	 * Create our metaserver
	 */
	std::cout << "Create MetaServer instance" << std::endl;
	std::cout << "Server Version: " << SERVER_VERSION << std::endl;
	std::cout << "API    Version: " << API_VERSION << std::endl;
	std::cout << "Metaserver Config File: " << config_file_path.string() << std::endl;

	MetaServer ms;

	try {

		std::ifstream config_file(config_file_path.string());
		boost::program_options::store(
				boost::program_options::parse_command_line(argc, argv, desc),
				vm
		);

		boost::program_options::store(
				boost::program_options::parse_config_file(config_file, desc, true),
				vm
		);

		boost::program_options::notify(vm);

		/**
		 * Special case for help
		 */
		if (vm.count("help")) {
			std::cout << desc << std::endl;
			return 1;
		}

		/**
		 * The only real options that we need to process outside of the
		 * metaesrver as it affects the handlers
		 */
		if (vm.count("server.port"))
			port = vm["server.port"].as<int>();

		if (vm.count("server.ip"))
			ip = vm["server.ip"].as<std::string>();

		/**
		 * Register the configuration.
		 */
		std::cout << "Attempting to register MetaServer configuration ... " << std::endl;
		ms.registerConfig(vm);
		std::cout << "Logging to logfile: " << ms.getLogFile() << std::endl;
		spdlog::trace("Configuration Registered");

		/**
		 *	Create pid file
		 */
//		std::cout << "Checking server.pidfile ..." << std::endl;
		std::filesystem::path pid_file_path;
		if (vm.count("server.pidfile")) {

			/*
			 * Create rundir as required
			 */
			pid_file_path = vm["server.pidfile"].as<std::string>();
			spdlog::info("Pidfile: {}", pid_file_path.string());
			std::cout << "Pidfile: " << pid_file_path.string() << std::endl;

			if (!std::filesystem::is_directory(pid_file_path.parent_path().string())) {
				spdlog::info("Creating Run Directory : {}", pid_file_path.parent_path().string());
				std::cout << "Creating Run Directory : " << pid_file_path.parent_path().string() << std::endl;
				bool pf = std::filesystem::create_directory(pid_file_path.parent_path());
				if (!pf)
					throw std::runtime_error("Can not create run directory");
			}

			/*
			 * Drop in pid
			 */
			if (std::filesystem::is_regular_file(pid_file_path)) {
				std::cout << "Pidfile Exists: " << pid_file_path.string() << std::endl;
				spdlog::info("Pidfile Exists: {}", pid_file_path.string());

				/*
				 * PIDFILE is present, check is process is running
				 * if so, refuse to start, if not, clean up pidfile
				 */
				std::ifstream pif(pid_file_path.string());
				int pid = 0;
				pif >> pid;
				std::cout << "Checking pid : " << pid << std::endl;
				spdlog::info("Checking pid : {}");
				if (kill(pid, 0) == -1) {
					std::cout << "Pidfile is stale, removing." << std::endl;
					spdlog::info("Pidfile is stale, removing.");
					std::filesystem::remove(pid_file_path);
				} else {
					spdlog::info("Refusing to start!");
					throw std::runtime_error("Pidfile exists and process is running, refusing to start");
				}
			}

		} else {
			spdlog::info("Pidfile Not Specified");
			std::cout << "Pidfile Not Specified " << std::endl;
		}

		/**
		 * Go daemon if needed
		 *
		 * NOTE: It is critical that the pidfile checking go BEFORE daemon
		 * because the potential to inform the user via std::cout disappears
		 * but is is critical that the pid file be filled AFTER the daemon,
		 * as the daemon is forking a child, and that will be the running
		 * process, not here.
		 */
		if (ms.isDaemon()) {
			std::cout << "Running as daemon." << std::endl;
			std::cout << "NOTE: Be sure to check the logfile for any messages, as console output is now disabled." << std::endl;
			spdlog::info("Running as a daemon");
			if (daemon(0, 0) != 0) {
				spdlog::error("Problem executing as a deamon.");
			}
		}


		/*
		 * Write the getpid
		 *
		 * NOTE: must occur AFTER daemon
		 */
		if (!std::filesystem::is_regular_file(pid_file_path)) {
			std::ofstream pidfile;
			pidfile.open(pid_file_path.c_str());
			pidfile.clear();
			pidfile << getpid();
			pidfile.close();
		}


		/**
		 * Define Handlers
		 */
		//MetaServerHandlerUDP tcp(ms, io_service, ip, port);
		spdlog::trace("Start UPD Handler");
		MetaServerHandlerUDP udp(ms, io_service, ip, port);

		/*
		 * Register Timers
		 */
		std::cout << "Register Deadline Timers ..." << std::endl;
		ms.initTimers(io_service);

		/**
		 * This is the async loop
		 */
		while (!ms.isShutdown()) {
			spdlog::info("Enter ASYNC loop");
			try {
				/*
				 * We run and complete normally
				 */
				io_service.run();
				break;
			}
			catch (const boost::exception& bex) {
				std::cerr << "Boost Exception :" << std::endl;
				std::cerr << boost::diagnostic_information(bex) << std::endl;
				spdlog::error("Boost Exception :");
				spdlog::error(boost::diagnostic_information(bex));
				spdlog::error("Calling reinitialisation of the timers");
				ms.initTimers(io_service);
			}
                        catch (const ShutdownException& ex) {
                                std::cout << "Shutdown sequence initiated : " << ex.what() << std::endl;
                                spdlog::info("Shutdown sequence initiated : {}", ex.what());
                        }
                        catch (const std::exception& ex) {
                                std::cerr << "IOService Loop Exception:" << ex.what() << std::endl;
                                std::cerr << boost::diagnostic_information(ex) << std::endl;
                                spdlog::error("IOService Loop Exception: {}", ex.what());
                                spdlog::error(boost::diagnostic_information(ex));
                                spdlog::error("Calling reinitialisation of the timers");
                                ms.initTimers(io_service);
                        }
                }
                spdlog::info("Shutting Down Metaserver");

        }
        catch (const ShutdownException& e) {
                std::cerr << "Shutdown Exception: " << e.what() << std::endl;
                spdlog::info("All Done!");
                return 0;
        }
        catch (const std::exception& e) {
                std::cerr << "Unknown Exception: " << e.what() << std::endl;
                throw;
        }
        spdlog::info("All Done!");
        return 0;
}
