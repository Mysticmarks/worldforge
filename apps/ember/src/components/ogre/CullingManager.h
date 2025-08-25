#pragma once

#include "OgreIncludes.h"
#include <OgreAxisAlignedBox.h>
#include <OgreCamera.h>

namespace Ember::OgreView {

/**
 * Simple frustum and occlusion culling helper.
 * Currently only performs frustum culling using the provided camera.
 * Occlusion culling is left as a placeholder for future GPU queries.
 */
class CullingManager {
public:
    explicit CullingManager(Ogre::Camera* camera = nullptr);

    /** Set the camera used for visibility checks. */
    void setCamera(Ogre::Camera* camera);

    /**
     * Checks if the bounding box is visible from the current camera frustum.
     * @return true if visible or no camera is set.
     */
    bool isVisible(const Ogre::AxisAlignedBox& bounds) const;

    /**
     * Placeholder for occlusion culling. Returns false until an occlusion
     * query implementation is provided.
     */
    bool isOccluded(const Ogre::AxisAlignedBox& bounds) const;

private:
    Ogre::Camera* mCamera;
};

} // namespace Ember::OgreView

