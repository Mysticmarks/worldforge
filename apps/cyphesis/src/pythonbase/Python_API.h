// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2000,2001 Alistair Riddoch
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA


#ifndef RULESETS_PYTHON_API_H
#define RULESETS_PYTHON_API_H

#include <string>
#include <set>
#include <vector>
#include <functional>
#include <sigc++/signal.h>
#include <boost/asio/io_context.hpp>
#include <spdlog/pattern_formatter.h>
#include "common/log.h"

class AssetsManager;

class BaseWorld;


/**
 * Emitted when python scripts needs reloading.
 */
extern sigc::signal<void()> python_reload_scripts;


namespace python_log {

inline std::vector<std::function<std::string()>>& prefix_stack()
{
    thread_local std::vector<std::function<std::string()>> stack;
    return stack;
}

inline std::string current_prefix()
{
    auto& stack = prefix_stack();
    if (stack.empty()) {
        return {};
    }
    return stack.back()();
}

class prefix_flag : public spdlog::custom_flag_formatter
{
public:
    void format(const spdlog::details::log_msg&, const std::tm&, spdlog::memory_buf_t& dest) override
    {
        auto prefix = current_prefix();
        dest.append(prefix.data(), prefix.data() + prefix.size());
    }

    std::unique_ptr<spdlog::custom_flag_formatter> clone() const override
    {
        return spdlog::details::make_unique<prefix_flag>();
    }
};

inline void install_python_log_formatter(const std::shared_ptr<spdlog::logger>& logger = spdlog::default_logger())
{
    auto formatter = std::make_unique<spdlog::pattern_formatter>();
    formatter->add_flag<prefix_flag>('!');
    formatter->set_pattern("%Y-%m-%d %H:%M:%S.%e %z [%t] %^%l%$ %!%v");
    logger->set_formatter(std::move(formatter));
}

} // namespace python_log

/**
 * Registers and unregisters the supplied log injection function.
 * The function is stored in thread-local storage and evaluated for each
 * log message through a custom spdlog formatter. This allows contextual
 * information to be injected while remaining safe for concurrent use.
 */
struct PythonLogGuard
{

    explicit PythonLogGuard(const std::function<std::string()>& logFn)
    {
        python_log::prefix_stack().push_back(logFn);
    }

    ~PythonLogGuard()
    {
        auto& stack = python_log::prefix_stack();
        if (!stack.empty()) {
            stack.pop_back();
        }
    }
};


/**
 * Initializes the Python embedded interpreter and scripting engine.
 * @param initFunctions A list of functions to call for registering modules.
 * @param scriptDirectories A list of file system directories in which to look for Python scripts to run at startup.
 * @param log_stdout True if Python should write log messages to std::cout.
 */
void init_python_api(std::vector<std::function<std::string()>> initFunctions, std::vector<std::string> scriptDirectories = {}, bool log_stdout = true);

void shutdown_python_api();

/**
 * When called will look in the XDG user directory for any scripts to run, for the supplied prefix.
 * @param prefix
 */
void run_user_scripts(const std::string& prefix);

void observe_python_directories(boost::asio::io_context& io_context, AssetsManager& assetsManager);


#endif // RULESETS_PYTHON_API_H
