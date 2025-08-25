#include <memory>

/*
 Copyright (C) 2004  Erik Ogenvik
 some parts Copyright (C) 2004 bad_camel at Ogre3d forums

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

#include "MovementController.h"

#include "domain/EmberEntity.h"
#include "components/ogre/GUIManager.h"
#include "components/ogre/Avatar.h"
#include "components/ogre/OgreInfo.h"
#include "components/ogre/Convert.h"
#include "components/ogre/Scene.h"
#include "components/ogre/FreeFlyingCameraMotionHandler.h"
#include "components/ogre/camera/MainCamera.h"
#include "components/ogre/camera/FirstPersonCameraMount.h"
#include "components/ogre/camera/ThirdPersonCameraMount.h"
#include "components/ogre/terrain/TerrainManager.h"
#include "components/ogre/authoring/AwarenessVisualizer.h"

#include "components/navigation/Awareness.h"
#include "components/navigation/Steering.h"

#include "services/server/ServerService.h"
#include "services/config/ConfigListenerContainer.h"

#include "framework/MainLoopController.h"

#include <Eris/View.h>
#include <Eris/Avatar.h>

#include <OgreRoot.h>
#include <OgreCamera.h>
#include <OgreSceneNode.h>

using namespace Ogre;
using namespace Ember;

namespace Ember::OgreView {

MovementControllerInputListener::MovementControllerInputListener(MovementController& controller) :
		mController(controller) {
	Input::getSingleton().EventMouseButtonPressed.connect(sigc::mem_fun(*this, &MovementControllerInputListener::input_MouseButtonPressed));
	Input::getSingleton().EventMouseButtonReleased.connect(sigc::mem_fun(*this, &MovementControllerInputListener::input_MouseButtonReleased));

}

void MovementControllerInputListener::input_MouseButtonPressed(Input::MouseButton button, Input::InputMode mode) {

	if (mode == Input::IM_MOVEMENT && button == Input::MouseButtonLeft) {
		if (mController.mIsFreeFlying) {
			mController.mMovementDirection.z() = -1;
			mController.stopSteering();
		} else {
			mController.mAvatar.performDefaultUsage();
		}
	}
}

void MovementControllerInputListener::input_MouseButtonReleased(Input::MouseButton button, Input::InputMode mode) {
	if (mode == Input::IM_GUI) {
		mController.mMovementDirection.x() = 0;
		mController.mMovementDirection.y() = 0;
		mController.mMovementDirection.z() = 0;
	} else if (mode == Input::IM_MOVEMENT && button == Input::MouseButtonLeft) {
		if (mController.mIsFreeFlying) {
			mController.mMovementDirection.z() = 0;
		} else {
			mController.mAvatar.stopCurrentTask();
		}
	}
}

MovementController::MovementController(Avatar& avatar, Camera::MainCamera& camera, IHeightProvider& heightProvider) :
		WalkToggle("+walk", this, "Toggle walking mode."),
		ToggleCameraAttached("toggle_cameraattached", this, "Toggle between the camera being attached to the avatar and free flying."),
		MovementMoveForward("+movement_move_forward", this, "Move forward."),
		MovementMoveBackward("+movement_move_backward", this, "Move backward."),
		MovementMoveDownwards("+movement_move_downwards", this, "Move downwards."),
		MovementMoveUpwards("+movement_move_upwards", this, "Move upwards."),
		MovementStrafeLeft("+movement_strafe_left", this, "Strafe left."),
		MovementStrafeRight("+movement_strafe_right", this, "Strafe right."),
		CameraOnAvatar("camera_on_avatar", this, "Positions the free flying camera on the avatar.")
		/*, MovementRotateLeft("+Movement_rotate_left", this, "Rotate left.")
		 , MovementRotateRight("+Movement_rotate_right", this, "Rotate right.")*/
		//, MoveCameraTo("movecamerato", this, "Moves the camera to a point.")
		, mCamera(camera),
		mMovementCommandMapper("movement", "key_bindings_movement"),
		mIsRunning(true),
		mMovementDirection(WFMath::Vector<3>::ZERO()),
		mDecalObject(nullptr),
		mDecalNode(nullptr),
		mControllerInputListener(*this),
		mAvatar(avatar),
		mFreeFlyingNode(nullptr),
		mIsFreeFlying(false),
		mAwareness(nullptr),
                mAwarenessVisualizer(nullptr),
                mSteering(nullptr),
                mConfigListenerContainer(std::make_unique<ConfigListenerContainer>()),
                mVisualizePath(false),
                mWalkableClimb(100),
                mWalkableSlopeAngle(70) {

	auto evaluateLocationFn = [this, &heightProvider, &camera](EmberEntity& location) {
		if (location.hasBBox()) {
			try {
                                mAwareness = std::make_unique<Navigation::Awareness>(mAvatar.getEmberEntity(), heightProvider, 64, mWalkableClimb, mWalkableSlopeAngle);
				mAwarenessVisualizer = std::make_unique<Authoring::AwarenessVisualizer>(*mAwareness, *camera.getCamera().getSceneManager());
				mSteering = std::make_unique<Navigation::Steering>(*mAwareness, mAvatar.getErisAvatar());
				mSteering->EventPathUpdated.connect(sigc::mem_fun(*this, &MovementController::Steering_PathUpdated));

				mAwareness->EventTileDirty.connect([this] {
					mAvatar.getErisAvatar().getView().getEventService().runOnMainThread([this]() { this->tileRebuild(); }, mActiveMarker);
				});

				mAwareness->EventTileUpdated.connect([&](int, int) {
					if (mSteering->isEnabled()) {
						mSteering->requestUpdate();
					}
				});

				MainLoopController::getSingleton().EventFrameProcessed.connect(sigc::mem_fun(*this, &MovementController::frameProcessed));


			} catch (const std::exception& ex) {
				logger->error("Could not setup awareness; steering and path finding will be disabled: {}", ex.what());
			}
		} else {
			mSteering.reset();
			mAwarenessVisualizer.reset();
			mAwareness.reset();
		}
	};


	avatar.getEmberEntity().LocationChanged.connect([&avatar, evaluateLocationFn, this](Eris::Entity* oldLocation) {
		auto newParentLocation = avatar.getEmberEntity().getEmberLocation();
		if (newParentLocation) {
			mParentBboxConnection = newParentLocation->observe("bbox", [evaluateLocationFn, newParentLocation](const Atlas::Message::Element&) {
				evaluateLocationFn(*newParentLocation);
			}, true);
			evaluateLocationFn(*avatar.getEmberEntity().getEmberLocation());
		} else {
			mParentBboxConnection.disconnect();
			mSteering.reset();
			mAwarenessVisualizer.reset();
			mAwareness.reset();
		}
	});
	if (avatar.getEmberEntity().getEmberLocation()) {
		evaluateLocationFn(*avatar.getEmberEntity().getEmberLocation());
	}

	mConfigListenerContainer->registerConfigListenerWithDefaults("authoring", "visualizerecasttiles", sigc::mem_fun(*this, &MovementController::Config_VisualizeRecastTiles), false);
        mConfigListenerContainer->registerConfigListenerWithDefaults("authoring", "visualizerecastpath", sigc::mem_fun(*this, &MovementController::Config_VisualizeRecastPath), false);
        mConfigListenerContainer->registerConfigListenerWithDefaults("navigation", "walkableclimb", sigc::mem_fun(*this, &MovementController::Config_WalkableClimb), mWalkableClimb);
        mConfigListenerContainer->registerConfigListenerWithDefaults("navigation", "walkableslopeangle", sigc::mem_fun(*this, &MovementController::Config_WalkableSlopeAngle), mWalkableSlopeAngle);

	mMovementCommandMapper.restrictToInputMode(Input::IM_MOVEMENT);
	avatar.getEmberEntity().Moved.connect(sigc::mem_fun(*this, &MovementController::Entity_Moved));


	mMovementCommandMapper.bindToInput(Input::getSingleton());
	Input::getSingleton().setMovementModeEnabled(true);

	try {
		mFreeFlyingNode = mAvatar.getScene().getSceneManager().getRootSceneNode()->createChildSceneNode(OgreInfo::createUniqueResourceName("FreeFlyingCameraNode"));
		if (mFreeFlyingNode) {
			if (mAvatar.getEmberEntity().getPredictedPos().isValid()) {
				mFreeFlyingNode->setPosition(Convert::toOgre(mAvatar.getEmberEntity().getPredictedPos()));
				mFreeFlyingNode->translate(Ogre::Vector3(0, 3, 0)); //put it a little on top of the avatar node
			}
			mFreeFlyingMotionHandler = std::make_unique<FreeFlyingCameraMotionHandler>(*mFreeFlyingNode);
			mCameraMount = std::make_unique<Camera::FirstPersonCameraMount>(mCamera.getCameraSettings(), mAvatar.getScene().getSceneManager());
			mCameraMount->setMotionHandler(mFreeFlyingMotionHandler.get());
			mCameraMount->attachToNode(mFreeFlyingNode);
		} else {
			logger->error("Failed to create free flying camera node.");
		}

	} catch (const std::exception& ex) {
		logger->error("Error when setting up free flying camera mount: {}", ex.what());
	}

}

MovementController::~MovementController() {

	if (mFreeFlyingNode) {
		mFreeFlyingNode->getCreator()->destroySceneNode(mFreeFlyingNode);
	}
	if (mDecalNode) {
		mDecalNode->getCreator()->destroySceneNode(mDecalNode);
	}
	if (mDecalObject) {
		mDecalObject->_getManager()->destroyMovableObject(mDecalObject);
	}
}

void MovementController::tileRebuild() {
	if (mAwareness) {
		size_t dirtyTiles = mAwareness->rebuildDirtyTile();
		if (dirtyTiles) {
			mAvatar.getErisAvatar().getView().getEventService().runOnMainThread([this] { this->tileRebuild(); }, mActiveMarker);
		}
	}
}

void MovementController::setCameraFreeFlying(bool freeFlying) {
	if (freeFlying && mCameraMount) {
		mCamera.attachToMount(mCameraMount.get());
		mIsFreeFlying = true;
	} else {
		mCamera.attachToMount(&mAvatar.getCameraMount());
		mIsFreeFlying = false;
	}
}

bool MovementController::isCameraFreeFlying() const {
	return mIsFreeFlying;
}

void MovementController::runCommand(const std::string& command, const std::string& args) {
	if (WalkToggle == command) {
		mIsRunning = false;
		EventMovementModeChanged(getMode());
	} else if (WalkToggle.getInverseCommand() == command) {
		mIsRunning = true;
		EventMovementModeChanged(getMode());
	} else if (ToggleCameraAttached == command) {
		setCameraFreeFlying(!mIsFreeFlying);
	} else if (MovementMoveForward == command) {
		mMovementDirection.z() = -1;
	} else if (MovementMoveForward.getInverseCommand() == command) {
		mMovementDirection.z() = 0;
	} else if (MovementMoveBackward == command) {
		mMovementDirection.z() = 1;
	} else if (MovementMoveBackward.getInverseCommand() == command) {
		mMovementDirection.z() = 0;
	} else if (MovementStrafeLeft == command) {
		mMovementDirection.x() = -1;
	} else if (MovementStrafeLeft.getInverseCommand() == command) {
		mMovementDirection.x() = 0;
	} else if (MovementStrafeRight == command) {
		mMovementDirection.x() = 1;
	} else if (MovementStrafeRight.getInverseCommand() == command) {
		mMovementDirection.x() = 0;
	} else if (MovementMoveUpwards == command) {
		mMovementDirection.y() = 1;
	} else if (MovementMoveUpwards.getInverseCommand() == command) {
		mMovementDirection.y() = 0;
	} else if (MovementMoveDownwards == command) {
		mMovementDirection.y() = -1;
	} else if (MovementMoveDownwards.getInverseCommand() == command) {
		mMovementDirection.y() = 0;
		/*	} else if (MovementRotateLeft == command) {
		 mAvatarCamera->yaw(Ogre::Degree(-15));
		 } else if (MovementRotateRight == command) {
		 mAvatarCamera->yaw(Ogre::Degree(15));*/
		//	} else if (MoveCameraTo == command) {
		//		if (!mIsAttached) {
		//			Tokeniser tokeniser;
		//			tokeniser.initTokens(args);
		//			std::string x = tokeniser.nextToken();
		//			std::string y = tokeniser.nextToken();
		//			std::string z = tokeniser.nextToken();
		//
		//			if (x == "" || y == "" || z == "") {
		//				return;
		//			} else {
		//				Ogre::Vector3 position(Ogre::StringConverter::parseReal(x),Ogre::StringConverter::parseReal(y),Ogre::StringConverter::parseReal(z));
		//				mFreeFlyingCameraNode->setPosition(position);
		//			}
		//		}
	} else if (CameraOnAvatar == command) {
		if (mFreeFlyingNode && mAvatar.getEmberEntity().getPosition().isValid()) {
			mFreeFlyingNode->setPosition(Convert::toOgre(mAvatar.getEmberEntity().getPosition()));
		}
	}
	if (mMovementDirection != WFMath::Vector<3>::ZERO()) {
		stopSteering();
	}
}

void MovementController::stopSteering() {
	if (mSteering) {
		mSteering->stopSteering();
		if (mAwareness->needsPruning()) {
			schedulePruning();
		}
	}
}


void MovementController::frameProcessed(const TimeFrame&, unsigned int) {

	if (mDecalObject) {
		//hide the decal when we're close to it
		if (mDecalNode->_getDerivedPosition().distance(mAvatar.getAvatarSceneNode()->_getDerivedPosition()) < 1) {
			mDecalNode->setVisible(false);
		}
	}
	if (mSteering) {
		mSteering->update();
	}

}

void MovementController::Config_VisualizeRecastTiles(const std::string&, const std::string&, varconf::Variable& var) {
	if (var.is_bool() && mAwarenessVisualizer) {
		mAwarenessVisualizer->setTileVisualizationEnabled((bool) var);
	}
}

void MovementController::Config_VisualizeRecastPath(const std::string&, const std::string&, varconf::Variable& var) {
	if (var.is_bool() && mAwarenessVisualizer) {
		mVisualizePath = (bool) var;
		if (!mVisualizePath) {
			//By visualizing an empty path we'll remove any lingering path.
			mAwarenessVisualizer->visualizePath(std::list<WFMath::Point<3>>());
		}
	}
}

void MovementController::Config_WalkableClimb(const std::string&, const std::string&, varconf::Variable& var) {
        if (var.is_int()) {
                mWalkableClimb = static_cast<int>(var);
        }
}

void MovementController::Config_WalkableSlopeAngle(const std::string&, const std::string&, varconf::Variable& var) {
        if (var.is_int()) {
                mWalkableSlopeAngle = static_cast<int>(var);
        }
}

MovementControllerMode::Mode MovementController::getMode() const {
	return mIsRunning ? MovementControllerMode::MM_RUN : MovementControllerMode::MM_WALK;
}

void MovementController::moveToPoint(const Ogre::Vector3& point) {
	if (!mDecalNode) {
		createDecal(point);
	}

	//TODO: reapply the terrain lookup without using the terrain manager directly
	//	if (mDecalNode) {
	//		//make sure it's at the correct height, since the visibility of it is determined by the bounding box
	//		Ogre::Real height = EmberOgre::getSingleton().getTerrainManager()->getAdapter()->getHeightAt(point.x, point.z);
	//		mDecalNode->setPosition(Ogre::Vector3(point.x, height, point.z));
	//		mDecalNode->setVisible(true);
	//	}
//
	if (mSteering) {
		WFMath::Point<3> atlasPos = Convert::toWF<WFMath::Point<3>>(point);
		mSteering->setDestination(atlasPos);
		mSteering->updatePath();
		mSteering->startSteering();

		if (mAwareness->needsPruning()) {
			schedulePruning();
		}
	}
}

void MovementController::schedulePruning() {
	mAvatar.getErisAvatar().getView().getEventService().runOnMainThread([this]() {
		this->mAwareness->pruneTiles();
		if (mAwareness->needsPruning()) {
			schedulePruning();
		}
	}, mActiveMarker);
}

void MovementController::Entity_Moved() {
	if (mSteering && mSteering->isEnabled()) {
		mSteering->requestUpdate();
		mSteering->setIsExpectingServerMovement(false);
	}
}

void MovementController::Steering_PathUpdated() {
	if (mVisualizePath) {
		mAwarenessVisualizer->visualizePath(mSteering->getPath());
	}
}


void MovementController::teleportTo(const Ogre::Vector3& point, EmberEntity* locationEntity) {
	WFMath::Vector<3> atlasVector = Convert::toWF<WFMath::Vector<3>>(point);
	WFMath::Point<3> atlasPos(atlasVector.x(), atlasVector.y(), atlasVector.z());

	mAvatar.getErisAvatar().place(&mAvatar.getEmberEntity(), mAvatar.getEmberEntity().getLocation(), atlasPos);
}

WFMath::Vector<3> MovementController::getMovementForCurrentFrame() const {
	if (mMovementDirection.isValid()) {
		//If the vector is null we'll get an error if we normalize, so we'll just return a zero vector then.
		if (mMovementDirection != WFMath::Vector<3>::ZERO()) {
			//When not running, we assume that the speed is half of the max speed (which is 1.0)
			if (mIsRunning) {
				return WFMath::Vector<3>(mMovementDirection).normalize(1.0);
			} else {
				return WFMath::Vector<3>(mMovementDirection).normalize(1.0) * 0.5;
			}
		} else {
			return mMovementDirection;
		}
	} else {
		return WFMath::Vector<3>::ZERO();
	}
}

void MovementController::createDecal(Ogre::Vector3 position) {
//	try {
//		// Create object MeshDecal
//		Ogre::SceneManager& sceneManager = mAvatar.getScene().getSceneManager();
//		Ogre::NameValuePairList params;
//		params["materialName"] = "/common/base/terraindecal";
//		params["width"] = StringConverter::toString(2);
//		params["height"] = StringConverter::toString(2);
//		params["sceneMgrInstance"] = sceneManager.getName();
//
//		mDecalObject = sceneManager.createMovableObject("MovementControllerMoveToDecal", "PagingLandScapeMeshDecal", &params);
//
//		// Add MeshDecal to Scene
//		mDecalNode = sceneManager.createSceneNode("MovementControllerMoveToDecalNode");
//		//the decal code is a little shaky and relies on us setting the position of the node before we add the moveable object
//		sceneManager.getRootSceneNode()->addChild(mDecalNode);
//		mDecalNode->setPosition(position);
//		mDecalNode->attachObject(mDecalObject);
//		// 	mDecalNode->showBoundingBox(true);
//
//		// 		mPulsatingController = new Ogre::WaveformControllerFunction(Ogre::WFT_SINE, 1, 0.33, 0.25);
//	} catch (const std::exception& ex) {
//		logger->warn("Error when creating terrain decal: {}", ex.what());
//	}
}

Ogre::SceneNode* MovementController::getFreeFlyingCameraNode() {
	return mFreeFlyingNode;
}

}


