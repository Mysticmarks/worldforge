/*
 * Copyright (c) 2013 Peter Szucs <peter.szucs.dev@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef LODDEFINITION_H
#define LODDEFINITION_H

//Need to include this first because of issues with the OGRE headers.
#include <OgreRenderOperation.h>
#include <MeshLodGenerator/OgreLodConfig.h>
#include <MeshLodGenerator/OgreMeshLodGenerator.h>

#include <OgreResource.h>

#include <string>
#include <map>


namespace Ember::OgreView::Lod {


/**
 * @brief Lod distance config container.
 */
struct LodDistance {
	/**
	 * The mesh name of the Lod distance, which is used in user created meshes.
	 */
	std::string meshName;

	/**
	 * The vertex reduction method of the Lod distance, which is used in automatic vertex reduction.
	 */
	Ogre::LodLevel::VertexReductionMethod reductionMethod = Ogre::LodLevel::VRM_PROPORTIONAL;

	/**
	 * The vertex reduction value of the Lod distance, which is used in automatic vertex reduction.
	 */
	float reductionValue = 0.5f;
};

/**
 * @brief Lod Definition resource. Each *.loddef file is represented by a LodDefinition instance.
 */
class LodDefinition :
		public Ogre::Resource {
public:
	typedef std::map<Ogre::Real, LodDistance> LodDistanceMap;

	/**
	 * @brief Enumeration of Distance types.
	 */
	enum LodType {
		/**
		 * @brief A built in algorithm should reduce the vertex count.
		 */
		LT_AUTOMATIC_VERTEX_REDUCTION,

		/**
		 * @brief User created mesh should be used.
		 */
		LT_USER_CREATED_MESH
	};

	/**
	 * @brief Enumeration of Lod strategies.
	 */
	enum LodStrategy {
		/**
		 * @brief It will use the distance to the camera.
		 */
		LS_DISTANCE,

                /**
                 * @brief Uses screen-space error measured on cluster bounds.
                 */
                LS_PIXEL_COUNT
	};

	/**
	 * @brief Ctor. The parameters are passed directly to Ogre::Resource constructor.
	 *
	 * By default, the Automatic mesh Lod management system is enabled.
	 */
	LodDefinition(Ogre::ResourceManager* creator,
				  const Ogre::String& name,
				  Ogre::ResourceHandle handle,
				  const Ogre::String& group,
				  bool isManual = false,
				  Ogre::ManualResourceLoader* loader = nullptr);

	/**
	 * @brief Dtor.
	 */
	~LodDefinition() override;

	/**
	 * @brief Pure function inherited from Ogre::Resource.
	 *        Loads the resource file as DataStream, then it will call
	 *        XMLLodDefinitionSerializer to load the user data.
	 */
	void loadImpl() override;

	/**
	 * @brief Pure function inherited from Ogre::Resource.
	 */
	void unloadImpl() override;

	/**
	 * @brief Pure function inherited from Ogre::Resource.
	 *        Should return the size of the user data.
	 *        We don't use this, so it will always return 0.
	 */
	size_t calculateSize() const override;

	/**
	 * @brief Returns whether automatic mesh Lod management is used.
	 */
	bool getUseAutomaticLod() const;

	/**
	 * @brief Sets whether automatic mesh Lod management should be used.
	 */
	void setUseAutomaticLod(bool useAutomaticLod);

	/**
	 * @brief Returns the type of the Lod.
	 */
	LodType getType() const;

	/**
	 * @brief Sets the type of the Lod.
	 */
	void setType(LodType type);

	/**
	 * @brief Returns the strategy of the Lod.
	 */
	LodStrategy getStrategy() const;

	/**
	 * @brief Sets the strategy of the Lod.
	 */
	void setStrategy(LodStrategy strategy);

	/**
	 * @brief Adds a Lod distance to the manual Lod configuration.
	 */
	void addLodDistance(Ogre::Real distVal, const LodDistance& distance);

	/**
	 * @brief Returns whether a Lod distance is existing in a manual Lod configuration.
	 */
	bool hasLodDistance(Ogre::Real distVal) const;

	/**
	 * @brief Returns a Lod distance from the manual Lod configuration.
	 */
	LodDistance& getLodDistance(Ogre::Real distVal);

	/**
	 * @brief Returns a Lod distance count for the manual Lod configuration.
	 */
	size_t getLodDistanceCount() const;

	/**
	 * @brief Creates a list of distances in a sorted order.
	 *
	 * This is meant for lua calls only.
	 */
	std::vector<float> createListOfDistances();

	/**
	 * @brief Creates a distance.
	 *
	 * This is meant for lua calls only. Use addLodDistance() if you can.
	 */
	LodDistance& createDistance(Ogre::Real distance);

	/**
	 * @brief Removes a Lod distance from the manual Lod configuration.
	 */
	void removeLodDistance(Ogre::Real distVal);

	/**
	 * @brief Returns a reference to the manual Lod configuration.
	 *
	 * This is useful for iterating through all elements.
	 */
	const LodDistanceMap& getManualLodData() const;

private:
	bool mUseAutomaticLod;
	LodType mType;
	LodStrategy mStrategy;
	LodDistanceMap mManualLod;
};

typedef Ogre::SharedPtr<LodDefinition> LodDefinitionPtr;

inline bool LodDefinition::getUseAutomaticLod() const {
	return mUseAutomaticLod;
}

inline void LodDefinition::setUseAutomaticLod(bool useAutomaticLod) {
	mUseAutomaticLod = useAutomaticLod;
}

inline LodDefinition::LodType LodDefinition::getType() const {
	return mType;
}

inline void LodDefinition::setType(LodType type) {
	mType = type;
}

inline LodDefinition::LodStrategy LodDefinition::getStrategy() const {
	return mStrategy;
}

inline void LodDefinition::setStrategy(LodStrategy strategy) {
	mStrategy = strategy;
}

inline const LodDefinition::LodDistanceMap& LodDefinition::getManualLodData() const {
	return mManualLod;
}

inline size_t LodDefinition::getLodDistanceCount() const {
	return mManualLod.size();
}

}


#endif // ifndef LODDEFINITION_H
