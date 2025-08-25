#include "MeshLodComponent.h"

namespace Ember::OgreView {

MeshLodComponent::MeshLodComponent(Ogre::Entity* entity) : mEntity(entity) {}

void MeshLodComponent::setEntity(Ogre::Entity* entity) { mEntity = entity; }

void MeshLodComponent::setMeshLod(size_t lod) {
    if (mEntity) {
        mEntity->setMeshLodBias(1.0f, static_cast<Ogre::ushort>(lod));
    }
}

void MeshLodComponent::setAnimationLod(float bias) {
    if (mEntity) {
        mEntity->setAnimationLodBias(bias);
    }
}

} // namespace Ember::OgreView

