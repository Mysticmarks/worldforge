#pragma once

#include "OgreIncludes.h"
#include <OgreEntity.h>

namespace Ember::OgreView {

/**
 * Component handling mesh and animation level-of-detail selection.
 * For now it simply forwards desired LOD levels to Ogre entities.
 */
class MeshLodComponent {
public:
    explicit MeshLodComponent(Ogre::Entity* entity = nullptr);

    void setEntity(Ogre::Entity* entity);

    /**
     * Apply a mesh LOD level.
     * @param lod Index of the mesh LOD to use.
     */
    void setMeshLod(size_t lod);

    /**
     * Apply animation LOD bias.
     * @param bias LOD bias factor.
     */
    void setAnimationLod(float bias);

private:
    Ogre::Entity* mEntity;
};

} // namespace Ember::OgreView

