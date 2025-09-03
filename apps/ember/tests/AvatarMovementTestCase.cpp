#include "AvatarMovementTestCase.h"
#include "components/ogre/Avatar.h"

#include <Atlas/Message/Element.h>
#include <Eris/Entity.h>
#include <wfmath/point.h>
#include <wfmath/vector.h>

using namespace Ember::OgreView;

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

void AvatarMovementTestCase::testGroundMovement() {
        DummyEntity entity;
        entity.setAttr("surface", Atlas::Message::Element("ground"));
        entity.setAttr("speed-ground", Atlas::Message::Element(2.0));
        WFMath::Point<3> pos(0, 0, 0);
        WFMath::Vector<3> move(1, 0, 0);
        float timeslice = 1.0f;
        float speed = getSurfaceSpeedModifier(entity);
        pos += move * timeslice * speed;
        CPPUNIT_ASSERT_DOUBLES_EQUAL(2.0, pos.x(), 0.0001);
}

void AvatarMovementTestCase::testMudMovement() {
        DummyEntity entity;
        entity.setAttr("surface", Atlas::Message::Element("mud"));
        entity.setAttr("speed-mud", Atlas::Message::Element(0.5));
        WFMath::Point<3> pos(0, 0, 0);
        WFMath::Vector<3> move(1, 0, 0);
        float timeslice = 1.0f;
        float speed = getSurfaceSpeedModifier(entity);
        pos += move * timeslice * speed;
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.5, pos.x(), 0.0001);
}

} // namespace Ember
