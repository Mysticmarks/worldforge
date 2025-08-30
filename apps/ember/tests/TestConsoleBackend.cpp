#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/TestResult.h>

#include "framework/ConsoleBackend.h"

#include <algorithm>

namespace Ember {

class ConsoleBackendTestCase : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(ConsoleBackendTestCase);
        CPPUNIT_TEST(testSpeechFallback);
        CPPUNIT_TEST(testInvalidCommandEntry);
        CPPUNIT_TEST_SUITE_END();

public:
        void testSpeechFallback() {
                ConsoleBackend backend;
                backend.runCommand("hello world");
                const auto& messages = backend.getConsoleMessages();
                CPPUNIT_ASSERT(!messages.empty());
                CPPUNIT_ASSERT(messages.back() == "Cannot send speech: '/say' command not available.");
        }

        void testInvalidCommandEntry() {
                ConsoleBackend backend;
                ConsoleBackend::ConsoleCallback callback;
                backend.registerCommand("badcmd", callback, "bad command");
                backend.runCommand("/list_commands");
                const auto& messages = backend.getConsoleMessages();
                auto it = std::find_if(messages.begin(), messages.end(), [](const std::string& msg) {
                        return msg.find("Invalid command entry: badcmd") != std::string::npos;
                });
                CPPUNIT_ASSERT(it != messages.end());
        }
};

}

CPPUNIT_TEST_SUITE_REGISTRATION(Ember::ConsoleBackendTestCase);

int main(int argc, char** argv) {
        CppUnit::TextUi::TestRunner runner;
        CppUnit::TestFactoryRegistry& registry = CppUnit::TestFactoryRegistry::getRegistry();
        runner.addTest(registry.makeTest());

        CppUnit::BriefTestProgressListener listener;
        runner.eventManager().addListener(&listener);

        bool wasSuccessful = runner.run("", false);
        return !wasSuccessful;
}
