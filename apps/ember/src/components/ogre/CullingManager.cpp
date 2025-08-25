#include "CullingManager.h"

namespace Ember::OgreView {

CullingManager::CullingManager(Ogre::Camera* camera) : mCamera(camera) {}

void CullingManager::setCamera(Ogre::Camera* camera) { mCamera = camera; }

bool CullingManager::isVisible(const Ogre::AxisAlignedBox& bounds) const {
    if (!mCamera) {
        return true;
    }
    return mCamera->isVisible(bounds);
}

bool CullingManager::isOccluded(const Ogre::AxisAlignedBox& bounds) const {
    (void)bounds; // unused until occlusion queries are implemented
    return false;
}

} // namespace Ember::OgreView

