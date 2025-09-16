#ifndef EMBER_MOVEMENTCONTROLLERTESTCASE_H
#define EMBER_MOVEMENTCONTROLLERTESTCASE_H

#include <cppunit/extensions/HelperMacros.h>

namespace Ember {

class MovementControllerTestCase : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(MovementControllerTestCase);
        CPPUNIT_TEST(testProjectsPointOntoTerrain);
        CPPUNIT_TEST(testKeepsOriginalHeightWhenLookupFails);
        CPPUNIT_TEST_SUITE_END();

public:
        void testProjectsPointOntoTerrain();
        void testKeepsOriginalHeightWhenLookupFails();
};

}

#endif // EMBER_MOVEMENTCONTROLLERTESTCASE_H
