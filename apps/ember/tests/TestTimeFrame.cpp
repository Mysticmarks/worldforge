#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/TestResult.h>

#include "framework/TimeFrame.h"

#include <chrono>
#include <thread>

namespace Ember {

class TimeFrameTestCase : public CppUnit::TestFixture {
	CPPUNIT_TEST_SUITE(TimeFrameTestCase);
        CPPUNIT_TEST(testElapsedTime);
        CPPUNIT_TEST(testRemainingTime);
        CPPUNIT_TEST(testRemainingTimeNonPositiveAfterExpiry);

        CPPUNIT_TEST_SUITE_END();

public:
        void testElapsedTime() {
                // Elapsed time should not under-report by more than 1ms.
                TimeFrame tested = TimeFrame(std::chrono::milliseconds(10));
                auto start = std::chrono::steady_clock::now();
                std::this_thread::sleep_for(std::chrono::milliseconds(5));

                auto local_elapsed = (std::chrono::steady_clock::now() - start);
                auto elapsed = tested.getElapsedTime();
                CPPUNIT_ASSERT(elapsed + std::chrono::milliseconds(1) >= local_elapsed);
        }

        void testRemainingTime() {
                // After half the frame duration, remaining time should be at most half with 1ms tolerance.
                TimeFrame tested = TimeFrame(std::chrono::milliseconds(10));
                std::this_thread::sleep_for(std::chrono::milliseconds(5));

                auto remaining = tested.getRemainingTime();
                CPPUNIT_ASSERT(remaining <= std::chrono::milliseconds(5) + std::chrono::milliseconds(1));
        }

        void testRemainingTimeNonPositiveAfterExpiry() {
                // Once the frame has expired there should be no positive remaining time (allow 1ms tolerance).
                TimeFrame tested = TimeFrame(std::chrono::milliseconds(2));
                std::this_thread::sleep_for(std::chrono::milliseconds(5));

                CPPUNIT_ASSERT(tested.getRemainingTime() <= std::chrono::milliseconds(1));
        }

};

}

CPPUNIT_TEST_SUITE_REGISTRATION( Ember::TimeFrameTestCase);

int main(int argc, char** argv) {
	CppUnit::TextUi::TestRunner runner;
	CppUnit::TestFactoryRegistry& registry = CppUnit::TestFactoryRegistry::getRegistry();
	runner.addTest(registry.makeTest());

	// Shows a message as each test starts
	CppUnit::BriefTestProgressListener listener;
	runner.eventManager().addListener(&listener);

	bool wasSuccessful = runner.run("", false);
	return !wasSuccessful;
}

