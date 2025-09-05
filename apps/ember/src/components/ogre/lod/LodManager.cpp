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

#include "LodManager.h"
#include "LodDefinitionManager.h"
#include "NaniteLodDefinition.h"
#include "ClusterLodStrategy.h"

#include <MeshLodGenerator/OgreMeshLodGenerator.h>
#include <OgreDistanceLodStrategy.h>
#include <OgreCamera.h>
#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <vector>


namespace Ember::OgreView::Lod {

LodManager::LodManager() = default;

LodManager::~LodManager() = default;

void LodManager::loadLod(Ogre::MeshPtr mesh) {
	assert(mesh->getNumLodLevels() == 1);
	std::string lodDefName = convertMeshNameToLodName(mesh->getName());

	try {
		Ogre::ResourcePtr resource = LodDefinitionManager::getSingleton().load(lodDefName, "General");
		auto def = dynamic_cast<const LodDefinition*>(resource.get());
		if (def) {
			loadLod(mesh, *def);
		}
	} catch (const Ogre::FileNotFoundException&) {
		// Exception is thrown if a mesh hasn't got a loddef.
		// By default, use the automatic mesh lod management system.
		Ogre::MeshLodGenerator::getSingleton().generateAutoconfiguredLodLevels(mesh);
	}
}

void LodManager::loadLod(Ogre::MeshPtr mesh, const LodDefinition& def) {
        if (auto nanite = dynamic_cast<const NaniteLodDefinition*>(&def)) {
                loadLod(mesh, *nanite);
        } else if (def.getUseAutomaticLod()) {
                Ogre::MeshLodGenerator::getSingleton().generateAutoconfiguredLodLevels(mesh);
        } else if (def.getLodDistanceCount() == 0) {
                mesh->removeLodLevels();
                return;
	} else {
		Ogre::LodStrategy* strategy;
		if (def.getStrategy() == LodDefinition::LS_DISTANCE) {
			strategy = &Ogre::DistanceLodBoxStrategy::getSingleton();
		} else {
                        strategy = &ClusterLodStrategy::getSingleton();
		}
		mesh->setLodStrategy(strategy);

		if (def.getType() == LodDefinition::LT_AUTOMATIC_VERTEX_REDUCTION) {
			// Automatic vertex reduction
			Ogre::LodConfig lodConfig;
			lodConfig.mesh = mesh;
			lodConfig.strategy = strategy;
                        const LodDefinition::LodDistanceMap& data = def.getManualLodData();

                        auto addAutomaticLod = [&lodConfig](auto it, auto end) {
                                for (; it != end; ++it) {
                                        const LodDistance& dist = it->second;
                                        Ogre::LodLevel lodLevel;
                                        lodLevel.distance = it->first;
                                        lodLevel.reductionMethod = dist.reductionMethod;
                                        lodLevel.reductionValue = dist.reductionValue;
                                        lodConfig.levels.push_back(lodLevel);
                                }
                        };

                        if (def.getStrategy() == LodDefinition::LS_DISTANCE) {
                                addAutomaticLod(data.begin(), data.end());
                        } else {
                                addAutomaticLod(data.rbegin(), data.rend());
                        }
			Ogre::MeshLodGenerator::getSingleton().generateLodLevels(lodConfig);
		} else {
			// User created Lod

			mesh->removeLodLevels();

                        const LodDefinition::LodDistanceMap& data = def.getManualLodData();

                        auto addUserLod = [&mesh](auto it, auto end) {
                                for (; it != end; ++it) {
                                        const Ogre::String& meshName = it->second.meshName;
                                        if (!meshName.empty()) {
                                                assert(Ogre::ResourceGroupManager::getSingleton().resourceExistsInAnyGroup(meshName));
                                                mesh->updateManualLodLevel(static_cast<Ogre::ushort>(it->first), meshName);
                                        }
                                }
                        };

                        if (def.getStrategy() == LodDefinition::LS_DISTANCE) {
                                addUserLod(data.begin(), data.end());
                        } else {
                                addUserLod(data.rbegin(), data.rend());
                        }
                }
        }
}

void LodManager::loadLod(Ogre::MeshPtr mesh, const NaniteLodDefinition& def) {
        mesh->removeLodLevels();

        Ogre::Camera* camera = nullptr;
        if (auto* root = Ogre::Root::getSingletonPtr()) {
                if (!root->getSceneManagerIterator().end()) {
                        auto sm = root->getSceneManagerIterator().begin()->second;
                        if (sm && !sm->getCameraIterator().end()) {
                                camera = sm->getCameraIterator().begin()->second;
                        }
                }
        }

        const auto& clusters = def.getClusters();
        std::vector<size_t> selected;
        if (camera) {
                ClusterLodStrategy::getSingleton().selectClusters(Ogre::Matrix4::IDENTITY,
                                                                  camera,
                                                                  clusters,
                                                                  1.0f,
                                                                  selected);
        }

        for (size_t idx : selected) {
                const auto& cluster = clusters[idx];
                // Placeholder: cluster geometry streaming would occur here
                (void)cluster;
        }
}

std::string LodManager::convertMeshNameToLodName(std::string meshName) {
	size_t start = meshName.find_last_of("/\\");
	if (start != std::string::npos) {
		meshName = meshName.substr(start + 1);
	}

	size_t end = meshName.find_last_of('.');
	if (end != std::string::npos) {
		meshName = meshName.substr(0, end);
	}

        meshName += ".loddef";
        return meshName;
}

}
