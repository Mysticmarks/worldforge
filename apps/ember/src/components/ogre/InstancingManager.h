#pragma once

#include "OgreIncludes.h"
#include <OgreMovableObject.h>
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

    /** Update instancing buffers (placeholder). */
    void update();

private:
    std::vector<Ogre::MovableObject*> mRegistered;
};

} // namespace Ember::OgreView

