#include <cppunit/extensions/HelperMacros.h>

namespace Ember {
class NestedEntityTestCase : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(NestedEntityTestCase);
        CPPUNIT_TEST(testWorldToEntityCoords);
        CPPUNIT_TEST_SUITE_END();
public:
        void testWorldToEntityCoords();
};
}
