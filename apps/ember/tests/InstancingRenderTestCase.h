#pragma once

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

namespace Ember {
class InstancingRenderTestCase : public CppUnit::TestFixture {
    CPPUNIT_TEST_SUITE(InstancingRenderTestCase);
    CPPUNIT_TEST(testInstancedBoxes);
    CPPUNIT_TEST_SUITE_END();
public:
    void testInstancedBoxes();
};
} // namespace Ember

