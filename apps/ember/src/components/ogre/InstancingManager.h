#pragma once

#include "OgreIncludes.h"
#include <OgreEntity.h>
#include <OgreInstanceManager.h>
#include <OgreInstancedEntity.h>
#include <vector>

namespace Ember::OgreView {

/**
 * Lightweight manager for GPU instancing/indirect drawing.
 * The current implementation only stores registered objects and iterates
 * over them during update; rendering backends may extend this to submit
 * GPU instance buffers.
 */
class InstancingManager {
public:
    InstancingManager();

    /** Register a movable object for instanced rendering. */
    void registerMovable(Ogre::MovableObject* obj);

    /** Build and upload instance buffers. */
    void update();

private:
    struct Entry {
        Ogre::Entity* source;
        Ogre::InstancedEntity* instanced{nullptr};
    };

    std::vector<Entry> mEntries;
    Ogre::InstanceManager* mInstanceManager{nullptr};
};

} // namespace Ember::OgreView

