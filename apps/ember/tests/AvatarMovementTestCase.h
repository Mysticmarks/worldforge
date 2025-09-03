#include <cppunit/extensions/HelperMacros.h>

namespace Ember {
class AvatarMovementTestCase : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(AvatarMovementTestCase);
        CPPUNIT_TEST(testGroundMovement);
        CPPUNIT_TEST(testMudMovement);
        CPPUNIT_TEST_SUITE_END();
public:
        void testGroundMovement();
        void testMudMovement();
};
}
