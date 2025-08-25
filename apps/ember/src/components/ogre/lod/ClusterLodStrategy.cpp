/*
-----------------------------------------------------------------------------
Parts of this source file is lifted from OGRE
    (Object-oriented Graphics Rendering Engine)
For the latest info, see http://www.ogre3d.org/

Copyright (c) 2000-2012 Torus Knot Software Ltd
Copyright (c) 2013 Erik Ogenvik <erik@ogenvik.org>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
-----------------------------------------------------------------------------
*/

#include <Ogre.h>
#include "ClusterLodStrategy.h"

#include <OgreViewport.h>
#include <OgreAny.h>
#include <functional>

using namespace Ogre;

template<>
Ember::OgreView::Lod::ClusterLodStrategy* Ogre::Singleton<Ember::OgreView::Lod::ClusterLodStrategy>::msSingleton = nullptr;


namespace Ember::OgreView::Lod {
//-----------------------------------------------------------------------

ClusterLodStrategy* ClusterLodStrategy::getSingletonPtr() { return msSingleton; }

ClusterLodStrategy& ClusterLodStrategy::getSingleton() {
        assert(msSingleton);
        return (*msSingleton);
}

//-----------------------------------------------------------------------
ClusterLodStrategy::ClusterLodStrategy() : LodStrategy("ClusterLod") {}

//-----------------------------------------------------------------------
Real ClusterLodStrategy::getError(const AxisAlignedBox& bounds, const Matrix4& transform, const Camera* camera) const {
        AxisAlignedBox world = bounds;
        world.transform(transform);

        const Vector3 center = world.getCenter();
        const Real radius = world.getHalfSize().length();

        const Viewport* viewport = camera->getViewport();
        Real viewportArea = static_cast<Real>(viewport->getActualWidth() * viewport->getActualHeight());

        Real boundingArea = Math::PI * Math::Sqr(radius);

        switch (camera->getProjectionType()) {
                case PT_PERSPECTIVE: {
                        Real distanceSquared = center.squaredDistance(camera->getDerivedPosition());
                        if (distanceSquared <= std::numeric_limits<Real>::epsilon())
                                return getBaseValue();
                        const Matrix4& proj = camera->getProjectionMatrix();
                        return (boundingArea * viewportArea * proj[0][0] * proj[1][1]) / distanceSquared;
                }
                case PT_ORTHOGRAPHIC: {
                        Real orthoArea = camera->getOrthoWindowHeight() * camera->getOrthoWindowWidth();
                        if (orthoArea <= std::numeric_limits<Real>::epsilon())
                                return getBaseValue();
                        return (boundingArea * viewportArea) / orthoArea;
                }
                default:
                        assert(0);
                        return 0;
        }
}

Real ClusterLodStrategy::getValueImpl(const MovableObject* movableObject, const Camera* camera) const {
        const Any* any = movableObject->getUserObjectBindings().getUserAny("lodClusters");
        const auto* clusters = any ? Ogre::any_cast<const std::vector<NaniteLodDefinition::Cluster>*>(any) : nullptr;
        Real maxError = 0;
        Matrix4 transform = movableObject->getParentNode()->_getFullTransform();
        if (clusters) {
                for (const auto& c : *clusters) {
                        maxError = std::max(maxError, getError(c.bounds, transform, camera));
                }
        } else {
                // Fallback to whole object bound
                maxError = getError(movableObject->getBoundingBox(), transform, camera);
        }
        return maxError;
}

//---------------------------------------------------------------------
Real ClusterLodStrategy::getBaseValue() const {
	// Use the maximum possible value as base
	return std::numeric_limits<Real>::max();
}

//---------------------------------------------------------------------
Real ClusterLodStrategy::transformBias(Real factor) const {
	// No transformation required for pixel count strategy
	return factor;
}

//---------------------------------------------------------------------
ushort ClusterLodStrategy::getIndex(Real value, const Mesh::MeshLodUsageList& meshLodUsageList) const {
	// Values are descending
	return getIndexDescending(value, meshLodUsageList);
}

//---------------------------------------------------------------------
ushort ClusterLodStrategy::getIndex(Real value, const Material::LodValueList& materialLodValueList) const {
	// Values are descending
	return getIndexDescending(value, materialLodValueList);
}

//---------------------------------------------------------------------
void ClusterLodStrategy::sort(Mesh::MeshLodUsageList& meshLodUsageList) const {
	// Sort descending
	sortDescending(meshLodUsageList);
}

//---------------------------------------------------------------------
bool ClusterLodStrategy::isSorted(const Mesh::LodValueList& values) const {
	// Check if values are sorted descending
	return isSortedDescending(values);
}

void ClusterLodStrategy::selectClusters(const Matrix4& transform,
                                        const Camera* camera,
                                        const std::vector<NaniteLodDefinition::Cluster>& clusters,
                                        Real errorThreshold,
                                        std::vector<size_t>& out) const {
        if (clusters.empty())
                return;

        std::function<void(size_t)> traverse = [&](size_t idx) {
                const auto& c = clusters[idx];
                Real err = getError(c.bounds, transform, camera);
                if (!c.children.empty() && err > errorThreshold) {
                        for (size_t child : c.children)
                                traverse(child);
                } else {
                        out.push_back(idx);
                }
        };

        traverse(0);
}

} // namespace Ember::OgreView::Lod
