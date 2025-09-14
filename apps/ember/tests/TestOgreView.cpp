#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

#include "ConvertTestCase.h"
#include "ModelMountTestCase.h"
#include "NestedEntityTestCase.h"

CPPUNIT_TEST_SUITE_REGISTRATION( Ember::ConvertTestCase);
CPPUNIT_TEST_SUITE_REGISTRATION( Ember::ModelMountTestCase );
CPPUNIT_TEST_SUITE_REGISTRATION( Ember::NestedEntityTestCase );

#include "AvatarMovementTestCase.h"
#include "EntityMoverRotationTestCase.h"

CPPUNIT_TEST_SUITE_REGISTRATION( Ember::ConvertTestCase);
CPPUNIT_TEST_SUITE_REGISTRATION( Ember::ModelMountTestCase );
CPPUNIT_TEST_SUITE_REGISTRATION( Ember::AvatarMovementTestCase );
CPPUNIT_TEST_SUITE_REGISTRATION( Ember::EntityMoverRotationTestCase );

int main(int argc, char** argv) {
	CppUnit::TextUi::TestRunner runner;
	CppUnit::TestFactoryRegistry& registry = CppUnit::TestFactoryRegistry::getRegistry();
	runner.addTest(registry.makeTest());

	bool wasSuccessful = runner.run("", false);
	return !wasSuccessful;
}

