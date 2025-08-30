#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef DEBUG
#define DEBUG
#endif

#include "tools/Interactive.h"
#include "tools/JunctureContext.h"

#include <Atlas/Objects/Operation.h>
#include <boost/asio/io_context.hpp>

#include <cassert>
#include <sstream>

class TestInteractive : public Interactive {
public:
    using Interactive::Interactive;
    Atlas::Objects::Operation::RootOperation lastSent;

    void send(const Atlas::Objects::Operation::RootOperation& op) override {
        lastSent = op;
        reply_flag = true;
    }
};

int main() {
    boost::asio::io_context io;
    Atlas::Objects::Factories factories;

    TestInteractive interactive(factories, io);
    auto ctx = std::make_shared<JunctureContext>(interactive, "1");
    interactive.addCurrentContext(ctx);

    interactive.exec("connect", "localhost 6767");
    assert(!interactive.lastSent->isDefaultSerialno());

    std::ostringstream out;
    auto* oldBuf = std::cout.rdbuf(out.rdbuf());
    interactive.exec("flush", "");
    std::cout.rdbuf(oldBuf);
    assert(out.str().find("usage: flush <type>") != std::string::npos);

    return 0;
}

extern "C" {
int rl_set_prompt(const char*) { return 0; }
void rl_redisplay() {}
}
