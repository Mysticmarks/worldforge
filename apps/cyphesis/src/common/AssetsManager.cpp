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

#include <boost/algorithm/string.hpp>
#include "AssetsManager.h"
#include "FileSystemObserver.h"
#include "globals.h"
#include "log.h"
#include <vector>

namespace {
auto callback(std::map<std::filesystem::path, std::list<std::function<void(const std::filesystem::path& path)>>>& callbacks,
			  std::map<std::filesystem::path, std::list<std::function<void(const std::filesystem::path& path)>>>& directoryCallbacks) {
	auto observerCallback = [&](const FileSystemObserver::FileSystemEvent& event) {
		auto I = callbacks.find(event.ev.path);
		if (I != callbacks.end()) {
			for (auto& callback: I->second) {
				callback(event.ev.path);
			}
		}
		spdlog::debug("Received file system change event of type {}. Path: {}", event.ev.type_cstr(), event.ev.path.string());

		for (auto& entry: directoryCallbacks) {

			if (event.ev.path.string().starts_with(entry.first.string())) {
				if (!std::filesystem::is_directory(event.ev.path)) {
					for (auto& callback: entry.second) {
						callback(event.ev.path);
					}
				} else {
					//Handle new files and directories being added. It's trickier with stuff being removed though.
					if (event.ev.type == boost::asio::dir_monitor_event::added) {
						std::filesystem::recursive_directory_iterator dir(event.ev.path), end{};
						while (dir != end) {
							if (!std::filesystem::is_directory(dir->status())) {
								for (auto& callback: entry.second) {
									callback(dir->path());
								}
							}
							++dir;
						}
					}
				}
			}
		}
	};
	return observerCallback;
}
}

AssetsManager::AssetsManager(std::unique_ptr<FileSystemObserver> file_system_observer)
		: m_file_system_observer(std::move(file_system_observer)) {

        auto observerCallback = callback(m_callbacks, m_directoryCallbacks);

        // Enumerate all configured share directories and register both scripts and rulesets paths.
        std::vector<std::string> share_dirs;
        boost::algorithm::split(share_dirs, share_directory, boost::is_any_of(":"));
        for (const auto& dir: share_dirs) {
                if (dir.empty()) {
                        continue;
                }
                m_file_system_observer->add_directory(std::filesystem::path(dir) / "cyphesis" / "scripts", observerCallback);
                m_file_system_observer->add_directory(std::filesystem::path(dir) / "cyphesis" / "rulesets", observerCallback);
        }

        // Enumerate all configured etc directories.
        std::vector<std::string> etc_dirs;
        boost::algorithm::split(etc_dirs, etc_directory, boost::is_any_of(":"));
        for (const auto& dir: etc_dirs) {
                if (dir.empty()) {
                        continue;
                }
                m_file_system_observer->add_directory(std::filesystem::path(dir) / "cyphesis", observerCallback);
        }
}

AssetsManager::~AssetsManager() = default;

void AssetsManager::observeAssetsDirectory() {
        std::vector<std::string> asset_dirs;
        boost::algorithm::split(asset_dirs, assets_directory, boost::is_any_of(":"));

        auto observerCallback = callback(m_callbacks, m_directoryCallbacks);

        for (const auto& dir: asset_dirs) {
                if (dir.empty()) {
                        continue;
                }
                std::filesystem::path assetsDirectory(dir);
                if (std::filesystem::exists(assetsDirectory)) {
                        if (mAssetsPath.empty()) {
                                mAssetsPath = assetsDirectory;
                        }
                        m_file_system_observer->add_directory(assetsDirectory, observerCallback);
                }
        }

        if (mAssetsPath.empty()) {
                mAssetsPath = std::filesystem::path(CYPHESIS_RAW_ASSETS_DIRECTORY);
                if (std::filesystem::exists(mAssetsPath)) {
                        spdlog::info("Could not find any assets directory in '{}' but found raw assets in '{}'.", assets_directory, CYPHESIS_RAW_ASSETS_DIRECTORY);
                        m_file_system_observer->add_directory(mAssetsPath, observerCallback);
                } else {
                        spdlog::error("Could not find neither assets directory in '{}' or found raw assets in '{}'. Will continue but the server will probably not function correctly.", assets_directory,
                                                  CYPHESIS_RAW_ASSETS_DIRECTORY);
                }
        }
}


void AssetsManager::observeFile(std::filesystem::path path, const std::function<void(const std::filesystem::path& path)>& callback) {
	m_callbacks[std::move(path)].push_back(callback);
}

void AssetsManager::observeDirectory(std::filesystem::path path, const std::function<void(const std::filesystem::path& path)>& callback) {
	m_directoryCallbacks[std::move(path)].push_back(callback);
}

void AssetsManager::stopFileObserver() {
	m_file_system_observer->stop();
}
