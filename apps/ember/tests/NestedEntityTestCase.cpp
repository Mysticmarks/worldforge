#include "NestedEntityTestCase.h"
#include "components/ogre/Avatar.h"
#include <Eris/Entity.h>
#include <Eris/Task.h>
#include <wfmath/atlasconv.h>
#include <wfmath/point.h>

using namespace Ember::OgreView;
using namespace WFMath;

namespace Ember {
void NestedEntityTestCase::testWorldToEntityCoords() {
        Eris::Entity::EntityContext context{};
        context.fetchEntity = [](const std::string&) -> Eris::Entity* { return nullptr; };
        context.getEntity = [](const std::string&) -> Eris::Entity* { return nullptr; };
        context.taskUpdated = [](Eris::Task&) {};

        Eris::Entity root{"root", nullptr, context};
        root.setProperty("pos", Point<3>(0,0,0).toAtlas());

        Eris::Entity parent{"parent", nullptr, context};
        parent.setProperty("pos", Point<3>(10,0,0).toAtlas());
        parent.setLocation(&root);

        Eris::Entity child{"child", nullptr, context};
        child.setProperty("pos", Point<3>(0,0,5).toAtlas());
        child.setLocation(&parent);

        Point<3> worldPos(15,0,7);
        auto childLocal = Avatar::worldToEntityCoords(worldPos, child);
        CPPUNIT_ASSERT(childLocal == Point<3>(5,0,2));

        auto parentLocal = Avatar::worldToEntityCoords(worldPos, parent);
        CPPUNIT_ASSERT(parentLocal == Point<3>(5,0,7));
}
}
