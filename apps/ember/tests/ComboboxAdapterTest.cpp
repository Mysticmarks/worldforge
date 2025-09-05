#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/TestResult.h>

#include "components/ogre/widgets/adapters/ComboboxAdapter.h"

namespace Ember {

class ComboboxAdapterTestCase : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(ComboboxAdapterTestCase);
        CPPUNIT_TEST(testThrowsOnInvalidWidget);
        CPPUNIT_TEST_SUITE_END();

public:
        void testThrowsOnInvalidWidget() {
                CEGUI::Window* notACombobox = nullptr;
                CPPUNIT_ASSERT_THROW(
                                OgreView::Gui::Adapters::ComboboxAdapter<std::string, std::string> adapter("", notACombobox),
                                Exception);
        }
};

}

CPPUNIT_TEST_SUITE_REGISTRATION(Ember::ComboboxAdapterTestCase);

int main(int argc, char** argv) {
        CppUnit::TextUi::TestRunner runner;
        CppUnit::TestFactoryRegistry& registry = CppUnit::TestFactoryRegistry::getRegistry();
        runner.addTest(registry.makeTest());

        CppUnit::BriefTestProgressListener listener;
        runner.eventManager().addListener(&listener);

        bool wasSuccessful = runner.run("", false);
        return !wasSuccessful;
}

