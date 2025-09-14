#include "InstancingRenderTestCase.h"
#include "components/ogre/InstancingManager.h"
#include "components/ogre/SimpleRenderContext.h"

#include <Ogre.h>
#include <vector>

using namespace Ember::OgreView;

namespace Ember {

void InstancingRenderTestCase::testInstancedBoxes() {
    Ogre::Root root;
    Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
        "../../../../apps/ember/data/assets", "FileSystem");
    Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

    SimpleRenderContext ctx("InstancingTest", 64, 64);
    Ogre::SceneManager* sm = ctx.getSceneManager();
    Ogre::SceneNode* rootNode = ctx.getSceneNode();

    Ogre::Entity* e1 = sm->createEntity("box1", "common/primitives/model/box.mesh");
    Ogre::SceneNode* n1 = rootNode->createChildSceneNode();
    n1->attachObject(e1);
    n1->setPosition(0, 0, 0);

    Ogre::Entity* e2 = sm->createEntity("box2", "common/primitives/model/box.mesh");
    Ogre::SceneNode* n2 = rootNode->createChildSceneNode();
    n2->attachObject(e2);
    n2->setPosition(10, 0, 0);

    InstancingManager inst;
    inst.registerMovable(e1);
    inst.registerMovable(e2);
    inst.update();

    ctx.getRenderTexture()->update();

    auto iter = sm->getMovableObjectIterator("InstancedEntity");
    std::vector<Ogre::Vector3> positions;
    while (iter.hasMoreElements()) {
        auto* mo = iter.getNext();
        auto* instEnt = static_cast<Ogre::InstancedEntity*>(mo);
        positions.push_back(instEnt->getParentNode()->_getDerivedPosition());
    }

    CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(2), positions.size());
    bool foundOrigin = false;
    bool foundOffset = false;
    for (const auto& p : positions) {
        if (p == Ogre::Vector3(0, 0, 0)) {
            foundOrigin = true;
        }
        if (p == Ogre::Vector3(10, 0, 0)) {
            foundOffset = true;
        }
    }
    CPPUNIT_ASSERT(foundOrigin && foundOffset);
}

} // namespace Ember

