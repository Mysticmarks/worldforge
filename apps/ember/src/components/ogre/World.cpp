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

#include "World.h"

#include "Avatar.h"
#include "MovementController.h"
#include "EmberEntityFactory.h"
#include "MotionManager.h"
#include "authoring/EntityMoveManager.h"
#include "Scene.h"
#include "domain/EmberEntity.h"
#include "EmberOgreSignals.h"
#include "ForestRenderingTechnique.h"
#include "RenderDistanceManager.h"
#include "AvatarCameraWarper.h"
#include "components/ogre/AvatarCameraMotionHandler.h"
#include "components/ogre/authoring/AuthoringManager.h"
#include "components/ogre/authoring/AuthoringMoverConnector.h"

#include "LodLevelManager.h"

#include "TerrainEntityManager.h"
#include "TerrainPageDataProvider.h"
#include "terrain/TerrainManager.h"
#include "terrain/TerrainHandler.h"

#include "camera/MainCamera.h"
#include "camera/ThirdPersonCameraMount.h"

#include "environment/Environment.h"
#include "environment/CaelumEnvironment.h"
#include "environment/SimpleEnvironment.h"

#include "services/config/ConfigService.h"

#include "ProjectileRenderingTechnique.h"
#include "WorldAttachment.h"

#include <Eris/Avatar.h>
#include <Eris/View.h>
#include <Eris/Connection.h>

#include <OgreSceneManager.h>
#include <OgreRenderWindow.h>
#include <OgreRoot.h>
#include <OgreCamera.h>
#include <OgreViewport.h>
#include <OgreSceneNode.h>

#include <memory>
#include <utility>
#include <execution>
#include <vector>

#include "framework/Profiler.h"

namespace Ember::OgreView {

World::World(Eris::View& view,
			 Ogre::RenderWindow& renderWindow,
			 Ember::OgreView::EmberOgreSignals& signals,
			 Ember::Input& input, Ember::OgreView::ShaderManager& shaderManager,
			 GraphicalChangeAdapter& graphicalChangeAdapter,
			 EntityMapping::EntityMappingManager& entityMappingManager) :
		mView(view),
		mRenderWindow(renderWindow),
		mSignals(signals),
		mScene(std::make_unique<Scene>()),
		mViewport(renderWindow.addViewport(&mScene->getMainCamera())),
		mAvatar(nullptr),
		mMovementController(nullptr),
		mTerrainManager(std::make_unique<Terrain::TerrainManager>(mScene->createTerrainAdapter(), *mScene, view, shaderManager, view.getEventService(), graphicalChangeAdapter)),
		mMainCamera(std::make_unique<Camera::MainCamera>(*mScene, mRenderWindow, input, *mTerrainManager->getTerrainAdapter())),
		mMoveManager(std::make_unique<Authoring::EntityMoveManager>(*this)),
		mMotionManager(std::make_unique<MotionManager>()),
		mAvatarCameraMotionHandler(nullptr),
		mAvatarCameraWarper(nullptr),
		mEntityWorldPickListener(std::make_unique<EntityWorldPickListener>(mView, *mScene)),
		mAuthoringManager(std::make_unique<Authoring::AuthoringManager>(*this)),
		mAuthoringMoverConnector(std::make_unique<Authoring::AuthoringMoverConnector>(*mAuthoringManager, *mMoveManager)),
		mTerrainEntityManager(std::make_unique<TerrainEntityManager>(view, mTerrainManager->getHandler(), mScene->getSceneManager())),
		mLodLevelManager(std::make_unique<Lod::LodLevelManager>(graphicalChangeAdapter, mScene->getMainCamera())),
		mEnvironment(std::make_unique<Environment::Environment>(mScene->getSceneManager(),
																*mTerrainManager,
																std::make_unique<Environment::CaelumEnvironment>(&mScene->getSceneManager(), &renderWindow, mScene->getMainCamera(),
																												 mView.getAvatar().getWorld().getCalendar()),
																std::make_unique<Environment::SimpleEnvironment>(&mScene->getSceneManager(), &renderWindow, mScene->getMainCamera()))),
		mRenderDistanceManager(std::make_unique<RenderDistanceManager>(graphicalChangeAdapter, *(mEnvironment->getFog()), mScene->getMainCamera())),
		mConfigListenerContainer(std::make_unique<ConfigListenerContainer>()),
		mTopLevelEntity(nullptr) {
	mAfterTerrainUpdateConnection = mTerrainManager->getHandler().EventAfterTerrainUpdate.connect(sigc::mem_fun(*this, &World::terrainManager_AfterTerrainUpdate));

	signals.EventTerrainManagerCreated.emit(*mTerrainManager);

	mScene->addRenderingTechnique("forest", std::make_unique<ForestRenderingTechnique>(*mEnvironment->getForest()));
	mScene->addRenderingTechnique("projectile", std::make_unique<ProjectileRenderingTechnique>(mScene->getSceneManager()));

	//set the background colour to black
	mViewport->setBackgroundColour(Ogre::ColourValue(0, 0, 0));
	mScene->getMainCamera().setAspectRatio(Ogre::Real(mViewport->getActualWidth()) / Ogre::Real(mViewport->getActualHeight()));

	signals.EventMotionManagerCreated.emit(*mMotionManager);
	Ogre::Root::getSingleton().addFrameListener(mMotionManager.get());

	view.registerFactory(std::make_unique<EmberEntityFactory>(view, *mScene));

	view.getAvatar().GotCharacterEntity.connect(sigc::mem_fun(*this, &World::View_gotAvatarCharacter));

	mMainCamera->pushWorldPickListener(mEntityWorldPickListener.get());

	view.TopLevelEntityChanged.connect(sigc::mem_fun(*this, &World::View_topLevelEntityChanged));

}

World::~World() {
	mMainCamera->attachToMount(nullptr);
	mMainCamera->setMovementProvider(nullptr);
	mSignals.EventMovementControllerDestroyed.emit();

	mAfterTerrainUpdateConnection.disconnect();

	mSignals.EventTerrainManagerBeingDestroyed();
	mTerrainManager.reset();
	mSignals.EventTerrainManagerDestroyed();

	if (mMotionManager) {
		Ogre::Root::getSingleton().removeFrameListener(mMotionManager.get());
	}
	mSignals.EventMotionManagerDestroyed();
}

Eris::View& World::getView() const {
	return mView;
}

Eris::EventService& World::getEventService() const {
	return mView.getEventService();
}


Scene& World::getScene() const {
	assert(mScene);
	return *mScene;
}

Ogre::SceneManager& World::getSceneManager() const {
	return mScene->getSceneManager();
}

Avatar* World::getAvatar() const {
	return mAvatar.get();
}

MotionManager& World::getMotionManager() const {
	assert(mMotionManager);
	return *mMotionManager;
}

Camera::MainCamera& World::getMainCamera() const {
	assert(mMainCamera);
	return *mMainCamera;
}

MovementController* World::getMovementController() const {
	return mMovementController.get();
}

EmberEntity* World::getEmberEntity(const std::string& eid) const {
	return dynamic_cast<EmberEntity*>(mView.getEntity(eid));
}

void World::View_topLevelEntityChanged() {
	if (mTopLevelEntity) {
		mTopLevelEntity->setAttachment({});
	}
	EmberEntity* nearestPhysicalDomainEntity = nullptr;
	auto* currentEntity = dynamic_cast<EmberEntity*>(mView.getAvatar().getEntity());

	//Find any parent entity with a physical domain. If so, create a WorldAttachment to
	if (currentEntity) {
		do {
			if (currentEntity->hasProperty("domain")) {
				auto& domainProp = currentEntity->valueOfProperty("domain");
				if (domainProp.isString() && domainProp.String() == "physical") {
					nearestPhysicalDomainEntity = currentEntity;
					break;
				}
			}
			if (!currentEntity->getLocation()) {
				break;
			}
			currentEntity = currentEntity->getEmberLocation();
		} while (currentEntity);
	}

	if (nearestPhysicalDomainEntity) {
		auto attachment = std::make_unique<WorldAttachment>(*this,
															*nearestPhysicalDomainEntity,
															mScene->getSceneManager().getRootSceneNode()->createChildSceneNode("entity_" + nearestPhysicalDomainEntity->getId()),
															*mScene);
		nearestPhysicalDomainEntity->setAttachment(std::move(attachment));
		mTopLevelEntity = nearestPhysicalDomainEntity;
	} else {
		mTopLevelEntity = dynamic_cast<EmberEntity*>(mView.getAvatar().getEntity()->getTopEntity());
		if (mTopLevelEntity) {
			mTopLevelEntity->setAttachment({});
		}
	}
}

EntityWorldPickListener& World::getEntityPickListener() const {
	assert(mEntityWorldPickListener);
	return *mEntityWorldPickListener;
}

Authoring::AuthoringManager& World::getAuthoringManager() const {
	assert(mAuthoringManager);
	//This can never be null.
	return *mAuthoringManager;
}

Terrain::TerrainManager& World::getTerrainManager() const {
	assert(mTerrainManager);
	return *mTerrainManager;
}

Lod::LodLevelManager& World::getLodLevelManager() const {
	assert(mLodLevelManager);
	return *mLodLevelManager;
}

RenderDistanceManager& World::getRenderDistanceManager() const {
	assert(mRenderDistanceManager);
	return *mRenderDistanceManager;
}

Environment::Environment* World::getEnvironment() const {
	return mEnvironment.get();
}

void World::terrainManager_AfterTerrainUpdate(const std::vector<WFMath::AxisBox<2>>& areas, const std::vector<std::shared_ptr<Terrain::TerrainPageGeometry>>&) {
        Profiler::start("terrainUpdate");
        auto* emberEntity = dynamic_cast<EmberEntity*>(mView.getTopLevel());
        if (emberEntity) {
                updateEntityPosition(emberEntity, areas);
        }
        Profiler::stop("terrainUpdate");
}

void World::updateEntityPosition(EmberEntity* entity, const std::vector<WFMath::AxisBox<2>>& areas) {
        entity->adjustPosition();
        std::vector<EmberEntity*> children;
        for (size_t i = 0; i < entity->numContained(); ++i) {
                if (auto* containedEntity = dynamic_cast<EmberEntity*>(entity->getContained(i))) {
                        children.push_back(containedEntity);
                }
        }
        std::for_each(std::execution::par, children.begin(), children.end(), [&](EmberEntity* child) {
                updateEntityPosition(child, areas);
        });
}

void World::View_gotAvatarCharacter(Eris::Entity* entity) {
	if (entity) {
		auto& emberEntity = dynamic_cast<EmberEntity&>(*entity);
		//Set up the third person avatar camera and switch to it.
		mAvatar = std::make_unique<Avatar>(mView.getAvatar(), emberEntity, *mScene, mMainCamera->getCameraSettings(), *(mTerrainManager->getTerrainAdapter()));
		mAvatarCameraMotionHandler = std::make_unique<AvatarCameraMotionHandler>(*mAvatar);
		mAvatar->getCameraMount().setMotionHandler(mAvatarCameraMotionHandler.get());
		mMovementController = std::make_unique<MovementController>(*mAvatar, *mMainCamera, *mTerrainManager);
		mMainCamera->setMovementProvider(mMovementController.get());
		mMainCamera->attachToMount(&mAvatar->getCameraMount());

		if (mAvatar->isAdmin()) {
			mAvatarCameraWarper = std::make_unique<AvatarCameraWarper>(*mMovementController, *mMainCamera, mView);
		}

		emberEntity.BeingDeleted.connect(sigc::mem_fun(*this, &World::avatarEntity_BeingDeleted));

		mSignals.EventMovementControllerCreated.emit();
		mSignals.EventCreatedAvatarEntity.emit(emberEntity);
		mTerrainManager->startPaging();
		EventGotAvatar();
	} else {
		logger->critical("Somehow got a null avatar entity.");
	}
}

void World::avatarEntity_BeingDeleted() {
//	mAvatarCameraWarper.reset();
//	mMainCamera->attachToMount(nullptr);
//	mMainCamera->setMovementProvider(nullptr);
//	mMovementController.reset();
//	mSignals.EventMovementControllerDestroyed.emit();
//	mAvatarCameraMotionHandler.reset();
//	mAvatar.reset();
}


}

