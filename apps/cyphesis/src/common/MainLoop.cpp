/*
 Copyright (C) 2018 Erik Ogenvik

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
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "MainLoop.h"

#include "globals.h"
#include "OperationsDispatcher.h"
#include "log.h"
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include "Remotery.h"
#include <thread>
#include <vector>

namespace {
void interactiveSignalsHandler(boost::asio::signal_set& this_, boost::system::error_code error, int signal_number) {
	if (!error) {
		switch (signal_number) {
			case SIGINT:
			case SIGTERM:
			case SIGHUP:
				//If we've already received one call to shut down softly we should elevate
				//it to a hard shutdown.
				//This also happens if "soft" exit isn't enabled.
				if (exit_flag_soft || !exit_soft_enabled) {
					exit_flag = true;
				} else {
					exit_flag_soft = true;
				}
				break;
			case SIGQUIT:
				exit_flag = true;
				break;
			default:
				break;
		}
		this_.async_wait([&this_](boost::system::error_code error_in, int signal_nbr) { interactiveSignalsHandler(this_, error_in, signal_nbr); });
	}
}

void daemonSignalsHandler(boost::asio::signal_set& this_, boost::system::error_code error, int signal_number) {
	if (!error) {
		switch (signal_number) {
			case SIGTERM:
				//If we've already received one call to shut down softly we should elevate
				//it to a hard shutdown.
				//This also happens if "soft" exit isn't enabled.
				if (exit_flag_soft || !exit_soft_enabled) {
					exit_flag = true;
				} else {
					exit_flag_soft = true;
				}
				break;
			default:
				break;
		}
		this_.async_wait([&this_](boost::system::error_code error_in, int signal_nbr) { daemonSignalsHandler(this_, error_in, signal_nbr); });
	}
}
}

void MainLoop::run(bool daemon,
                                   boost::asio::io_context& io_context,
                                   OperationsHandler& operationsHandler,
                                   const Callbacks& callbacks,
                                   std::chrono::steady_clock::duration& time,
                                   std::size_t io_threads) {

	boost::asio::signal_set signalSet(io_context);
	//If we're not running as a daemon we should use the interactive signal handler.
	if (!daemon) {
		//signalSet.add(SIGINT);
		signalSet.add(SIGTERM);
		signalSet.add(SIGHUP);
		signalSet.add(SIGQUIT);

		signalSet.async_wait([&signalSet](boost::system::error_code error, int signal_number) { interactiveSignalsHandler(signalSet, error, signal_number); });
	} else {
		signalSet.add(SIGTERM);

		signalSet.async_wait([&signalSet](boost::system::error_code error, int signal_number) { daemonSignalsHandler(signalSet, error, signal_number); });
	}


	bool soft_exit_in_progress = false;


        //Make sure that the io_context never runs out of work.
        auto work = boost::asio::make_work_guard(io_context);
        //This timer will set a deadline for any mind persistence during soft exits.
        boost::asio::steady_timer softExitTimer(io_context);

        //Start a pool of threads running the io_context.
        std::vector<std::thread> threads;
        threads.reserve(io_threads);
        for (std::size_t i = 0; i < io_threads; ++i) {
                threads.emplace_back([&io_context]() {
                        try {
                                io_context.run();
                        } catch (const std::exception& ex) {
                                spdlog::error("Exception in io_context thread: {}", ex.what());
                        }
                });
        }

	std::chrono::steady_clock::duration tick_size = std::chrono::milliseconds(10);
	// Loop until the exit flag is set. The exit flag can be set anywhere in
	// the code easily.
	while (!exit_flag) {

		rmt_ScopedCPUSample(MainLoop, 0)

                auto frameStartTime = std::chrono::steady_clock::now();
                auto max_wall_time = std::chrono::milliseconds(8);
                auto nextTick = frameStartTime + tick_size;

                time += tick_size;

                //Dispatch any incoming messages first
                {
                        rmt_ScopedCPUSample(dispatchOperations, 0)
                        callbacks.dispatchOperations();
                }
                {
                        rmt_ScopedCPUSample(processOps, 0)
                        operationsHandler.processUntil(time, max_wall_time);
                }
		if (soft_exit_in_progress) {
			//If we're in soft exit mode and either the deadline has been exceeded
			//or we've persisted all minds we should shut down normally.
			if (!callbacks.softExitPoll || callbacks.softExitPoll()) {
				exit_flag = true;
				softExitTimer.cancel();
			}
		} else if (exit_flag_soft) {
			exit_flag_soft = false;
			soft_exit_in_progress = true;
			if (callbacks.softExitStart) {
				auto duration = callbacks.softExitStart();
#if BOOST_VERSION >= 106600
				softExitTimer.expires_after(duration);
#else
				softExitTimer.expires_from_now(duration);
#endif
				softExitTimer.async_wait([&](boost::system::error_code ec) {
					if (!ec) {
						if (callbacks.softExitTimeout) {
							callbacks.softExitTimeout();
						}
						exit_flag = true;
					}
				});
			}
		}
                //Sleep until next tick
                auto now = std::chrono::steady_clock::now();
                if (now < nextTick) {
                        std::this_thread::sleep_until(nextTick);
                }
        }
        // exit flag has been set so we close down the databases, and indicate
        // to the metaserver (if we are using one) that this server is going down.
        // It is assumed that any preparation for the shutdown that is required
        // by the game has been done before exit flag was set.
        spdlog::debug("Performing clean shutdown...");


        signalSet.cancel();
        signalSet.clear();

        //Allow io_context.run() to exit and join all threads
        softExitTimer.cancel();
        work.reset();
        io_context.stop();
        for (auto& t : threads) {
                if (t.joinable()) {
                        t.join();
                }
        }

}
