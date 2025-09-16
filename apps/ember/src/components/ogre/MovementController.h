/*
    Copyright (C) 2004  Erik Ogenvik

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

#ifndef MovementController_H
#define MovementController_H

#include "EntityWorldPickListener.h"
#include "components/ogre/IMovementProvider.h"

#include "services/input/Input.h"
#include "services/input/InputCommandMapper.h"
#include "framework/ConsoleObject.h"

#include <Eris/ActiveMarker.h>

#include <wfmath/vector.h>
#include <sigc++/trackable.h>
#include "framework/AutoCloseConnection.h"

namespace varconf {
class Variable;
}

namespace Ember {
class EmberEntity;

struct IHeightProvider;

class ConfigListenerContainer;

struct TimeFrame;
namespace Navigation {
class Awareness;

class Steering;
}
namespace OgreView {

Ogre::Vector3 projectPointOntoTerrain(IHeightProvider& heightProvider, const Ogre::Vector3& point);

namespace Authoring {
class AwarenessVisualizer;
}
namespace Camera {
class FirstPersonCameraMount;

class MainCamera;
}
class FreeFlyingCameraMotionHandler;

class Avatar;

class GUIManager;

class MovementController;

/**
The movement mode of the avatar, run or walk.
*/
class MovementControllerMode {
public:
	enum Mode {
		MM_WALK = 0,
		MM_RUN = 1
	};
};

/**
Listens for left mouse button pressed in movement mode and moves the character forward.
*/
class MovementControllerInputListener : public virtual sigc::trackable {
public:
	explicit MovementControllerInputListener(MovementController& controller);

protected:

	void input_MouseButtonPressed(Input::MouseButton button, Input::InputMode mode);

	void input_MouseButtonReleased(Input::MouseButton button, Input::InputMode mode);

	MovementController& mController;
};

/**
Controls the avatar. All avatar movement is handled by an instance of this class.
*/
class MovementController
		: public virtual sigc::trackable,
		  public ConsoleObject,
		  public IMovementProvider {
public:
	friend class MovementControllerInputListener;

	/**
	 * @brief Ctor.
	 * @param avatar The main avatar.
	 * @param camera The main camera.
	 */
	MovementController(Avatar& avatar, Camera::MainCamera& camera, IHeightProvider& heightProvider);

	~MovementController() override;


	/**
	Emitted when the movement mode changes between run and walk.
	*/
	sigc::signal<void(MovementControllerMode::Mode)> EventMovementModeChanged;

	/**
	 *    Gets the current movement for this frame.
	 * @return
	 */
//	const MovementControllerMovement& getCurrentMovement() const;

	const ConsoleCommandWrapper WalkToggle;
	const ConsoleCommandWrapper ToggleCameraAttached;

	const ConsoleCommandWrapper MovementMoveForward;
	const ConsoleCommandWrapper MovementMoveBackward;
	const ConsoleCommandWrapper MovementMoveDownwards;
	const ConsoleCommandWrapper MovementMoveUpwards;
	const ConsoleCommandWrapper MovementStrafeLeft;
	const ConsoleCommandWrapper MovementStrafeRight;
/*	const ConsoleCommandWrapper MovementRotateLeft;
	const ConsoleCommandWrapper MovementRotateRight;*/


//	const ConsoleCommandWrapper MoveCameraTo;

	/**
	 * @brief Repositions the free flying camera on the avatar.
	 * This command is useful when you want to find the avatar when in free flying mode, or if you're moving the avatar through teleporting and want the camera to follow.
	 */
	const ConsoleCommandWrapper CameraOnAvatar;

	/**
	 *    Reimplements the ConsoleObject::runCommand method
	 * @param command
	 * @param args
	 */
	void runCommand(const std::string& command, const std::string& args) override;

	/**
	Moves the avatar to the specified point.
	A terrain decal will be shown.
	*/
	void moveToPoint(const Ogre::Vector3& point);

	/**
	 *    Teleports the avatar to the specified point.
	 * @param point
	 * @param locationEntity
	 */
	void teleportTo(const Ogre::Vector3& point, EmberEntity* locationEntity);


	WFMath::Vector<3> getMovementForCurrentFrame() const override;

	MovementControllerMode::Mode getMode() const;

	void setCameraFreeFlying(bool freeFlying);

	bool isCameraFreeFlying() const;

	Ogre::SceneNode* getFreeFlyingCameraNode();

protected:

        /**
         * @brief Provides terrain height information for placement helpers.
         */
        IHeightProvider& mHeightProvider;

        /**
         * @brief The main camera.
         */
        Camera::MainCamera& mCamera;

	InputCommandMapper mMovementCommandMapper;

	/**
	True if we're in running mode.
	*/
	bool mIsRunning;

	WFMath::Vector<3> mMovementDirection;

	/**
	Listen for double clicks and send the avatar to the double clicked position.
	*/
	//void entityPicker_PickedEntity(const EntityPickResult& result, const MousePickerArgs& args);

	/**
	Creates the terrain decal needed for displaying where the avatar is heading.
	*/
	void createDecal(Ogre::Vector3 position);

	void Entity_Moved();

	void Config_VisualizeRecastTiles(const std::string&, const std::string&, varconf::Variable& var);

        void Config_VisualizeRecastPath(const std::string&, const std::string&, varconf::Variable& var);
        void Config_WalkableClimb(const std::string&, const std::string&, varconf::Variable& var);
        void Config_WalkableSlopeAngle(const std::string&, const std::string&, varconf::Variable& var);

	void tileRebuild();

	void stopSteering();

	void schedulePruning();

	/**
	 * @brief Called each frame.
	 */
	void frameProcessed(const TimeFrame&, unsigned int);

	void Steering_PathUpdated();


	/**
	A decal object for showing a decal on the terrain when the user uses the "move to here" functionality.
	The decal will be shown at the destination, and removed when the user either gets close to it, or aborts the "move to here" movement (for example by moving manually).
	*/
	Ogre::MovableObject* mDecalObject;

	/**
	The scene node to which the decal object for showing the destination of a "move to here" movement operation is attached.
	*/
	Ogre::SceneNode* mDecalNode;

	MovementControllerInputListener mControllerInputListener;

	Avatar& mAvatar;

	Ogre::SceneNode* mFreeFlyingNode;
	std::unique_ptr<FreeFlyingCameraMotionHandler> mFreeFlyingMotionHandler;
	std::unique_ptr<Camera::FirstPersonCameraMount> mCameraMount;
	bool mIsFreeFlying;

	/**
	 * @brief Handles awareness about the world around the avatar.
	 */
	std::unique_ptr<Navigation::Awareness> mAwareness;

	/**
	 * @brief Optionally visualizes the awareness.
	 */
	std::unique_ptr<Authoring::AwarenessVisualizer> mAwarenessVisualizer;

	/**
	 * @brief Handles steering of the avatar.
	 */
	std::unique_ptr<Navigation::Steering> mSteering;

	/**
	 * @brief Listens to changes to the configuration.
	 */
	std::unique_ptr<ConfigListenerContainer> mConfigListenerContainer;

	/**
	 * @brief True if the path used for steering should be visualized.
	 */
        bool mVisualizePath;

        /**
         * @brief Maximum climb in voxels allowed for walkable surfaces.
         */
        int mWalkableClimb;

        /**
         * @brief Maximum walkable slope angle in degrees.
         */
        int mWalkableSlopeAngle;

	/**
	 * @brief An active marker used for cancelling EventService handlers.
	 */
	Eris::ActiveMarker mActiveMarker;

	AutoCloseConnection mParentBboxConnection;
};


}

}

#endif // MovementController_H
