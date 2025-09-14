#pragma once

#include "OgreIncludes.h"
#include <OgreAxisAlignedBox.h>
#include <OgreCamera.h>
#include <memory>

namespace Ogre {
class HardwareOcclusionQuery;
class VertexData;
class IndexData;
}

namespace Ember::OgreView {

/**
 * Simple frustum and occlusion culling helper.
 * Frustum culling uses the provided camera while occlusion culling relies on
 * GPU hardware queries to test visibility of bounding boxes.
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
     * Uses a GPU occlusion query to determine if the bounding box is hidden
     * by previously rendered geometry.
     * @return true if the query reports zero visible fragments.
     */
    bool isOccluded(const Ogre::AxisAlignedBox& bounds) const;

    ~CullingManager();

private:
    Ogre::Camera* mCamera;

    /** Hardware occlusion query object used for visibility tests. */
    mutable Ogre::HardwareOcclusionQuery* mQuery{nullptr};

    /** Static vertex/index data representing a unit cube used for queries. */
    mutable Ogre::VertexData* mVertexData{nullptr};
    mutable Ogre::IndexData* mIndexData{nullptr};

    /** Ensure occlusion query resources are created. */
    void ensureQueryObjects() const;
};

} // namespace Ember::OgreView

