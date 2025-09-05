#include "DomainFilter.hpp"

#include <cppunit/TestCase.h>
#include <cppunit/TestRunner.h>
#include <cppunit/TextOutputter.h>
#include <cppunit/TextTestRunner.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

class DomainFilter_unittest : public CppUnit::TestFixture {
CPPUNIT_TEST_SUITE(DomainFilter_unittest);
        CPPUNIT_TEST(testAllowedDomain);
        CPPUNIT_TEST(testDisallowedDomain);
CPPUNIT_TEST_SUITE_END();
public:
        void testAllowedDomain() {
                std::string name = "foo.ms.worldforge.org";
                std::vector<std::string> whitelist = {"ms.worldforge.org"};
                bool result = filterDomain(name, whitelist);
                CPPUNIT_ASSERT(result);
                CPPUNIT_ASSERT_EQUAL(std::string("foo"), name);
        }
        void testDisallowedDomain() {
                std::string name = "foo.example.com";
                std::vector<std::string> whitelist = {"ms.worldforge.org"};
                bool result = filterDomain(name, whitelist);
                CPPUNIT_ASSERT(!result);
                CPPUNIT_ASSERT_EQUAL(std::string("foo.example.com"), name);
        }
};

CPPUNIT_TEST_SUITE_REGISTRATION(DomainFilter_unittest);

int main() {
        CppUnit::TextTestRunner runner;
        CppUnit::Test* tp =
                        CppUnit::TestFactoryRegistry::getRegistry().makeTest();

        runner.addTest(tp);

        if (runner.run()) {
                return 0;
        } else {
                return 1;
        }
}

