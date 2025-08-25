#pragma once

#include "LodDefinition.h"
#include <OgreAxisAlignedBox.h>
#include <vector>

namespace Ember::OgreView::Lod {

/**
 * @brief LOD definition storing Nanite style cluster hierarchy.
 */
class NaniteLodDefinition : public LodDefinition {
public:
    struct Cluster {
        Ogre::AxisAlignedBox bounds;
        std::vector<unsigned int> indices;
        std::vector<size_t> children;
        Cluster() { bounds.setNull(); }
    };

    NaniteLodDefinition(Ogre::ResourceManager* creator,
                        const Ogre::String& name,
                        Ogre::ResourceHandle handle,
                        const Ogre::String& group,
                        bool isManual = false,
                        Ogre::ManualResourceLoader* loader = nullptr);

    /// Add a cluster to the definition
    void addCluster(Cluster cluster);

    /// Access stored clusters
    const std::vector<Cluster>& getClusters() const;

private:
    std::vector<Cluster> mClusters;
};

using NaniteLodDefinitionPtr = Ogre::SharedPtr<NaniteLodDefinition>;

inline const std::vector<NaniteLodDefinition::Cluster>& NaniteLodDefinition::getClusters() const {
    return mClusters;
}

} // namespace Ember::OgreView::Lod

