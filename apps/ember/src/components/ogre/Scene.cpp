/*
 Copyright (C) 2010 Erik Ogenvik <erik@ogenvik.org>

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software Foundation,
 Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "Scene.h"
#include "ISceneRenderingTechnique.h"
#include "terrain/OgreTerrain/OgreTerrainAdapter.h"
#include "CullingManager.h"
#include "InstancingManager.h"

#include "services/config/ConfigService.h"

#include "framework/Log.h"


#include <OgreRoot.h>


namespace Ember::OgreView {


Scene::Scene() :
//The default scene manager actually provides better performance in our benchmarks than the Octree SceneManager
                mSceneManager(Ogre::Root::getSingleton().createSceneManager(Ogre::SMT_DEFAULT, "World")),
                //create the main camera, we will of course have a couple of different cameras, but this will be the main one
                mMainCamera(mSceneManager->createCamera("MainCamera")),
                mBulletWorld(std::make_unique<BulletWorld>()),
                mCullingManager(std::make_unique<CullingManager>(mMainCamera)),
                mInstancingManager(std::make_unique<InstancingManager>()) {
        logger->info("Using SceneManager: {}", mSceneManager->getTypeName());

}

Scene::~Scene() {
	if (!mTechniques.empty()) {
		logger->warn("Scene was deleted while there still was registered techniques.");
	}
	//Delete all techniques before destroying the scene manager.
	mTechniques.clear();
	//No need to delete the camera, as that will taken care of when destroying the scene manager.
	Ogre::Root::getSingleton().destroySceneManager(mSceneManager);
}

Ogre::SceneManager& Scene::getSceneManager() const {
	assert(mSceneManager);
	return *mSceneManager;
}

void Scene::registerEntityWithTechnique(EmberEntity& entity, const std::string& technique) {
	auto I = mTechniques.find(technique);
	if (I != mTechniques.end()) {
		I->second->registerEntity(entity);
	}
}

void Scene::deregisterEntityWithTechnique(EmberEntity& entity, const std::string& technique) {
	auto I = mTechniques.find(technique);
	if (I != mTechniques.end()) {
		I->second->deregisterEntity(entity);
	}
}

void Scene::addRenderingTechnique(const std::string& name, std::unique_ptr<ISceneRenderingTechnique> technique) {
	if (mTechniques.count(name) == 0) {
		mTechniques.emplace(name, std::move(technique));
	}
}

std::unique_ptr<ISceneRenderingTechnique> Scene::removeRenderingTechnique(const std::string& name) {
	auto I = mTechniques.find(name);
	if (I != mTechniques.end()) {
		auto technique = std::move(I->second);
		mTechniques.erase(I);
		return technique;
	}
	return nullptr;
}

std::unique_ptr<Terrain::ITerrainAdapter> Scene::createTerrainAdapter() const {
	ConfigService& configService = ConfigService::getSingleton();
	int pageSize = static_cast<int>(configService.getValue("terrain", "pagesize"));

	if (pageSize <= 0) {
		logger->warn("Page size (\"terrain:pageSize\") config option set to invalid value of {} ; setting it to 512", pageSize);
		pageSize = 512;
	}

	assert(mSceneManager);
	assert(mMainCamera);
	auto adapter = std::make_unique<Terrain::OgreTerrainAdapter>(*mSceneManager, pageSize);
	adapter->setCamera(mMainCamera);
	return adapter;
}

Ogre::Camera& Scene::getMainCamera() const {
        assert(mMainCamera);
        return *mMainCamera;
}

BulletWorld& Scene::getBulletWorld() const {
        return *mBulletWorld;
}

CullingManager& Scene::getCullingManager() const {
        assert(mCullingManager);
        return *mCullingManager;
}

InstancingManager& Scene::getInstancingManager() const {
        assert(mInstancingManager);
        return *mInstancingManager;
}

void Scene::update() {
        // Update instancing buffers and perform culling steps.
        if (mInstancingManager) {
                mInstancingManager->update();
        }
        // Additional culling logic can be added here using mCullingManager.
}

}

