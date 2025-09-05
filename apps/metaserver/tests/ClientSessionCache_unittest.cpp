/**
 * Verify that DataObject::expireClientSessionCache removes expired entries
 */
#define private public
#include "../src/server/DataObject.hpp"
#undef private

#include <cppunit/TestCase.h>
#include <cppunit/TestRunner.h>
#include <cppunit/TextTestRunner.h>
#include <cppunit/TestListener.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>

#include <algorithm>

class ClientSessionCache_unittest : public CppUnit::TestFixture {
CPPUNIT_TEST_SUITE(ClientSessionCache_unittest);
    CPPUNIT_TEST(test_ExpireClientSessionCache);
CPPUNIT_TEST_SUITE_END();

private:
    DataObject* msdo;
public:
    void setUp() override {
        msdo = new DataObject();
    }
    void tearDown() override {
        delete msdo;
    }

    void test_ExpireClientSessionCache() {
        msdo->addServerSession("server1");
        msdo->createServerSessionListresp("client1");
        boost::posix_time::ptime old = DataObject::getNow() - boost::posix_time::seconds(10);
        msdo->m_listreqExpiry["client1"] = boost::posix_time::to_iso_string(old);
        std::vector<std::string> expired = msdo->expireClientSessionCache(5);
        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), expired.size());
        CPPUNIT_ASSERT(std::find(expired.begin(), expired.end(), std::string("client1")) != expired.end());
        CPPUNIT_ASSERT(msdo->getServerSessionCacheList().empty());
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION(ClientSessionCache_unittest);

int main(int argc, char** argv) {
    CppUnit::TextTestRunner runner;
    runner.addTest(CppUnit::TestFactoryRegistry::getRegistry().makeTest());
    bool wasSuccessful = runner.run();
    return wasSuccessful ? 0 : 1;
}
