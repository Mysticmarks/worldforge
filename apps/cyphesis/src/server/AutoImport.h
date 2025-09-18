#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

struct AutoImportSettings {
        std::string binDirectory;
        std::string etcDirectory;
        std::string varDirectory;
        std::string shareDirectory;
        std::vector<std::filesystem::path> allowedRoots;
};

struct CyimportInvocation {
        std::vector<std::string> arguments;
        std::filesystem::path sanitizedImportPath;
};

std::optional<CyimportInvocation> prepareCyimportInvocation(const AutoImportSettings& settings,
                                                            const std::string& importPath,
                                                            std::string& errorMessage);

using CyimportLaunchFn = std::function<int(const std::vector<std::string>&)>;

CyimportLaunchFn& cyimportLauncher();

int spawnCyimportProcess(const std::vector<std::string>& arguments);

