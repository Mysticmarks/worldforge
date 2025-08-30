// Command-line test for cydb user modification
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <sys/wait.h>

int run(const std::string& cmd) {
    int rc = std::system(cmd.c_str());
    if (rc == -1) {
        return -1;
    }
#ifdef _WIN32
    return rc;
#else
    if (WIFEXITED(rc)) {
        return WEXITSTATUS(rc);
    }
    return rc;
#endif
}

int main(int argc, char** argv) {
    namespace fs = std::filesystem;
    fs::path exeDir = fs::path(argv[0]).parent_path();
    fs::path cydb = fs::canonical(exeDir / "../../src/tools/cydb");
    fs::path varDir = exeDir / "cydbtest_var";
    fs::remove_all(varDir);
    fs::create_directories(varDir);

    std::string base = cydb.string() + " --cyphesis:vardir=" + varDir.string() + " user mod -p secret";

    int ret = run(base + " admin > /dev/null 2>&1");
    assert(ret == 0);

    ret = run(base + " missing > /dev/null 2>&1");
    assert(ret != 0);

    return 0;
}
