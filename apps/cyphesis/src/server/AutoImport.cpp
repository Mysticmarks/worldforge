#include "AutoImport.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdlib>
#include <system_error>

#ifndef _WIN32
#include <spawn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#else
#include <process.h>
#endif

#ifndef _WIN32
extern char** environ;
#endif

namespace {

bool containsControlCharacter(const std::string& value)
{
        return std::any_of(value.begin(), value.end(), [](unsigned char ch) {
                return ch < 0x20 || ch == 0x7f;
        });
}

std::filesystem::path normalizePath(const std::filesystem::path& path)
{
        std::error_code ec;
        auto normalized = std::filesystem::weakly_canonical(path, ec);
        if (ec) {
                normalized = std::filesystem::absolute(path, ec).lexically_normal();
        }
        return normalized;
}

bool isSubPath(const std::filesystem::path& base, const std::filesystem::path& candidate)
{
        auto baseIter = base.begin();
        auto candidateIter = candidate.begin();
        for (; baseIter != base.end(); ++baseIter, ++candidateIter) {
                if (candidateIter == candidate.end()) {
                        return false;
                }
                if (*baseIter != *candidateIter) {
                        return false;
                }
        }
        return true;
}

std::vector<std::filesystem::path> buildAllowedRoots(const AutoImportSettings& settings)
{
        std::vector<std::filesystem::path> roots = settings.allowedRoots;
#ifdef _WIN32
        constexpr char delimiter = ';';
#else
        constexpr char delimiter = ':';
#endif
        if (!settings.shareDirectory.empty()) {
                if (settings.shareDirectory.find(delimiter) == std::string::npos) {
                        roots.emplace_back(settings.shareDirectory);
                } else {
                        std::string current;
                        for (char ch : settings.shareDirectory) {
                                if (ch == delimiter) {
                                        if (!current.empty()) {
                                                roots.emplace_back(current);
                                                current.clear();
                                        }
                                } else {
                                        current.push_back(ch);
                                }
                        }
                        if (!current.empty()) {
                                roots.emplace_back(current);
                        }
                }
        }
        if (!settings.varDirectory.empty()) {
                roots.emplace_back(settings.varDirectory);
        }
        return roots;
}

} // namespace

std::optional<CyimportInvocation> prepareCyimportInvocation(const AutoImportSettings& settings,
                                                            const std::string& importPath,
                                                            std::string& errorMessage)
{
        if (containsControlCharacter(importPath)) {
                errorMessage = "Import path contains control characters.";
                return std::nullopt;
        }

        std::filesystem::path normalizedImport;
        try {
                normalizedImport = normalizePath(importPath);
        } catch (const std::exception& ex) {
                errorMessage = std::string("Failed to normalize import path: ") + ex.what();
                return std::nullopt;
        }

        const auto allowedRoots = buildAllowedRoots(settings);
        bool insideAllowedRoot = false;
        for (const auto& root : allowedRoots) {
                if (root.empty()) {
                        continue;
                }
                std::filesystem::path normalizedRoot;
                try {
                        normalizedRoot = normalizePath(root);
                } catch (const std::exception&) {
                        continue;
                }
                if (normalizedRoot.empty()) {
                        continue;
                }
                if (isSubPath(normalizedRoot, normalizedImport)) {
                        insideAllowedRoot = true;
                        break;
                }
        }

        if (!insideAllowedRoot) {
                errorMessage = "Import path is not located inside an allowed directory.";
                return std::nullopt;
        }

        CyimportInvocation invocation;
        invocation.sanitizedImportPath = normalizedImport;

        std::filesystem::path cyimportExecutable = std::filesystem::path(settings.binDirectory) / "cyimport";
        invocation.arguments.emplace_back(cyimportExecutable.string());
        invocation.arguments.emplace_back("--cyphesis:confdir=" + settings.etcDirectory);
        invocation.arguments.emplace_back("--cyphesis:vardir=" + settings.varDirectory);
        invocation.arguments.emplace_back("--cyphesis:directory=" + settings.shareDirectory);
        invocation.arguments.emplace_back("--resume");
        invocation.arguments.emplace_back(invocation.sanitizedImportPath.string());

        return invocation;
}

CyimportLaunchFn& cyimportLauncher()
{
        static CyimportLaunchFn launcher = spawnCyimportProcess;
        return launcher;
}

int spawnCyimportProcess(const std::vector<std::string>& arguments)
{
        if (arguments.empty()) {
                return -1;
        }

#ifdef _WIN32
        std::vector<const char*> argv;
        argv.reserve(arguments.size() + 1);
        for (const auto& arg : arguments) {
                argv.push_back(arg.c_str());
        }
        argv.push_back(nullptr);
        intptr_t result = _spawnv(_P_WAIT, arguments.front().c_str(), argv.data());
        if (result == -1) {
                return -1;
        }
        return static_cast<int>(result);
#else
        std::vector<char*> argv;
        argv.reserve(arguments.size() + 1);
        for (const auto& arg : arguments) {
                argv.push_back(const_cast<char*>(arg.c_str()));
        }
        argv.push_back(nullptr);

        pid_t pid = 0;
        int spawnResult = posix_spawn(&pid, arguments.front().c_str(), nullptr, nullptr, argv.data(), environ);
        if (spawnResult != 0) {
                errno = spawnResult;
                return -1;
        }

        int status = 0;
        pid_t waitResult = 0;
        do {
                waitResult = waitpid(pid, &status, 0);
        } while (waitResult == -1 && errno == EINTR);

        if (waitResult == -1) {
                return -1;
        }

        if (WIFEXITED(status)) {
                return WEXITSTATUS(status);
        }
        if (WIFSIGNALED(status)) {
                return 128 + WTERMSIG(status);
        }
        return -1;
#endif
}

