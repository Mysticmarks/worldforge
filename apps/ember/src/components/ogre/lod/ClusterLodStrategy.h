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
#ifndef CLUSTER_LOD_STRATEGY_H
#define CLUSTER_LOD_STRATEGY_H

#include <OgrePrerequisites.h>

#include <OgreLodStrategy.h>
#include <OgreSingleton.h>
#include <OgreNode.h>
#include <OgreAxisAlignedBox.h>
#include <OgreMatrix4.h>

#include "NaniteLodDefinition.h"


namespace Ember::OgreView::Lod {

/**
 * @brief LOD strategy evaluating screen-space error per cluster using hierarchical bounds.
 *
 * Instead of relying on whole-mesh pixel counts, this strategy traverses a
 * cluster hierarchy and determines LOD based on the maximum projected error of
 * the contained clusters. It can also select multiple visible clusters for a
 * single frame.
 */
class ClusterLodStrategy : public Ogre::LodStrategy, public Ogre::Singleton<ClusterLodStrategy> {
protected:
        /// @copydoc Ogre::LodStrategy::getValueImpl
        Ogre::Real getValueImpl(const Ogre::MovableObject* movableObject, const Ogre::Camera* camera) const override;

        /// Compute screen-space error for a cluster bounds
        Ogre::Real getError(const Ogre::AxisAlignedBox& bounds,
                           const Ogre::Matrix4& transform,
                           const Ogre::Camera* camera) const;

public:
        /** Default constructor. */
        explicit ClusterLodStrategy();

        ~ClusterLodStrategy() override = default;

	/// @copydoc Ogre::LodStrategy::getBaseValue
	Ogre::Real getBaseValue() const override;

	/// @copydoc Ogre::LodStrategy::transformBias
        Ogre::Real transformBias(Ogre::Real factor) const override;

	/// @copydoc Ogre::LodStrategy::getIndex
        Ogre::ushort getIndex(Ogre::Real value, const Ogre::Mesh::MeshLodUsageList& meshLodUsageList) const override;

	/// @copydoc Ogre::LodStrategy::getIndex
        Ogre::ushort getIndex(Ogre::Real value, const Ogre::Material::LodValueList& materialLodValueList) const override;

	/// @copydoc Ogre::LodStrategy::sort
        void sort(Ogre::Mesh::MeshLodUsageList& meshLodUsageList) const override;

	/// @copydoc Ogre::LodStrategy::isSorted
        bool isSorted(const Ogre::Mesh::LodValueList& values) const override;

        /// Select visible clusters using hierarchical bounds
        void selectClusters(const Ogre::Matrix4& transform,
                            const Ogre::Camera* camera,
                            const std::vector<NaniteLodDefinition::Cluster>& clusters,
                            Ogre::Real errorThreshold,
                            std::vector<size_t>& out) const;

	/** Override standard Singleton retrieval.
	@remarks
	Why do we do this? Well, it's because the Singleton
	implementation is in a .h file, which means it gets compiled
	into anybody who includes it. This is needed for the
	Singleton template to work, but we actually only want it
	compiled into the implementation of the class based on the
	Singleton, not all of them. If we don't change this, we get
	link errors when trying to use the Singleton-based class from
	an outside dll.
	@par
	This method just delegates to the template version anyway,
	but the implementation stays in this single compilation unit,
	preventing link errors.
	*/
        static ClusterLodStrategy& getSingleton();

	/** Override standard Singleton retrieval.
	@remarks
	Why do we do this? Well, it's because the Singleton
	implementation is in a .h file, which means it gets compiled
	into anybody who includes it. This is needed for the
	Singleton template to work, but we actually only want it
	compiled into the implementation of the class based on the
	Singleton, not all of them. If we don't change this, we get
	link errors when trying to use the Singleton-based class from
	an outside dll.
	@par
	This method just delegates to the template version anyway,
	but the implementation stays in this single compilation unit,
	preventing link errors.
	*/
        static ClusterLodStrategy* getSingletonPtr();

};
/** @} */
/** @} */

} // namespace
// namespace
// namespace

#endif
