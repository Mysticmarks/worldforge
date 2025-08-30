#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/TestResult.h>

#include "services/EmberServices.h"
#include "services/scripting/ScriptingService.h"
#include "services/config/ConfigService.h"

#include <filesystem>

namespace Ember {

struct TrackingScriptingService : ScriptingService {
        static bool destroyed;
        ~TrackingScriptingService() override { destroyed = true; }
};

bool TrackingScriptingService::destroyed = false;

class EmberServicesTestCase : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(EmberServicesTestCase);
        CPPUNIT_TEST(testConstructionAndTeardown);
        CPPUNIT_TEST_SUITE_END();

public:
        void testConstructionAndTeardown() {
                TrackingScriptingService::destroyed = false;
                ConfigService config(std::filesystem::current_path());
                {
                        auto scripting = std::make_unique<TrackingScriptingService>();
                        TrackingScriptingService* ptr = scripting.get();
                        EmberServices services(config,
                                                std::move(scripting),
                                                std::unique_ptr<SoundService>(),
                                                std::unique_ptr<ServerService>(),
                                                std::unique_ptr<MetaserverService>(),
                                                std::unique_ptr<ServerSettings>());
                        CPPUNIT_ASSERT(services.scriptingService.get() == ptr);
                        CPPUNIT_ASSERT(!TrackingScriptingService::destroyed);
                }
                CPPUNIT_ASSERT(TrackingScriptingService::destroyed);
        }
};

}

CPPUNIT_TEST_SUITE_REGISTRATION(Ember::EmberServicesTestCase);

int main(int argc, char** argv) {
        CppUnit::TextUi::TestRunner runner;
        CppUnit::TestFactoryRegistry& registry = CppUnit::TestFactoryRegistry::getRegistry();
        runner.addTest(registry.makeTest());

        CppUnit::BriefTestProgressListener listener;
        runner.eventManager().addListener(&listener);

        bool wasSuccessful = runner.run("", false);
        return !wasSuccessful;
}
