#pragma once
#include <string>
#include <vector>
#include "OgreAxisAlignedBox.h"

namespace Ogre {
class ResourceManager;
using String = std::string;
using ResourceHandle = unsigned long long;
class ManualResourceLoader;
}

namespace Ember::OgreView::Lod {
class LodDefinition {};
class NaniteLodDefinition : public LodDefinition {
public:
    struct Cluster {
        Ogre::AxisAlignedBox bounds;
        std::vector<unsigned int> indices;
        std::vector<size_t> children;
        Cluster() { bounds.setNull(); }
    };
    NaniteLodDefinition(Ogre::ResourceManager*, const Ogre::String&, Ogre::ResourceHandle,
                        const Ogre::String&, bool = false, Ogre::ManualResourceLoader* = nullptr) {}
    void addCluster(Cluster cluster) { mClusters.push_back(std::move(cluster)); }
    const std::vector<Cluster>& getClusters() const { return mClusters; }
private:
    std::vector<Cluster> mClusters;
};
}
