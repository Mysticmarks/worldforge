#include <Atlas/Objects/Decoder.h>
#include <Atlas/Objects/Operation.h>
#include <Atlas/Objects/Entity.h>
#include <Atlas/Objects/Root.h>
#include "loadDefaults.h"

#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>

#include <iostream>

class Decoder1TestCase : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(Decoder1TestCase);
        CPPUNIT_TEST(testRootNotArrived);
        CPPUNIT_TEST(testAnonymousArrived);
        CPPUNIT_TEST(testLookArrived);
        CPPUNIT_TEST(testAccountNotArrived);
        CPPUNIT_TEST(testUnknownNotArrived);
        CPPUNIT_TEST_SUITE_END();

        bool root_arrived;
        bool look_arrived;
        bool acct_arrived;
        bool anonymous_arrived;
        bool unknown_arrived;

        struct TestDecoder : public Atlas::Objects::ObjectsDecoder {
                TestDecoder(Atlas::Objects::Factories& factories,
                            bool& root_arrived,
                            bool& look_arrived,
                            bool& acct_arrived,
                            bool& anonymous_arrived,
                            bool& unknown_arrived)
                        : ObjectsDecoder(factories),
                          root_arrived(root_arrived),
                          look_arrived(look_arrived),
                          acct_arrived(acct_arrived),
                          anonymous_arrived(anonymous_arrived),
                          unknown_arrived(unknown_arrived) {}

                void objectArrived(Atlas::Objects::Root obj) override {
                        if (obj) {
                                switch (obj->getClassNo()) {
                                        case Atlas::Objects::ROOT_NO:
                                                CPPUNIT_ASSERT(obj->getAttr("id").asString() == "root_instance");
                                                root_arrived = true;
                                                break;
                                        case Atlas::Objects::Operation::LOOK_NO:
                                                CPPUNIT_ASSERT(obj->getAttr("id").asString() == "look_instance");
                                                look_arrived = true;
                                                break;
                                        case Atlas::Objects::Entity::ACCOUNT_NO:
                                                acct_arrived = true;
                                                break;
                                        case Atlas::Objects::Entity::ANONYMOUS_NO:
                                                anonymous_arrived = true;
                                                break;
                                        default:
                                                unknown_arrived = true;
                                }
                        }
                }

        private:
                bool& root_arrived;
                bool& look_arrived;
                bool& acct_arrived;
                bool& anonymous_arrived;
                bool& unknown_arrived;
        };

public:
        void setUp() override {
                root_arrived = false;
                look_arrived = false;
                acct_arrived = false;
                anonymous_arrived = false;
                unknown_arrived = false;

                Atlas::Objects::Factories factories;
                try {
                        Atlas::Objects::loadDefaults(TEST_ATLAS_XML_PATH, factories);
                } catch (const Atlas::Objects::DefaultLoadingException& e) {
                        std::cout << "DefaultLoadingException: " << e.what() << std::endl;
                }

                TestDecoder t(factories, root_arrived, look_arrived, acct_arrived, anonymous_arrived, unknown_arrived);
                t.streamBegin();
                t.streamMessage();
                t.mapStringItem("parent", "");
                t.mapStringItem("id", "foo");
                t.mapEnd();
                t.streamMessage();
                t.mapStringItem("objtype", "op");
                t.mapStringItem("parent", "look");
                t.mapStringItem("id", "look_instance");
                t.mapEnd();
                t.streamEnd();
        }

        void testRootNotArrived() { CPPUNIT_ASSERT(!root_arrived); }
        void testAnonymousArrived() { CPPUNIT_ASSERT(anonymous_arrived); }
        void testLookArrived() { CPPUNIT_ASSERT(look_arrived); }
        void testAccountNotArrived() { CPPUNIT_ASSERT(!acct_arrived); }
        void testUnknownNotArrived() { CPPUNIT_ASSERT(!unknown_arrived); }
};

CPPUNIT_TEST_SUITE_REGISTRATION(Decoder1TestCase);

int main(int argc, char** argv) {
        CppUnit::TextUi::TestRunner runner;
        CppUnit::TestFactoryRegistry& registry = CppUnit::TestFactoryRegistry::getRegistry();
        runner.addTest(registry.makeTest());
        bool wasSuccessful = runner.run("", false);
        return wasSuccessful ? 0 : 1;
}

