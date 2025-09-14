#include <cppunit/extensions/HelperMacros.h>

namespace Ember {
class EntityMoverRotationTestCase : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(EntityMoverRotationTestCase);
        CPPUNIT_TEST(testRotationAroundAxis);
        CPPUNIT_TEST_SUITE_END();
public:
        void testRotationAroundAxis();
};
}
