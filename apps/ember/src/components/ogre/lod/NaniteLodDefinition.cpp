#include "NaniteLodDefinition.h"

namespace Ember::OgreView::Lod {

NaniteLodDefinition::NaniteLodDefinition(Ogre::ResourceManager* creator,
                                         const Ogre::String& name,
                                         Ogre::ResourceHandle handle,
                                         const Ogre::String& group,
                                         bool isManual,
                                         Ogre::ManualResourceLoader* loader)
    : LodDefinition(creator, name, handle, group, isManual, loader) {}

void NaniteLodDefinition::addCluster(Cluster cluster) {
    mClusters.push_back(std::move(cluster));
}

} // namespace Ember::OgreView::Lod

