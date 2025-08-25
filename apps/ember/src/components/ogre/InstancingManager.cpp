#include "InstancingManager.h"

namespace Ember::OgreView {

InstancingManager::InstancingManager() = default;

void InstancingManager::registerMovable(Ogre::MovableObject* obj) {
    if (obj) {
        mRegistered.push_back(obj);
    }
}

void InstancingManager::update() {
    // Placeholder: in a full implementation this would build GPU instance
    // buffers and issue indirect draw calls. For now we simply ensure that
    // the objects remain valid.
    for (auto* obj : mRegistered) {
        (void)obj;
    }
}

} // namespace Ember::OgreView

