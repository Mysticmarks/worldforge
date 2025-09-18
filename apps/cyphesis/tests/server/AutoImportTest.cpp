// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2024
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

#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef DEBUG
#define DEBUG
#endif

#include "server/AutoImport.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::filesystem::path createTempDirectory(const std::string& name)
{
        auto root = std::filesystem::temp_directory_path() / name;
        std::filesystem::remove_all(root);
        std::filesystem::create_directories(root);
        return root;
}

} // namespace

int main()
{
        const auto tempRoot = createTempDirectory("cyphesis-autoimport-test");
        const auto shareDir = tempRoot / "share";
        const auto worldsDir = shareDir / "worlds";
        std::filesystem::create_directories(worldsDir);

        const auto varDir = tempRoot / "var";
        std::filesystem::create_directories(varDir);

        const std::string maliciousName = "malicious\";touch;";
        const auto maliciousPath = worldsDir / (maliciousName + "world.xml");

        {
                std::ofstream file(maliciousPath);
                file << "<world/>";
        }

        AutoImportSettings settings;
        settings.binDirectory = tempRoot.string();
        settings.etcDirectory = (tempRoot / "etc").string();
        settings.varDirectory = varDir.string();
        settings.shareDirectory = shareDir.string();

        auto originalLauncher = cyimportLauncher();

        bool launcherCalled = false;
        std::vector<std::string> capturedArguments;
        cyimportLauncher() = [&](const std::vector<std::string>& arguments) {
                launcherCalled = true;
                capturedArguments = arguments;
                return 0;
        };

        std::string errorMessage;
        auto invocation = prepareCyimportInvocation(settings, maliciousPath.string(), errorMessage);
        assert(invocation);
        assert(invocation->sanitizedImportPath == std::filesystem::weakly_canonical(maliciousPath));

        auto launcherResult = cyimportLauncher()(invocation->arguments);
        assert(launcherResult == 0);
        assert(launcherCalled);
        assert(capturedArguments.size() == 6);
        assert(capturedArguments.front().find("cyimport") != std::string::npos);
        assert(capturedArguments[4] == "--resume");
        assert(capturedArguments[5] == invocation->sanitizedImportPath.string());
        assert(capturedArguments[5].find('"') != std::string::npos);
        assert(capturedArguments[5].find(';') != std::string::npos);

        std::string controlPath = (shareDir / "bad\nname.xml").string();
        std::string controlError;
        auto invalidInvocation = prepareCyimportInvocation(settings, controlPath, controlError);
        assert(!invalidInvocation);

        cyimportLauncher() = originalLauncher;

        std::filesystem::remove_all(tempRoot);

        return 0;
}

