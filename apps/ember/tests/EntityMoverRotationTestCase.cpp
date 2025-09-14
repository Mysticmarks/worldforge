#include "EntityMoverRotationTestCase.h"
#include "components/ogre/authoring/EntityMoverBase.h"
#include "components/ogre/Convert.h"
#include <Ogre.h>
#include <Eris/Entity.h>
#include <Atlas/Message/Element.h>
#include <wfmath/numeric_constants.h>

using namespace Ember::OgreView;
using namespace Ember::OgreView::Authoring;

namespace Ember {

class DummyEntity : public Eris::Entity {
public:
        DummyEntity() : Eris::Entity("0", nullptr) {}
        Eris::TypeService* getTypeService() const override { return nullptr; }
        void removeFromMovementPrediction() override {}
        void addToMovementPrediction() override {}
        Eris::Entity* getEntity(const std::string& /*id*/) override { return nullptr; }
        void setAttr(const std::string& p, const Atlas::Message::Element& v) override {
                Eris::Entity::setAttr(p, v);
        }
        void setFromMessage(const Atlas::Message::MapType& attrs) override {
                Eris::Entity::setFromMessage(attrs);
        }
};

class TestMover : public EntityMoverBase {
public:
        TestMover(Eris::Entity* entity, Ogre::Node* node, Ogre::SceneManager& sm)
                : EntityMoverBase(entity, node, sm) {}
        void finalizeMovement() override {}
        void cancelMovement() override {}
};

void EntityMoverRotationTestCase::testRotationAroundAxis() {
        Ogre::Root root;
        auto* sceneManager = root.createSceneManager(Ogre::DefaultSceneManagerFactory::FACTORY_TYPE_NAME);
        auto* node = sceneManager->getRootSceneNode()->createChildSceneNode();

        DummyEntity entity;
        TestMover mover(&entity, node, *sceneManager);

        WFMath::CoordType angle = WFMath::numeric_constants<WFMath::CoordType>::pi() / 2;
        mover.setRotation(1, angle);

        WFMath::Quaternion expected;
        expected.rotation(1, angle);
        auto orientation = mover.getOrientation();
        CPPUNIT_ASSERT(orientation.isEqualTo(expected, 0.0001));
}

} // namespace Ember
