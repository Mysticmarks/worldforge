#include "InstancingManager.h"

#include <OgreSceneManager.h>
#include <OgreSceneNode.h>

namespace Ember::OgreView {

InstancingManager::InstancingManager() = default;

void InstancingManager::registerMovable(Ogre::MovableObject* obj) {
    auto* entity = dynamic_cast<Ogre::Entity*>(obj);
    if (entity) {
        mEntries.push_back({entity, nullptr});
    }
}

void InstancingManager::update() {
    if (mEntries.empty()) {
        return;
    }

    if (!mInstanceManager) {
        Ogre::Entity* base = mEntries.front().source;
        Ogre::SceneManager* sceneMgr = base->getSceneManager();
        const Ogre::String& meshName = base->getMesh()->getName();
        mInstanceManager = sceneMgr->createInstanceManager(
                meshName + "/InstancingManager", meshName, base->getMesh()->getGroup(),
                Ogre::InstanceManager::HWInstancingBasic);
    }

    for (auto& entry : mEntries) {
        if (!entry.instanced) {
            const Ogre::String& matName = entry.source->getSubEntity(0)->getMaterialName();
            entry.instanced = mInstanceManager->createInstancedEntity(matName);
            if (auto* node = entry.source->getParentSceneNode()) {
                node->attachObject(entry.instanced);
            }
            entry.source->setVisible(false);
        }
    }

    if (mInstanceManager) {
        mInstanceManager->_updateDirtyBatches();
    }
}

} // namespace Ember::OgreView

