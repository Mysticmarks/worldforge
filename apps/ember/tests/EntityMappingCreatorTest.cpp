#include <cppunit/extensions/TestFactoryRegistry.h>
#include <cppunit/ui/text/TestRunner.h>
#include <cppunit/extensions/HelperMacros.h>
#include <cppunit/BriefTestProgressListener.h>
#include <cppunit/TestResult.h>

#include "components/entitymapping/EntityMappingCreator.h"
#include "components/entitymapping/Definitions/EntityMappingDefinition.h"
#include "components/entitymapping/Definitions/MatchDefinition.h"
#include "components/entitymapping/Definitions/CaseDefinition.h"
#include "components/entitymapping/Cases/CaseBase.h"
#include "components/entitymapping/Matches/VirtualAttributeMatch.h"
#include "components/entitymapping/Cases/AttributeComparers/HeightComparerWrapper.h"
#include "components/entitymapping/IActionCreator.h"

#include <Eris/Entity.h>
#include <Eris/Connection.h>
#include <Eris/EventService.h>

#include <boost/asio/io_context.hpp>

namespace Ember::EntityMapping {

namespace {
class DummyActionCreator : public IActionCreator {
public:
        void createActions(EntityMapping&, Cases::CaseBase&, Definitions::CaseDefinition&) override {}
};

class TestableEntityMappingCreator : public EntityMappingCreator {
public:
        TestableEntityMappingCreator(Definitions::EntityMappingDefinition& definition,
                                                                 Eris::Entity& entity,
                                                                 IActionCreator& actionCreator,
                                                                 Eris::TypeService& typeService)
                        : EntityMappingCreator(definition, entity, actionCreator, typeService, nullptr) {}

        using EntityMappingCreator::addAttributeMatch;
        using EntityMappingCreator::getAttributeCaseComparer;
};
}

class EntityMappingCreatorTestCase : public CppUnit::TestFixture {
        CPPUNIT_TEST_SUITE(EntityMappingCreatorTestCase);
        CPPUNIT_TEST(testHeightFunctionComparer);
        CPPUNIT_TEST(testHeightFunctionMatchCreation);
        CPPUNIT_TEST(testAlternateFunctionAttribute);
        CPPUNIT_TEST(testUnsupportedFunctionAttribute);
        CPPUNIT_TEST_SUITE_END();

public:
        void setUp() override {
                mEventService = std::make_unique<Eris::EventService>(mIoContext);
                mConnection = std::make_unique<Eris::Connection>(mIoContext, *mEventService, "testclient", "localhost", 6767);
                mTypeService = &mConnection->getTypeService();
                Eris::Entity::EntityContext context{};
                mEntity = std::make_unique<Eris::Entity>("entity", nullptr, context);
        }

        void tearDown() override {
                mEntity.reset();
                mConnection.reset();
                mEventService.reset();
                mTypeService = nullptr;
        }

        void testHeightFunctionComparer() {
                Definitions::EntityMappingDefinition definition;
                TestableEntityMappingCreator creator(definition, *mEntity, mActionCreator, *mTypeService);

                Definitions::MatchDefinition matchDefinition;
                matchDefinition.Properties["type"] = "function";
                matchDefinition.Properties["attribute"] = "height";

                Definitions::CaseDefinition caseDefinition;
                caseDefinition.Parameters.emplace_back("equals", "2.0");

                Matches::VirtualAttributeMatch match("height", std::initializer_list<std::string>{"bbox", "scale"});
                auto comparer = creator.getAttributeCaseComparer(&match, matchDefinition, caseDefinition);
                CPPUNIT_ASSERT(comparer);
                CPPUNIT_ASSERT(dynamic_cast<Cases::AttributeComparers::HeightComparerWrapper*>(comparer.get()));
        }

        void testHeightFunctionMatchCreation() {
                Definitions::EntityMappingDefinition definition;
                TestableEntityMappingCreator creator(definition, *mEntity, mActionCreator, *mTypeService);

                Definitions::MatchDefinition matchDefinition;
                matchDefinition.Properties["type"] = "function";
                matchDefinition.Properties["attribute"] = "height";

                Definitions::CaseDefinition caseDefinition;
                caseDefinition.Parameters.emplace_back("equals", "1.0");
                matchDefinition.Cases.push_back(caseDefinition);

                Cases::CaseBase caseBase;
                creator.addAttributeMatch(caseBase, matchDefinition);

                CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), caseBase.getMatches().size());
                auto* virtualMatch = dynamic_cast<Matches::VirtualAttributeMatch*>(caseBase.getMatches().front().get());
                CPPUNIT_ASSERT(virtualMatch);
                CPPUNIT_ASSERT_EQUAL(std::string("height"), virtualMatch->getAttributeName());
        }

        void testAlternateFunctionAttribute() {
                Definitions::EntityMappingDefinition definition;
                TestableEntityMappingCreator creator(definition, *mEntity, mActionCreator, *mTypeService);

                Definitions::MatchDefinition matchDefinition;
                matchDefinition.Properties["type"] = "function";
                matchDefinition.Properties["attribute"] = "verticalextent";

                Definitions::CaseDefinition caseDefinition;
                caseDefinition.Parameters.emplace_back("equals", "3.5");

                Matches::VirtualAttributeMatch match("verticalextent", std::initializer_list<std::string>{"bbox", "scale"});
                auto comparer = creator.getAttributeCaseComparer(&match, matchDefinition, caseDefinition);
                CPPUNIT_ASSERT(comparer);
                CPPUNIT_ASSERT(dynamic_cast<Cases::AttributeComparers::HeightComparerWrapper*>(comparer.get()));

                matchDefinition.Cases.push_back(caseDefinition);
                Cases::CaseBase caseBase;
                creator.addAttributeMatch(caseBase, matchDefinition);
                CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), caseBase.getMatches().size());
                auto* virtualMatch = dynamic_cast<Matches::VirtualAttributeMatch*>(caseBase.getMatches().front().get());
                CPPUNIT_ASSERT(virtualMatch);
                CPPUNIT_ASSERT_EQUAL(std::string("verticalextent"), virtualMatch->getAttributeName());
        }

        void testUnsupportedFunctionAttribute() {
                Definitions::EntityMappingDefinition definition;
                TestableEntityMappingCreator creator(definition, *mEntity, mActionCreator, *mTypeService);

                Definitions::MatchDefinition matchDefinition;
                matchDefinition.Properties["type"] = "function";
                matchDefinition.Properties["attribute"] = "unsupported";

                Definitions::CaseDefinition caseDefinition;
                caseDefinition.Parameters.emplace_back("equals", "1.0");
                matchDefinition.Cases.push_back(caseDefinition);

                Cases::CaseBase caseBase;
                creator.addAttributeMatch(caseBase, matchDefinition);
                CPPUNIT_ASSERT(caseBase.getMatches().empty());

                Matches::VirtualAttributeMatch match("unsupported", std::initializer_list<std::string>{});
                auto comparer = creator.getAttributeCaseComparer(&match, matchDefinition, matchDefinition.Cases.front());
                CPPUNIT_ASSERT(!comparer);
        }

private:
        boost::asio::io_context mIoContext;
        std::unique_ptr<Eris::EventService> mEventService;
        std::unique_ptr<Eris::Connection> mConnection;
        Eris::TypeService* mTypeService{nullptr};
        std::unique_ptr<Eris::Entity> mEntity;
        DummyActionCreator mActionCreator;
};

}

CPPUNIT_TEST_SUITE_REGISTRATION(Ember::EntityMapping::EntityMappingCreatorTestCase);

int main(int argc, char** argv) {
        CppUnit::TextUi::TestRunner runner;
        CppUnit::TestFactoryRegistry& registry = CppUnit::TestFactoryRegistry::getRegistry();
        runner.addTest(registry.makeTest());

        CppUnit::BriefTestProgressListener listener;
        runner.eventManager().addListener(&listener);

        bool wasSuccessful = runner.run("", false);
        return !wasSuccessful;
}
