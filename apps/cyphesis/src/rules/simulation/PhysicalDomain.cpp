/*
 Copyright (C) 2014 Erik Ogenvik

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "PhysicalDomain.h"

#include "TerrainProperty.h"
#include "rules/simulation/LocatedEntity.h"
#include "PropelProperty.h"
#include "rules/simulation/GeometryProperty.h"
#include "rules/simulation/AngularFactorProperty.h"
#include "TerrainModProperty.h"
#include "PhysicalWorld.h"

#include "physics/Convert.h"

#include "common/debug.h"
#include "common/globals.h"
#include "common/operations/Tick.h"
#include "rules/simulation/BaseWorld.h"
#include "PerceptionSightProperty.h"
#include "rules/ScaleProperty_impl.h"
#include "SimulationSpeedProperty.h"
#include "ModeDataProperty.h"
#include "VisibilityDistanceProperty.h"
#include "Remotery.h"
#include "common/AtlasFactories.h"

#include <Mercator/Segment.h>
#include <Mercator/TerrainMod.h>
#include <Mercator/Area.h>

#include <Atlas/Objects/Operation.h>
#include <Atlas/Objects/Anonymous.h>


#include <btBulletDynamicsCommon.h>
#include <BulletCollision/BroadphaseCollision/btAxisSweep3.h>
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>

#include <algorithm>
#include <memory>
#include <unordered_set>
#include <optional>
#include <fmt/format.h>
#include "AreaProperty.h"

static constexpr auto debug_flag = false;

INT_OPTION(visibility_broadphase_max_handles,
        65536,
        CYPHESIS,
        "visibility_broadphase_max_handles",
        "Maximum number of handles for the PhysicalDomain visibility broadphase.");

using Atlas::Message::Element;
using Atlas::Message::MapType;
using Atlas::Objects::Root;
using Atlas::Objects::Entity::RootEntity;
using Atlas::Objects::Entity::Anonymous;
using Atlas::Objects::Operation::Set;
using Atlas::Objects::Operation::Sight;
using Atlas::Objects::Operation::Appearance;
using Atlas::Objects::Operation::Disappearance;
using Atlas::Objects::Operation::Move;

using Atlas::Objects::smart_dynamic_cast;

namespace {

constexpr unsigned int MIN_VISIBILITY_BROADPHASE_HANDLES = 16384;
constexpr unsigned int DEFAULT_VISIBILITY_BROADPHASE_MAX_HANDLES = 65536;

unsigned int resolveVisibilityBroadphaseHandles(std::optional<unsigned int> overrideValue) {
        if (overrideValue.has_value()) {
                return std::max(overrideValue.value(), MIN_VISIBILITY_BROADPHASE_HANDLES);
        }
        if (visibility_broadphase_max_handles > 0) {
                return std::max(static_cast<unsigned int>(visibility_broadphase_max_handles), MIN_VISIBILITY_BROADPHASE_HANDLES);
        }
        return DEFAULT_VISIBILITY_BROADPHASE_MAX_HANDLES;
}

float to_seconds(std::chrono::milliseconds duration) {
        return std::chrono::duration_cast<std::chrono::duration<float>>(duration).count();
}

std::unique_ptr<btAxisSweep3> createVisibilityBroadphase(const LocatedEntity& entity, float scalingFactor, unsigned int maxHandles) {
        const unsigned int handles = std::max(maxHandles, 1u);
        auto bbox = ScaleProperty<LocatedEntity>::scaledBbox(entity);
        if (!bbox.isValid()) {
                return std::make_unique<bt32BitAxisSweep3>(btVector3{0, 0, 0}, btVector3{0, 0, 0}, handles);
        }
        const auto lowCorner = Convert::toBullet(bbox.lowCorner()) * scalingFactor;
        const auto highCorner = Convert::toBullet(bbox.highCorner()) * scalingFactor;
        return std::make_unique<bt32BitAxisSweep3>(lowCorner, highCorner, handles);
}

bool fuzzyEquals(WFMath::CoordType a, WFMath::CoordType b, WFMath::CoordType epsilon) {
	return std::abs(a - b) < epsilon;
}

//    bool fuzzyEquals(const WFMath::Point<3>& a, const WFMath::Point<3>& b, WFMath::CoordType epsilon)
//    {
//        return fuzzyEquals(a.x(), b.x(), epsilon) && fuzzyEquals(a.y(), b.y(), epsilon) && fuzzyEquals(a.z(), b.z(), epsilon);
//    }

bool fuzzyEquals(const WFMath::Vector<3>& a, const WFMath::Vector<3>& b, WFMath::CoordType epsilon) {
	return fuzzyEquals(a.x(), b.x(), epsilon) && fuzzyEquals(a.y(), b.y(), epsilon) && fuzzyEquals(a.z(), b.z(), epsilon);
}

/**
 * Processes an existing set and a new sorted vector, checking which elements are new, which are removed and which are kept.
 * The supplied set will be altered so that it contains the content of the vector.
 * @tparam T The type of items.
 * @tparam FunctorRemove Called when an item was removed.
 * @tparam FunctorAdd Called when an item was added.
 * @param newCollection The vector of new items. Must be sorted.
 * @param previousCollection The set of existing items. This will be altered to match the vector.
 * @param removedFn Called when an item has been removed.
 * @param addedFn Called when an item has been added.
 */
template<typename T, typename FunctorRemove, typename FunctorAdd>
void processDifferences(std::vector<T>& newCollection, std::set<T>& previousCollection, FunctorRemove removedFn, FunctorAdd addedFn) {
	auto existingI = previousCollection.begin();
	std::sort(newCollection.begin(), newCollection.end());
	auto newI = newCollection.begin();
	while (existingI != previousCollection.end() && newI != newCollection.end()) {
		if (*existingI == *newI) {
			//elements are equal, the entity was already in previousCollection
			//existedFn(*existingI);
			++existingI;
			++newI;
		} else if (*existingI < *newI) {
			//an element is missing from the new collection. This entity was removed.
			removedFn(*existingI);
			existingI = previousCollection.erase(existingI);
		} else {
			//an element wasn't present in the existing previousCollection entities. This entity was added.
			addedFn(*newI);
			existingI = previousCollection.insert(*newI).first;
		}
	}
	//Check if there were more entries in the existing collection.
	while (existingI != previousCollection.end()) {
		removedFn(*existingI);
		existingI = previousCollection.erase(existingI);
	}

	//Check if there were more new entries.
	for (; newI != newCollection.end(); ++newI) {
		addedFn(*newI);
		previousCollection.insert(*newI);
	}
}

template<typename T>
bool removeAndShift(std::vector<T>& collection, const T& entry) {
	for (size_t i = 0; i < collection.size(); ++i) {
		//Find the entry in collection and do a quick delete by moving the last entry and decreasing the size.
		if (collection[i] == entry) {
			//If it's the last entry just decrease the size without moving anything.
			if (i < collection.size() - 1) {
				collection[i] = collection[collection.size() - 1];
			}
			collection.resize(collection.size() - 1);
			return true;
		}
	}
	return false;
}
}

long PhysicalDomain::s_processTimeUs = 0;

/**
 * The minimum angular resolution of visibility, expressed as degrees.
 *
 * The "angular resolution" describes the minimum size an object needs to be in order to be viewed, as a degree of the
 * "viewing sphere" that would surround an observer.
 */
constexpr double VISIBILITY_ANGULAR_DEGREE = 1.0;

/**
 * The ratio between the size of an object and how far away it can be seen, based on the minimum angular resolution.
 */
const double VISIBILITY_RATIO = 1.0 / std::tan((WFMath::numeric_constants<double>::pi() / 180.0) * VISIBILITY_ANGULAR_DEGREE);

/**
 * How much objects in the visibility world should be scaled.
 *
 * The Bullet system is designed to handle "normal" sized things, measured in meters.
 * The visibility system uses much larger spheres. Therefore we should scale them down to make them better fit the
 * "normal" world.
 * We'll base this on VISIBILITY_RATIO.
 */
const float VISIBILITY_SCALING_FACTOR = 1.0f / static_cast<float>(VISIBILITY_RATIO);

/**
 * A list of "thresholds" for visibility distance into which any visibility sphere's radius will be slotted into.
 * The main reason is to improve performance, so visibility checks aren't redone each time an entity changes size.
 */
constexpr std::array<double, 10> VISIBILITY_DISTANCE_THRESHOLDS = {10.0, 20, 30, 50, 75, 100, 300, 500, 1000, 2000};

/**
 * The amount of time between each tick, in seconds.
 * The simulation will happen at 60hz, but the tick size determines how often entities are processed and operations are sent.
 */
constexpr std::chrono::milliseconds TICK_SIZE(66);

/**
 * The base size of the view sphere for a perceptive entity. Everything within this radius will be visible.
 */
constexpr double VIEW_SPHERE_RADIUS = 0.5;

/**
 * Mask used by visibility checks for observing entries (i.e. creatures etc.).
 */
constexpr short VISIBILITY_MASK_OBSERVER = 1u << 1u;

/**
 * Mask used by visibility checks for entries that can be observed (i.e. most entities).
 */
constexpr short VISIBILITY_MASK_OBSERVABLE = 1u << 2u;

/**
 * Mask used by visibility checks for entries that only can be observed by admin entities.
 */
constexpr short VISIBILITY_MASK_OBSERVABLE_PRIVATE = 1u << 3u;

/**
 * Mask used by all physical items. They should collide with other physical items, and with the terrain.
 */
constexpr short COLLISION_MASK_PHYSICAL = 1;
/**
 * Mask used by the terrain. It's static.
 */
constexpr short COLLISION_MASK_NON_PHYSICAL = 2;
/**
 * Mask used by all non-physical items. These should only collide with the terrain.
 */
constexpr short COLLISION_MASK_TERRAIN = 4;
/**
 * Mask used by static items (i.e. those with mode "fixed" and "planted").
 */
constexpr short COLLISION_MASK_STATIC = 8;

/**
 * The max amount of entities to do visibility checks for each tick.
 */
constexpr size_t VISIBILITY_CHECK_MAX_ENTRIES = 20;

constexpr auto CCD_MOTION_FACTOR = 0.2f;

constexpr auto CCD_SPHERE_FACTOR = 0.2f;

/**
 * Set on btCollisionObjects which are water bodies.
 */
constexpr auto USER_INDEX_WATER_BODY = 1;

struct PhysicalDomain::PhysicalMotionState : public btMotionState {
	BulletEntry& m_bulletEntry;
	btRigidBody& m_rigidBody;
	PhysicalDomain& m_domain;
	btTransform m_worldTrans;
	btTransform m_centerOfMassOffset;

	PhysicalMotionState(BulletEntry& bulletEntry,
						btRigidBody& rigidBody,
						PhysicalDomain& domain,
						const btTransform& startTrans,
						const btTransform& centerOfMassOffset = btTransform::getIdentity())
		:
		m_bulletEntry(bulletEntry),
		m_rigidBody(rigidBody),
		m_domain(domain),
		m_worldTrans(startTrans),
		m_centerOfMassOffset(centerOfMassOffset) {
	}

	///synchronizes world transform from user to physics
	void getWorldTransform(btTransform& centerOfMassWorldTrans) const override {
		centerOfMassWorldTrans = m_worldTrans * m_centerOfMassOffset.inverse();
	}

	///synchronizes world transform from physics to user
	///Bullet only calls the update of worldtransform for active objects
	void setWorldTransform(const btTransform& centerOfMassWorldTrans) override {
		if (m_bulletEntry.lastTransform != centerOfMassWorldTrans) {
			m_bulletEntry.lastTransform = centerOfMassWorldTrans;

			LocatedEntity& entity = m_bulletEntry.entity;
			if (!m_bulletEntry.addedToMovingList) {
				m_domain.m_movingEntities.emplace_back(&m_bulletEntry);
				m_bulletEntry.addedToMovingList = true;
				m_bulletEntry.markedAsMovingLastFrame = false;
			}
			m_bulletEntry.markedAsMovingThisFrame = true;
			if (!m_bulletEntry.markedForVisibilityRecalculation) {
				//Mark the entity for visibility recalculation, but don't move the visibility and view sphere here.
				//Instead rely on that being done by the calling code.
				m_domain.m_visibilityRecalculateQueue.emplace_back(&m_bulletEntry);
				m_bulletEntry.markedForVisibilityRecalculation = true;
			}

			//            cy_debug_print(
			//                    "setWorldTransform: "<< m_entity.describeEntity() << " (" << centerOfMassWorldTrans.getOrigin().x() << "," << centerOfMassWorldTrans.getOrigin().y() << "," << centerOfMassWorldTrans.getOrigin().z() << ")");

			auto& bulletTransform = m_rigidBody.getCenterOfMassTransform();
			btTransform newTransform = bulletTransform * m_centerOfMassOffset;

			auto pos = Convert::toWF<WFMath::Point<3>>(newTransform.getOrigin());
			auto velocity = Convert::toWF<WFMath::Vector<3>>(m_rigidBody.getLinearVelocity());
			auto angular = Convert::toWF<WFMath::Vector<3>>(m_rigidBody.getAngularVelocity());
			auto orientation = Convert::toWF(newTransform.getRotation());

			//If the magnitude is small enough, consider the velocity to be zero.
			if (velocity.sqrMag() < 0.001f) {
				velocity.zero();
			}
			if (angular.sqrMag() < 0.001f) {
				angular.zero();
			}

			if (pos != m_bulletEntry.positionProperty.data()) {
				m_bulletEntry.positionProperty.data() = pos;
				m_bulletEntry.positionProperty.flags().removeFlags(prop_flag_persistence_clean);
				m_bulletEntry.entity.applyProperty(m_bulletEntry.positionProperty);
			}
			if (velocity != m_bulletEntry.velocityProperty.data()) {
				m_bulletEntry.velocityProperty.data() = velocity;
				m_bulletEntry.velocityProperty.flags().removeFlags(prop_flag_persistence_clean);
				m_bulletEntry.entity.applyProperty(m_bulletEntry.velocityProperty);
			}
			if (angular != m_bulletEntry.angularVelocityProperty.data()) {
				m_bulletEntry.angularVelocityProperty.data() = angular;
				m_bulletEntry.angularVelocityProperty.flags().removeFlags(prop_flag_persistence_clean);
				m_bulletEntry.entity.applyProperty(m_bulletEntry.angularVelocityProperty);
			}
			if (orientation != m_bulletEntry.orientationProperty.data()) {
				m_bulletEntry.orientationProperty.data() = orientation;
				m_bulletEntry.orientationProperty.flags().removeFlags(prop_flag_persistence_clean);
				m_bulletEntry.entity.applyProperty(m_bulletEntry.orientationProperty);
			}

			entity.removeFlags(entity_pos_clean | entity_orient_clean);
		}
	}

};

struct PhysicalDomain::WaterCollisionCallback : public btOverlappingPairCallback {
	PhysicalDomain* m_domain = nullptr;

	void addToWater(BulletEntry* waterEntry, BulletEntry* otherEntry) const {
		otherEntry->waterNearby = waterEntry;
		if (!otherEntry->addedToMovingList) {
			m_domain->m_movingEntities.emplace_back(otherEntry);
			otherEntry->addedToMovingList = true;
			otherEntry->markedAsMovingThisFrame = false;
			otherEntry->markedAsMovingLastFrame = false;
		}
	}

	void removeFromWater(const BulletEntry* waterEntry, BulletEntry* otherEntry) const {
		if (otherEntry->waterNearby == waterEntry) {
			otherEntry->waterNearby = otherEntry;
			if (!otherEntry->addedToMovingList) {
				m_domain->m_movingEntities.emplace_back(otherEntry);
				otherEntry->addedToMovingList = true;
				otherEntry->markedAsMovingThisFrame = false;
				otherEntry->markedAsMovingLastFrame = false;
			}
		}
	}

	btBroadphasePair* addOverlappingPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) override {
		auto* colObj0 = (btCollisionObject*)proxy0->m_clientObject;
		auto* colObj1 = (btCollisionObject*)proxy1->m_clientObject;
		if (colObj0->getUserIndex() == USER_INDEX_WATER_BODY) {
			//Don't let two water bodies affect each other
			if (colObj1->getUserIndex() != USER_INDEX_WATER_BODY) {
				auto waterBodyEntry = (BulletEntry*)colObj0->getUserPointer();
				auto otherEntry = (BulletEntry*)colObj1->getUserPointer();
				addToWater(waterBodyEntry, otherEntry);
			}
		} else if (colObj1->getUserIndex() == USER_INDEX_WATER_BODY) {
			auto waterBodyEntry = (BulletEntry*)colObj1->getUserPointer();
			auto otherEntry = (BulletEntry*)colObj0->getUserPointer();
			addToWater(waterBodyEntry, otherEntry);
		}
		return nullptr;
	}

	void* removeOverlappingPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1, btDispatcher* dispatcher) override {
		auto* colObj0 = (btCollisionObject*)proxy0->m_clientObject;
		auto* colObj1 = (btCollisionObject*)proxy1->m_clientObject;
		if (colObj0->getUserIndex() == USER_INDEX_WATER_BODY) {
			//Don't let two water bodies affect each other
			if (colObj1->getUserIndex() != USER_INDEX_WATER_BODY) {
				auto waterBodyEntry = (BulletEntry*)colObj0->getUserPointer();
				auto otherEntry = (BulletEntry*)colObj1->getUserPointer();
				removeFromWater(waterBodyEntry, otherEntry);
			}
		} else if (colObj1->getUserIndex() == USER_INDEX_WATER_BODY) {
			auto waterBodyEntry = (BulletEntry*)colObj1->getUserPointer();
			auto otherEntry = (BulletEntry*)colObj0->getUserPointer();
			removeFromWater(waterBodyEntry, otherEntry);
		}
		return nullptr;
	}

	void removeOverlappingPairsContainingProxy(btBroadphaseProxy* /*proxy0*/, btDispatcher* /*dispatcher*/) override {
	}
};

struct PhysicalDomain::VisibilityPairCallback : public btOverlappingPairCallback {
	btBroadphasePair* addOverlappingPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1) override {
		bool collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
		collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask);
		if (collides) {
			auto* movedObject = (btCollisionObject*)proxy0->m_clientObject;
			auto movedEntry = (BulletEntry*)movedObject->getUserPointer();
			auto* otherObject = (btCollisionObject*)proxy1->m_clientObject;
			auto otherEntry = (BulletEntry*)otherObject->getUserPointer();
			if (movedEntry != otherEntry) {
				if (movedObject == movedEntry->viewSphere.get()) {
					//An observer was moved
					movedEntry->observedByThisChanges.emplace_back(otherEntry, BulletEntry::VisibilityQueueOperationType::Add);
				} else {
					//An observable was moved
					movedEntry->observingThisChanges.emplace_back(otherEntry, BulletEntry::VisibilityQueueOperationType::Add);
				}
			}
		}
		return nullptr;
	}

	void* removeOverlappingPair(btBroadphaseProxy* proxy0, btBroadphaseProxy* proxy1, btDispatcher* dispatcher) override {
		bool collides = (proxy0->m_collisionFilterGroup & proxy1->m_collisionFilterMask) != 0;
		collides = collides && (proxy1->m_collisionFilterGroup & proxy0->m_collisionFilterMask);
		if (collides) {
			auto* movedObject = (btCollisionObject*)proxy0->m_clientObject;
			auto movedEntry = (BulletEntry*)movedObject->getUserPointer();
			auto* otherObject = (btCollisionObject*)proxy1->m_clientObject;
			auto otherEntry = (BulletEntry*)otherObject->getUserPointer();
			if (movedEntry != otherEntry) {
				if (movedObject == movedEntry->viewSphere.get()) {
					//An observer was moved
					movedEntry->observedByThisChanges.emplace_back(otherEntry, BulletEntry::VisibilityQueueOperationType::Remove);
				} else {
					//An observable was moved
					movedEntry->observingThisChanges.emplace_back(otherEntry, BulletEntry::VisibilityQueueOperationType::Remove);
				}
			}
		}
		return nullptr;
	}

	void removeOverlappingPairsContainingProxy(btBroadphaseProxy* proxy0, btDispatcher* dispatcher) override {
	}
};

std::chrono::steady_clock::duration postDuration;

PhysicalDomain::PhysicalDomain(LocatedEntity& entity, std::optional<unsigned int> visibilityBroadphaseMaxHandles) :
        Domain(entity),
        mWorldInfo{.propellingEntries = &m_propellingEntries, .steppingEntries = &m_steppingEntries},
        //default config for now
        m_collisionConfiguration(new btDefaultCollisionConfiguration()),
        m_dispatcher(new btCollisionDispatcher(m_collisionConfiguration.get())),
	m_constraintSolver(new btSequentialImpulseConstraintSolver()),
	//We'll use a dynamic broadphase for the main world. It's not as fast as SAP variants, but it's faster when dynamic objects are at rest.
	m_broadphase(new btDbvtBroadphase()),
	// m_broadphase(new btAxisSweep3(Convert::toBullet(entity.m_location.bBox().lowCorner()),
	//                                              Convert::toBullet(entity.m_location.bBox().highCorner()))),
        m_dynamicsWorld(new PhysicalWorld(m_dispatcher.get(), m_broadphase.get(), m_constraintSolver.get(), m_collisionConfiguration.get())),
        m_visibilityPairCallback(new VisibilityPairCallback()),
        m_visibilityDispatcher(new btCollisionDispatcher(m_collisionConfiguration.get())),
        m_visibilityBroadphaseMaxHandles(resolveVisibilityBroadphaseHandles(visibilityBroadphaseMaxHandles)),
        //We'll use a 32-bit SAP broadphase for the visibility to support large scenes efficiently.
        m_visibilityBroadphase(createVisibilityBroadphase(entity, VISIBILITY_SCALING_FACTOR, m_visibilityBroadphaseMaxHandles)),
	m_visibilityWorld(new btCollisionWorld(m_visibilityDispatcher.get(),
		m_visibilityBroadphase.get(),
		m_collisionConfiguration.get())),
	m_visibilityCheckCountdown(0),
	mContainingEntityEntry{
		.entity = entity,
		.positionProperty = mFakeProperties.positionProperty,
		.velocityProperty = mFakeProperties.velocityProperty,
		.angularVelocityProperty = mFakeProperties.angularVelocityProperty,
		.orientationProperty = mFakeProperties.orientationProperty
	},
	m_terrain(nullptr),
	m_ghostPairCallback(new WaterCollisionCallback()) {
	mContainingEntityEntry.bbox = ScaleProperty<LocatedEntity>::scaledBbox(m_entity);

	m_ghostPairCallback->m_domain = this;
	m_dynamicsWorld->getPairCache()->setInternalGhostPairCallback(m_ghostPairCallback.get());
	m_visibilityBroadphase->setOverlappingPairUserCallback(m_visibilityPairCallback.get());

	//This is to prevent us from sliding down slopes.
	//m_dynamicsWorld->getDispatchInfo().m_allowedCcdPenetration = 0.0001f;

	//By default all collision objects have their aabbs updated each tick; we'll disable it for performance.
	m_dynamicsWorld->setForceUpdateAllAabbs(false);

	m_visibilityWorld->setForceUpdateAllAabbs(false);

	if (m_entity.getPropertyClassFixed<TerrainProperty>()) {
		m_terrain = &TerrainProperty::getData(m_entity);
	}

	createDomainBorders();

	//Update the linear velocity of all self propelling entities each tick.
	auto preTickCallback = [](btDynamicsWorld* world, btScalar timeStep) {
		rmt_ScopedCPUSample(PhysicalDomain_preTickCallback, 0)
		auto worldInfo = static_cast<WorldInfo*>(world->getWorldUserInfo());
		auto propellingEntries = worldInfo->propellingEntries;
		for (auto& entry: *propellingEntries) {
			float verticalVelocity = entry.second.rigidBody->getLinearVelocity().y();

			//TODO: check if we're on the ground, in the water or flying and apply different speed modifiers
			double speed;
			if (entry.second.bulletEntry->mode == ModeProperty::Mode::Submerged) {
				speed = entry.second.bulletEntry->speedWater;
			} else {
				speed = entry.second.bulletEntry->speedGround;
			}
			btVector3 finalSpeed = entry.second.velocity * (btScalar)speed;

			//Apply gravity
			if (!WFMath::Equal(verticalVelocity, 0, WFMath::numeric_constants<float>::epsilon())) {
				verticalVelocity += entry.second.rigidBody->getGravity().y() * timeStep;
				entry.second.rigidBody->setLinearVelocity(finalSpeed + btVector3(0, verticalVelocity, 0));
			} else {
				entry.second.rigidBody->setLinearVelocity(finalSpeed);
			}

			//When entities are being propelled they will have low friction. When propelling stops the friction will be returned in setVelocity.
			entry.second.bulletEntry->collisionObject->setFriction(0.5);
			entry.second.bulletEntry->collisionObject->activate();
		}
	};

	auto postTickCallback = [](btDynamicsWorld* world, btScalar timeStep) {
		rmt_ScopedCPUSample(PhysicalDomain_postTickCallback, 0)
		auto start = std::chrono::steady_clock::now();
		auto worldInfo = static_cast<WorldInfo*>(world->getWorldUserInfo());
		auto steppingEntries = worldInfo->steppingEntries;
		for (auto& entry: *steppingEntries) {
			auto collisionObject = btRigidBody::upcast(entry->collisionObject.get());
			//Check that the object has moved, and if so check if it should be clamped to the ground.
			//But only do this if there's no major upwards jumping motion.
			auto isJumpingUpwards = entry->isJumping && collisionObject->getInterpolationLinearVelocity().y() > 0;
			if (collisionObject->getInterpolationLinearVelocity().length2() > 0.001 && !isJumpingUpwards) {
				struct : btCollisionWorld::ContactResultCallback {
					bool isHit = false;

					btScalar addSingleResult(btManifoldPoint& cp,
											 const btCollisionObjectWrapper* colObj0Wrap,
											 int partId0,
											 int index0,
											 const btCollisionObjectWrapper* colObj1Wrap,
											 int partId1,
											 int index1) override {
						//If the normal points upwards it's below us.
						if (cp.m_normalWorldOnB.y() > 0) {
							isHit = true;
						}
						return 1.0f;
					}
				} collideCallback;
				collideCallback.m_collisionFilterMask = collisionObject->getBroadphaseHandle()->m_collisionFilterMask;
				collideCallback.m_collisionFilterGroup = collisionObject->getBroadphaseHandle()->m_collisionFilterGroup;

				world->contactTest(collisionObject, collideCallback);
				if (!collideCallback.isHit) {
					//The entity isn't standing on top of anything. Check that we're not jumping; if not we should try to clamp it to the ground.
					if (!entry->isJumping) {
						//Cast a ray from the bottom and check if we're already colliding with the object we hit with our ray.
						//If we don't collide it means that we're in the air, and should be clamped to the ground.
						auto& worldTransform = collisionObject->getWorldTransform();
						btVector3 aabbMin, aabbMax;
						collisionObject->getCollisionShape()->getAabb(worldTransform, aabbMin, aabbMax);
						btVector3 bottomOfObject = worldTransform.getOrigin();
						bottomOfObject.setY(aabbMin.y());
						btVector3 bottomOfRay = bottomOfObject;
						auto rayDistance = (float)entry->step_factor * (aabbMax.y() - aabbMin.y());

						bottomOfRay.setY(bottomOfRay.y() - rayDistance);

						btCollisionWorld::ClosestRayResultCallback callback(bottomOfObject, bottomOfRay);
						callback.m_collisionFilterMask = collisionObject->getBroadphaseHandle()->m_collisionFilterMask;
						callback.m_collisionFilterGroup = collisionObject->getBroadphaseHandle()->m_collisionFilterGroup;

						world->rayTest(bottomOfObject, bottomOfRay, callback);

						if (callback.hasHit()) {
							float distance = bottomOfObject.y() - callback.m_hitPointWorld.y();
							if (distance > 0.2) {
								worldTransform.getOrigin().setY(worldTransform.getOrigin().y() - distance);
								collisionObject->setWorldTransform(worldTransform);
								//Dampen the vertical velocity a bit
								auto velocity = collisionObject->getLinearVelocity();
								velocity.setY(velocity.y() * 0.1f);
								collisionObject->setLinearVelocity(velocity);
							}
						}
					}
				} else {
					//The entity is standing on top of something, make sure it's not marked as jumping anymore.
					entry->isJumping = false;
				}
			}
		}
		if (debug_flag) {
			postDuration += std::chrono::steady_clock::now() - start;
		}
	};

	m_dynamicsWorld->setInternalTickCallback(preTickCallback, &mWorldInfo, true);
	m_dynamicsWorld->setInternalTickCallback(postTickCallback, &mWorldInfo, false);

	mContainingEntityEntry.mode = ModeProperty::Mode::Fixed;

	m_entries.emplace(entity.getIdAsInt(), std::unique_ptr<BulletEntry>(&mContainingEntityEntry));

	buildTerrainPages();

	m_entity.propertyApplied.connect(sigc::mem_fun(*this, &PhysicalDomain::entityPropertyApplied));
}

PhysicalDomain::~PhysicalDomain() {
	for (auto& planeBody: m_borderPlanes) {
		m_dynamicsWorld->removeCollisionObject(planeBody.first.get());
	}
	m_borderPlanes.clear();

	for (auto& entry: m_terrainSegments) {
		m_dynamicsWorld->removeCollisionObject(entry.second.rigidBody.get());
	}
	m_terrainSegments.clear();

	//Remove our own entry first, since we own the memory
	m_entries[m_entity.getIdAsInt()].release();
	m_entries.erase(m_entity.getIdAsInt());

	for (auto& entry: m_entries) {
		if (entry.second->collisionObject) {
			m_dynamicsWorld->removeCollisionObject(entry.second->collisionObject.get());
		}

		entry.second->collisionShape.reset();

		entry.second->propertyUpdatedConnection.disconnect();
	}

	m_propertyAppliedConnection.disconnect();
}

void PhysicalDomain::installDelegates(LocatedEntity& entity, const std::string& propertyName) {
	entity.installDelegate(Atlas::Objects::Operation::TICK_NO, propertyName);
	auto tickOp = scheduleTick(entity);
	entity.sendWorld(tickOp);
}

Atlas::Objects::Operation::RootOperation PhysicalDomain::scheduleTick(const LocatedEntity& entity) {
	Anonymous tick_arg;
	tick_arg->setName("domain");
	Atlas::Objects::Operation::Tick tickOp;
	tickOp->setTo(entity.getIdAsString());
	tickOp->setArgs1(tick_arg);

	return tickOp;
}

HandlerResult PhysicalDomain::operation(LocatedEntity& entity, const Operation& op, OpVector& res) {
	return tick_handler(entity, op, res);
}

HandlerResult PhysicalDomain::tick_handler(const LocatedEntity& entity, const Operation& op, OpVector& res) {
	if (!op->getArgs().empty() && !op->getArgs().front()->isDefaultName() && op->getArgs().front()->getName() == "domain") {
		auto timeNow = std::chrono::milliseconds(op->getStamp());
		auto tickSize = TICK_SIZE;
		Element elem;
		if (op->copyAttr("lastTick", elem) == 0 && elem.isInt()) {
			tickSize = timeNow - std::chrono::milliseconds(elem.Int());
		}

		tick(tickSize, res);
		auto tickOp = scheduleTick(entity);
		tickOp->setStamp((timeNow + TICK_SIZE).count());
		tickOp->setAttr("lastTick", timeNow.count());
		res.emplace_back(std::move(tickOp));

		return OPERATION_BLOCKED;
	}
	return OPERATION_IGNORED;
}

void PhysicalDomain::buildTerrainPages() {
	std::optional<float> friction;
	std::optional<float> rollingFriction;
	std::optional<float> spinningFriction;

	{
		auto frictionProp = m_entity.getPropertyType<double>("friction");

		if (frictionProp) {
			friction = (float)frictionProp->data();
		}
	}

	{
		auto frictionProp = m_entity.getPropertyType<double>("friction_roll");

		if (frictionProp) {
			rollingFriction = (float)frictionProp->data();
		}
	}

	{
		auto frictionProp = m_entity.getPropertyType<double>("friction_spin");

		if (frictionProp) {
			spinningFriction = (float)frictionProp->data();
		}
	}
	const auto* terrainProperty = m_entity.getPropertyClassFixed<TerrainProperty>();
	if (terrainProperty) {
		auto& terrain = TerrainProperty::getData(m_entity);
		auto& segments = terrain.getTerrain();
		for (auto& row: segments) {
			for (auto& entry: row.second) {
				auto& segment = entry.second;
				auto& terrainEntry = buildTerrainPage(*segment);
				if (friction) {
					terrainEntry.rigidBody->setFriction(*friction);
				}
				if (spinningFriction) {
					terrainEntry.rigidBody->setSpinningFriction(*spinningFriction);
				}
				if (rollingFriction) {
					terrainEntry.rigidBody->setRollingFriction(*rollingFriction);
				}
			}
		}
	}
}

PhysicalDomain::TerrainEntry& PhysicalDomain::buildTerrainPage(Mercator::Segment& segment) {
	if (!segment.isValid()) {
		segment.populate();
	}

	int vertexCountOneSide = segment.getSize();

	TerrainEntry& terrainEntry = m_terrainSegments[fmt::format("{}:{}", segment.getXRef(), segment.getZRef())];
	if (!terrainEntry.data) {
		terrainEntry.data = std::make_unique<std::array<btScalar, 65 * 65>>();
	}
	if (terrainEntry.rigidBody) {
		m_dynamicsWorld->removeRigidBody(terrainEntry.rigidBody.get());
		terrainEntry.rigidBody.reset();
		terrainEntry.shape.reset();
	}
	auto* data = terrainEntry.data->data();
	const auto* mercatorData = segment.getPoints();
	float min = segment.getMin();
	float max = segment.getMax();

	//Even though the API seems to allow various types of data to be specified in the ctor for btHeightfieldTerrainShape it seems that when using doubles as btScalar we must also supply doubles.
#if defined(BT_USE_DOUBLE_PRECISION)
	for (size_t i = 0; i < (size_t) (vertexCountOneSide * vertexCountOneSide); ++i) {
		data[i] = mercatorData[i];
	}
	terrainEntry.shape = std::make_unique<btHeightfieldTerrainShape>(vertexCountOneSide, vertexCountOneSide, data, 1.0f, min, max, 1, PHY_DOUBLE, false);
#else
	memcpy(data, mercatorData, vertexCountOneSide * vertexCountOneSide * sizeof(float));
	terrainEntry.shape = std::make_unique<btHeightfieldTerrainShape>(vertexCountOneSide, vertexCountOneSide, data, 1.0f, min, max, 1, PHY_FLOAT, false);
#endif


	terrainEntry.shape->setLocalScaling(btVector3(1, 1, 1));

	auto res = segment.getResolution();

	auto xPos = (segment.getXRef() + (res / 2));
	float yPos = min + ((max - min) * 0.5f);
	auto zPos = segment.getZRef() + (res / 2);

	WFMath::Point<3> pos(xPos, yPos, zPos);
	btVector3 btPos = Convert::toBullet(pos);

	btRigidBody::btRigidBodyConstructionInfo segmentCI(.0f, nullptr, terrainEntry.shape.get());
	auto segmentBody = std::make_unique<btRigidBody>(segmentCI);
	segmentBody->setWorldTransform(btTransform(btQuaternion::getIdentity(), btPos));

	m_dynamicsWorld->addRigidBody(segmentBody.get(),
		COLLISION_MASK_TERRAIN,
		COLLISION_MASK_NON_PHYSICAL | COLLISION_MASK_PHYSICAL);

	terrainEntry.rigidBody = std::move(segmentBody);
	terrainEntry.rigidBody->setUserPointer(&mContainingEntityEntry);
	return terrainEntry;
}

void PhysicalDomain::createDomainBorders() {
	auto bbox = ScaleProperty<LocatedEntity>::scaledBbox(m_entity);
	if (bbox.isValid()) {
		//We'll now place six planes representing the bounding box.

		m_borderPlanes.reserve(6);
		auto createPlane =
				[&](const btVector3& normal, const btVector3& translate) {
			auto plane = std::make_unique<btStaticPlaneShape>(normal, .0f);
			auto planeBody = std::make_unique<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(0, nullptr, plane.get()));
			planeBody->setWorldTransform(btTransform(btQuaternion::getIdentity(), translate));
			planeBody->setUserPointer(&mContainingEntityEntry);
			m_dynamicsWorld->addRigidBody(planeBody.get(), COLLISION_MASK_TERRAIN, COLLISION_MASK_NON_PHYSICAL | COLLISION_MASK_PHYSICAL);
			m_borderPlanes.emplace_back(std::move(planeBody), std::move(plane));
		};

		//Bottom plane
		createPlane(btVector3(0, 1, 0), btVector3(0, (btScalar)bbox.lowerBound(1), 0));

		//Top plane
		createPlane(btVector3(0, -1, 0), btVector3(0, (btScalar)bbox.upperBound(1), 0));

		//Crate surrounding planes
		createPlane(btVector3(1, 0, 0), btVector3((btScalar)bbox.lowerBound(0), 0, 0));
		createPlane(btVector3(-1, 0, 0), btVector3((btScalar)bbox.upperBound(0), 0, 0));
		createPlane(btVector3(0, 0, 1), btVector3(0, 0, (btScalar)bbox.lowerBound(2)));
		createPlane(btVector3(0, 0, -1), btVector3(0, 0, (btScalar)bbox.upperBound(2)));
	}
}

bool PhysicalDomain::isEntityVisibleFor(const LocatedEntity& observingEntity, const LocatedEntity& observedEntity) const {
	//Is it observing the domain entity?
	if (&observedEntity == &m_entity) {
		return true;
	}

	//Is it observing itself?
	if (&observingEntity == &observedEntity) {
		return true;
	}

	//Is it the domain entity?
	if (&observingEntity == &m_entity) {
		return true;
	}

	auto observingI = m_entries.find(observingEntity.getIdAsInt());
	if (observingI == m_entries.end()) {
		return false;
	}
	auto observedI = m_entries.find(observedEntity.getIdAsInt());
	if (observedI == m_entries.end()) {
		return false;
	}

	auto& observedEntry = observedI->second;
	auto& observingEntry = observingI->second;
	return observedEntry->observingThis.contains(observingEntry.get());
}

void PhysicalDomain::getVisibleEntitiesFor(const LocatedEntity& observingEntity, std::list<LocatedEntity*>& entityList) const {
	auto observingI = m_entries.find(observingEntity.getIdAsInt());
	if (observingI != m_entries.end()) {
		const auto& bulletEntry = observingI->second;
		for (const auto& observedEntry: bulletEntry->observedByThis) {
			entityList.push_back(&observedEntry->entity);
		}
	}
}

std::vector<LocatedEntity*> PhysicalDomain::getObservingEntitiesFor(const LocatedEntity& observedEntity) const {
	std::vector<LocatedEntity*> entityList;

	auto observedI = m_entries.find(observedEntity.getIdAsInt());
	if (observedI != m_entries.end()) {
		auto& bulletEntry = observedI->second;
		for (const auto& observingEntry: bulletEntry->observingThis) {
			entityList.push_back(&observingEntry->entity);
		}
	}

	return entityList;
}

void PhysicalDomain::updateObserverEntry(BulletEntry& bulletEntry, OpVector& res) const {
	if (bulletEntry.viewSphere) {
		//This entry is an observer; check what it can see after it has moved
		const auto& viewSphere = bulletEntry.viewSphere;
		if (viewSphere) {
			viewSphere->setWorldTransform(btTransform(btQuaternion::getIdentity(), Convert::toBullet(bulletEntry.positionProperty.data()) * VISIBILITY_SCALING_FACTOR));
			m_visibilityWorld->updateSingleAabb(viewSphere.get());
		}

		std::vector<Root> appearArgs;
		std::vector<Root> disappearArgs;

		auto disappearFn = [&](BulletEntry* disappearedEntry) {
			Anonymous that_ent;
			that_ent->setId(disappearedEntry->entity.getIdAsString());
			that_ent->setStamp(disappearedEntry->entity.getSeq());

			disappearArgs.push_back(std::move(that_ent));

			disappearedEntry->observingThis.erase(&bulletEntry);
		};

		auto appearFn = [&](BulletEntry* appearedEntry) {
			//Send Appear
			// cy_debug_print(" appear: " << viewedEntry->entity.describeEntity() << " for " << bulletEntry.entity.describeEntity());
			Anonymous that_ent;
			that_ent->setId(appearedEntry->entity.getIdAsString());
			that_ent->setStamp(appearedEntry->entity.getSeq());
			appearArgs.push_back(std::move(that_ent));

			appearedEntry->observingThis.insert(&bulletEntry);
		};

		for (auto& entry: bulletEntry.observedByThisChanges) {
			if (entry.second == BulletEntry::VisibilityQueueOperationType::Add) {
				appearFn(entry.first);
				bulletEntry.observedByThis.insert(entry.first);
			} else {
				disappearFn(entry.first);
				bulletEntry.observedByThis.erase(entry.first);
			}
		}

		if (!appearArgs.empty()) {
			Appearance appear;
			appear->setTo(bulletEntry.entity.getIdAsString());
			appear->setArgs(std::move(appearArgs));
			res.push_back(std::move(appear));
		}
		if (!disappearArgs.empty()) {
			Disappearance disappear;
			disappear->setTo(bulletEntry.entity.getIdAsString());
			disappear->setArgs(std::move(disappearArgs));
			res.push_back(std::move(disappear));
		}

		bulletEntry.observedByThisChanges.clear();

	}
}

void PhysicalDomain::updateObservedEntry(BulletEntry& bulletEntry, OpVector& res, bool generateOps) const {
	if (bulletEntry.visibilitySphere) {
		//This entry is an observable; check what can see it after it has moved

		auto& visibilitySphere = bulletEntry.visibilitySphere;
		if (visibilitySphere) {
			visibilitySphere->setWorldTransform(btTransform(btQuaternion::getIdentity(), Convert::toBullet(bulletEntry.positionProperty.data()) * VISIBILITY_SCALING_FACTOR));
			m_visibilityWorld->updateSingleAabb(visibilitySphere.get());
		}

		auto disappearFn = [&](BulletEntry* existingObserverEntry) {
			if (generateOps) {
				//Send disappearence
				// cy_debug_print(" disappear: " << bulletEntry.entity.describeEntity() << " for " << noLongerObservingEntry->entity.describeEntity());
				Disappearance disappear;
				Anonymous that_ent;
				that_ent->setId(bulletEntry.entity.getIdAsString());
				that_ent->setStamp(bulletEntry.entity.getSeq());
				disappear->setArgs1(std::move(that_ent));
				disappear->setTo(existingObserverEntry->entity.getIdAsString());
				res.push_back(std::move(disappear));
			}

			existingObserverEntry->observedByThis.erase(&bulletEntry);
		};

		auto appearFn = [&](BulletEntry* newObserverEntry) {
			if (generateOps) {
				//Send appear
				// cy_debug_print(" appear: " << bulletEntry.entity.describeEntity() << " for " << viewingEntry->entity.describeEntity());
				Appearance appear;
				Anonymous that_ent;
				that_ent->setId(bulletEntry.entity.getIdAsString());
				that_ent->setStamp(bulletEntry.entity.getSeq());
				appear->setArgs1(std::move(that_ent));
				appear->setTo(newObserverEntry->entity.getIdAsString());
				res.push_back(std::move(appear));
			}

			newObserverEntry->observedByThis.insert(&bulletEntry);
		};

		for (auto& entry: bulletEntry.observingThisChanges) {
			if (entry.second == BulletEntry::VisibilityQueueOperationType::Add) {
				appearFn(entry.first);
				bulletEntry.observingThis.insert(entry.first);
			} else {
				disappearFn(entry.first);
				bulletEntry.observingThis.erase(entry.first);
			}
		}

		bulletEntry.observingThisChanges.clear();
	}
}

void PhysicalDomain::updateVisibilityOfDirtyEntities(OpVector& res) {
	rmt_ScopedCPUSample(PhysicalDomain_updateVisibilityOfDirtyEntities, 0)

	if (!m_visibilityRecalculateQueue.empty()) {
		size_t i = 0;
		//Handle max VISIBILITY_CHECK_MAX_ENTRIES entities each time.
		auto I = m_visibilityRecalculateQueue.begin();
		for (; I != m_visibilityRecalculateQueue.end() && i < VISIBILITY_CHECK_MAX_ENTRIES; ++i, ++I) {
			auto& bulletEntry = *I;
			updateObservedEntry(*bulletEntry, res);
			updateObserverEntry(*bulletEntry, res);
			bulletEntry->markedForVisibilityRecalculation = false;
			bulletEntry->entity.onUpdated();
		}
		m_visibilityRecalculateQueue.erase(m_visibilityRecalculateQueue.begin(), I);
	}
}

float PhysicalDomain::getMassForEntity(const LocatedEntity& entity) {
	float mass = 0;

	auto massProp = entity.getPropertyType<double>("mass");
	if (massProp) {
		mass = (float)massProp->data();
	}
	return mass;
}

std::shared_ptr<btCollisionShape> PhysicalDomain::createCollisionShapeForEntry(const LocatedEntity& entity,
																			   const WFMath::AxisBox<3>& bbox,
																			   float mass,
																			   btVector3& centerOfMassOffset) {
	if (auto geometryProp = entity.getPropertyClassFixed<GeometryProperty>()) {
		return geometryProp->createShape(bbox, centerOfMassOffset, mass);
	} else {
		auto size = bbox.highCorner() - bbox.lowCorner();
		auto btSize = Convert::toBullet(size * 0.5).absolute();
		centerOfMassOffset = -Convert::toBullet(bbox.getCenter());
		return std::make_shared<btBoxShape>(btSize);
	}
}

//TODO: send along placement data (position, orientation etc.)
void PhysicalDomain::addEntity(LocatedEntity& entity) {
	assert(m_entries.find(entity.getIdAsInt()) == m_entries.end());

	auto existingPosProp = entity.modPropertyClassFixed<PositionProperty<LocatedEntity>>();
	if (!existingPosProp || !existingPosProp->data().isValid()) {
		spdlog::warn("Tried to add entity {} to physical domain belonging to {}, but there's no valid position.", entity.describeEntity(), m_entity.describeEntity());
		return;
	}

	auto& posProp = *existingPosProp;
	auto& velocityProp = entity.requirePropertyClassFixed<VelocityProperty<LocatedEntity>>(Atlas::Message::ListType{0, 0, 0});
	auto& angularProp = entity.requirePropertyClassFixed<AngularVelocityProperty<LocatedEntity>>(Atlas::Message::ListType{0, 0, 0});
	auto& orientationProp = entity.requirePropertyClassFixed<OrientationProperty<LocatedEntity>>(Atlas::Message::ListType{0, 0, 0, 1});


	float mass = getMassForEntity(entity);

	WFMath::AxisBox<3> bbox = ScaleProperty<LocatedEntity>::scaledBbox(entity);
	btVector3 angularFactor(1, 1, 1);

	auto result = m_entries.emplace(entity.getIdAsInt(),
		std::make_unique<BulletEntry>(BulletEntry{
				.entity = entity,
				.positionProperty = posProp,
				.velocityProperty = velocityProp,
				.angularVelocityProperty = angularProp,
				.orientationProperty = orientationProp,
				.bbox = bbox
			}
		)
	);
	auto& entry = *result.first->second;

	auto angularFactorProp = entity.getPropertyClassFixed<AngularFactorProperty>();
	if (angularFactorProp && angularFactorProp->data().isValid()) {
		angularFactor = Convert::toBullet(angularFactorProp->data());
	}

	if (auto solidProperty = entity.getPropertyClassFixed<SolidProperty<LocatedEntity>>()) {
		entry.isSolid = solidProperty->isTrue();
	} else {
		//Entities are by default solid unless marked not to be.
		entry.isSolid = true;
	}

	ModeProperty::Mode mode = ModeProperty::Mode::Free;
	if (auto modeProp = entity.getPropertyClassFixed<ModeProperty>()) {
		mode = modeProp->getMode();
	}

	entry.modeChanged = false;
	entry.mode = mode;

	if (mode == ModeProperty::Mode::Planted || mode == ModeProperty::Mode::Fixed) {
		//"fixed" mode means that the entity stays in place, always
		//"planted" mode means it's planted in the ground
		//Zero mass makes the rigid body static
		mass = .0f;
	}

	btQuaternion orientation = orientationProp.data().isValid() ? Convert::toBullet(orientationProp.data()) : btQuaternion::getIdentity();

	short collisionMask;
	short collisionGroup;
	getCollisionFlagsForEntity(entry, collisionGroup, collisionMask);

	auto waterBodyProp = entity.getPropertyClass<BoolProperty<LocatedEntity>>("water_body");
	if (waterBodyProp && waterBodyProp->isTrue()) {

		auto ghostObject = std::make_unique<btGhostObject>();
		entry.collisionObject = std::move(ghostObject);
		entry.collisionObject->setUserIndex(USER_INDEX_WATER_BODY);
		entry.collisionObject->setUserPointer(&entry);

		//If there's a valid bbox, use that to create a contained body of water.
		// Otherwise, create an infinitely large body of water (i.e. an "ocean") using a plane.
		if (bbox.isValid()) {
			//"Center of mass offset" is the inverse of the center of the object in relation to origo.
			auto size = bbox.highCorner() - bbox.lowCorner();
			entry.collisionShape = std::make_shared<btBoxShape>(Convert::toBullet(size / 2));
			entry.centerOfMassOffset = -Convert::toBullet(bbox.getCenter());
		} else {
			entry.collisionShape = std::make_shared<btStaticPlaneShape>(btVector3(0, 1, 0), 0);
			entry.centerOfMassOffset = btVector3(0, 0, 0);
		}
		entry.collisionObject->setCollisionShape(entry.collisionShape.get());
		entry.collisionObject->setCollisionFlags(entry.collisionObject->getCollisionFlags() | btCollisionObject::CF_NO_CONTACT_RESPONSE);

		calculatePositionForEntity(mode, entry, posProp.data());
		entry.collisionObject->setWorldTransform(btTransform(orientation, Convert::toBullet(posProp.data()))
			* btTransform(btQuaternion::getIdentity(), entry.centerOfMassOffset).inverse());

		m_dynamicsWorld->addCollisionObject(entry.collisionObject.get(), collisionGroup, collisionMask);
		entry.collisionObject->activate();
	} else {
		if (bbox.isValid()) {
			//"Center of mass offset" is the inverse of the center of the object in relation to origo.
			auto size = bbox.highCorner() - bbox.lowCorner();
			btVector3 inertia(0, 0, 0);

			entry.collisionShape = createCollisionShapeForEntry(entry.entity, bbox, mass, entry.centerOfMassOffset);

			if (mass > 0) {
				entry.collisionShape->calculateLocalInertia(mass, inertia);
			}


			cy_debug_print("PhysicsDomain adding entity " << entity.describeEntity() << " with mass " << mass
				<< " and inertia (" << inertia.x() << "," << inertia.y() << "," << inertia.z() << ")")

			btRigidBody::btRigidBodyConstructionInfo rigidBodyCI(mass, nullptr, entry.collisionShape.get(), inertia);

			auto frictionProp = entity.getPropertyType<double>("friction");
			if (frictionProp) {
				rigidBodyCI.m_friction = (btScalar)frictionProp->data();
			}
			auto frictionRollProp = entity.getPropertyType<double>("friction_roll");
			if (frictionRollProp) {
				rigidBodyCI.m_rollingFriction = (btScalar)frictionRollProp->data();
			}
			auto frictionSpinProp = entity.getPropertyType<double>("friction_spin");
			if (frictionSpinProp) {
				rigidBodyCI.m_spinningFriction = (btScalar)frictionSpinProp->data();
			}

			auto rigidBody = new btRigidBody(rigidBodyCI);
			entry.collisionObject = std::unique_ptr<btCollisionObject>(rigidBody);

			calculatePositionForEntity(mode, entry, posProp.data());

			entry.motionState = std::make_unique<PhysicalMotionState>(entry,
				*rigidBody,
				*this,
				btTransform(orientation, Convert::toBullet(posProp.data())),
				btTransform(btQuaternion::getIdentity(), entry.centerOfMassOffset));
			rigidBody->setMotionState(entry.motionState.get());
			rigidBody->setAngularFactor(angularFactor);
			entry.collisionObject->setUserPointer(&entry);

			//To prevent tunneling we'll turn on CCD with suitable values.
			auto minSize = std::min({size.x(), size.y(), size.z()});
			//          float maxSize = std::max(size.x(), std::max(size.y(), size.z()));
			entry.collisionObject->setCcdMotionThreshold((float)minSize * CCD_MOTION_FACTOR);
			entry.collisionObject->setCcdSweptSphereRadius((float)minSize * CCD_SPHERE_FACTOR);

			//Set up cached speed values
			auto speedGroundProp = entity.getPropertyType<double>("speed_ground");
			entry.speedGround = speedGroundProp ? speedGroundProp->data() : 0;

			auto speedWaterProp = entity.getPropertyType<double>("speed_water");
			entry.speedWater = speedWaterProp ? speedWaterProp->data() : 0;

			auto speedFlightProp = entity.getPropertyType<double>("speed_flight");
			entry.speedFlight = speedFlightProp ? speedFlightProp->data() : 0;

			//Only add to world if position is valid. Otherwise this will be done when a new valid position is applied in applyNewPositionForEntity
			if (posProp.data().isValid()) {
				m_dynamicsWorld->addRigidBody(rigidBody, collisionGroup, collisionMask);
			}

			//Call to "activate" will be ignored for bodies marked with CF_STATIC_OBJECT
			entry.collisionObject->activate();

			auto propelProp = entity.getPropertyClassFixed<PropelProperty>();
			if (propelProp && propelProp->data().isValid() && propelProp->data() != WFMath::Vector<3>::ZERO()) {
				applyPropel(entry, Convert::toBullet(propelProp->data()));
			}
			auto destinationProp = entity.getPropertyClass<Vector3Property<LocatedEntity>>("_destination");
			if (destinationProp && destinationProp->data().isValid()) {
				childEntityPropertyApplied("_destination", *destinationProp, entry);
			}


			//applyPropel(entry, propelProp, destinationProp);

			auto stepFactorProp = entity.getPropertyType<double>("step_factor");
			entry.step_factor = stepFactorProp ? stepFactorProp->data() : 0;
			if (entry.step_factor > 0) {
				m_steppingEntries.emplace(&entry);
			}

			//Should we only do this for "free" and "projectile"?
			if (velocityProp.data().isValid() && velocityProp.data() != WFMath::Vector<3>::ZERO()) {
				rigidBody->applyCentralImpulse(Convert::toBullet(velocityProp.data()));
			}
		}
	}

	entry.propertyUpdatedConnection = entity.propertyApplied.connect([&](const std::string& name, const PropertyBase& property) { childEntityPropertyApplied(name, property, entry); });

	updateTerrainMod(entry, true);

	{
		auto visSphere = std::make_unique<btSphereShape>(0);
		visSphere->setUnscaledRadius(calculateVisibilitySphereRadius(entry));

		auto visObject = std::make_unique<btCollisionObject>();
		visObject->setCollisionShape(visSphere.get());
		visObject->setUserPointer(&entry);
		auto visibilityObjectPtr = visObject.get();
		entry.visibilitySphere = std::move(visObject);
		entry.visibilityShape = std::move(visSphere);
		if (posProp.data().isValid()) {
			visibilityObjectPtr->setWorldTransform(btTransform(btQuaternion::getIdentity(), Convert::toBullet(posProp.data()) * VISIBILITY_SCALING_FACTOR));
			m_visibilityWorld->addCollisionObject(visibilityObjectPtr,
				VISIBILITY_MASK_OBSERVER,
				entity.hasFlags(entity_visibility_protected) || entity.hasFlags(entity_visibility_private) ? VISIBILITY_MASK_OBSERVABLE_PRIVATE : VISIBILITY_MASK_OBSERVABLE);
		}
	}
	if (entity.isPerceptive()) {
		auto viewShape = std::make_unique<btSphereShape>(VIEW_SPHERE_RADIUS * VISIBILITY_SCALING_FACTOR);
		auto viewSphere = std::make_unique<btCollisionObject>();
		viewSphere->setCollisionShape(viewShape.get());
		viewSphere->setUserPointer(&entry);
		auto viewSpherePtr = viewSphere.get();
		entry.viewSphere = std::move(viewSphere);
		entry.viewShape = std::move(viewShape);

		//Should always be able to observe the domain entity.
		entry.observedByThisChanges.emplace_back(&mContainingEntityEntry, BulletEntry::VisibilityQueueOperationType::Add);

		if (posProp.data().isValid()) {
			viewSpherePtr->setWorldTransform(btTransform(btQuaternion::getIdentity(), Convert::toBullet(posProp.data()) * VISIBILITY_SCALING_FACTOR));
			m_visibilityWorld->addCollisionObject(viewSpherePtr,
				entity.hasFlags(entity_admin) ? VISIBILITY_MASK_OBSERVABLE | VISIBILITY_MASK_OBSERVABLE_PRIVATE : VISIBILITY_MASK_OBSERVABLE,
				VISIBILITY_MASK_OBSERVER);
		}
		mContainingEntityEntry.observingThis.insert(&entry);

		//We are observing ourselves, and we are being observed by ourselves.
		entry.observedByThis.insert(&entry);
		entry.observingThis.insert(&entry);
	}

	if (m_entity.isPerceptive()) {
		entry.observingThis.insert(&mContainingEntityEntry);
		mContainingEntityEntry.observedByThis.insert(&entry);
	}

	OpVector res;
	updateObserverEntry(entry, res);
	updateObservedEntry(entry, res, false); //Don't send any ops, since that will be handled by the calling code when changing locations.
	for (auto& op: res) {
		m_entity.sendWorld(op);
	}
}

void PhysicalDomain::toggleChildPerception(LocatedEntity& entity) {
	auto I = m_entries.find(entity.getIdAsInt());
	if (I == m_entries.end()) {
		//This could happen if the entity didn't have any position, for example.
		return;
	}
	auto& entry = I->second;
	if (entity.isPerceptive()) {
		if (!entry->viewSphere) {
			mContainingEntityEntry.observingThis.insert(entry.get());
			auto viewShape = std::make_unique<btSphereShape>(VIEW_SPHERE_RADIUS * VISIBILITY_SCALING_FACTOR);
			auto viewSphere = std::make_unique<btCollisionObject>();
			viewSphere->setCollisionShape(viewShape.get());
			viewSphere->setUserPointer(entry.get());
			if (entry->positionProperty.data().isValid()) {
				viewSphere->setWorldTransform(btTransform(btQuaternion::getIdentity(), Convert::toBullet(entry->positionProperty.data()) * VISIBILITY_SCALING_FACTOR));
				m_visibilityWorld->addCollisionObject(viewSphere.get(),
					entity.hasFlags(entity_admin) ? VISIBILITY_MASK_OBSERVABLE | VISIBILITY_MASK_OBSERVABLE_PRIVATE : VISIBILITY_MASK_OBSERVABLE,
					VISIBILITY_MASK_OBSERVER);
			}
			entry->viewSphere = std::move(viewSphere);

			//We are observing ourselves, and we are being observed by ourselves.
			entry->observedByThis.insert(entry.get());
			entry->observingThis.insert(entry.get());

			OpVector res;
			updateObserverEntry(*entry, res);
			for (auto& op: res) {
				m_entity.sendWorld(op);
			}
		}
	} else {
		if (entry->viewSphere) {
			m_visibilityWorld->removeCollisionObject(entry->viewSphere.get());
			entry->viewShape.reset();
			entry->viewSphere.reset();
			mContainingEntityEntry.observingThis.erase(entry.get());
		}
	}
}

void PhysicalDomain::removeEntity(LocatedEntity& entity) {
	cy_debug_print("PhysicalDomain::removeEntity " << entity.describeEntity())
	auto I = m_entries.find(entity.getIdAsInt());
	if (I == m_entries.end()) {
		//This could happen if the entity didn't have any position, for example.
		return;
	}
	auto& entry = I->second;

	auto modI = m_terrainMods.find(entity.getIdAsInt());
	if (modI != m_terrainMods.end()) {
		m_terrain->updateMod(entity.getIdAsInt(), nullptr);
		m_terrainMods.erase(modI);
	}

	if (entry->collisionObject) {
		m_dynamicsWorld->removeCollisionObject(entry->collisionObject.get());
	}

	if (entry->addedToMovingList) {
		removeAndShift(m_movingEntities, entry.get());
	}

	entry->propertyUpdatedConnection.disconnect();
	if (entry->viewSphere) {
		m_visibilityWorld->removeCollisionObject(entry->viewSphere.get());
	}
	if (entry->visibilitySphere) {
		m_visibilityWorld->removeCollisionObject(entry->visibilitySphere.get());
	}
	for (BulletEntry* observer: entry->observingThis) {
		observer->observedByThis.erase(entry.get());
	}
	for (BulletEntry* observedEntry: entry->observedByThis) {
		observedEntry->observingThis.erase(entry.get());
	}

	if (entry->markedForVisibilityRecalculation) {
		removeAndShift(m_visibilityRecalculateQueue, entry.get());
	}

	m_propelUpdateQueue.erase(entry.get());
	m_directionUpdateQueue.erase(entry.get());

	mContainingEntityEntry.observingThis.erase(entry.get());

	//The entity owning the domain should normally not be perceptive, so we'll check first to optimize a bit.
	if (m_entity.isPerceptive()) {
		mContainingEntityEntry.observedByThis.insert(entry.get());
	}

	auto modeDataProp = entity.getPropertyClassFixed<ModeDataProperty>();
	if (modeDataProp && modeDataProp->getMode() == ModeProperty::Mode::Planted) {
		auto& plantedOnData = modeDataProp->getPlantedOnData();
		if (plantedOnData.entityId) {
			auto plantedOnI = m_entries.find(*plantedOnData.entityId);
			if (plantedOnI != m_entries.end()) {
				plantedOnI->second->attachedEntities.erase(entry.get());
			}
		}
	}

	auto closenessObservationsCopy = entry->closenessObservations;
	for (auto& observation: closenessObservationsCopy) {
		if (observation->callback) {
			observation->callback();
		}
	}

	//We need to first extract the list of attached entities before we erase the instance.
	//Otherwise the entity will still be there when we apply new positions for the attached entities.
	auto attachedEntities = std::move(entry->attachedEntities);

	//Reset all physical properties when something is moved out of the physical domain.
	entry->positionProperty.data() = {};
	entry->entity.applyProperty(entry->positionProperty);
	entry->orientationProperty.data() = {};
	entry->entity.applyProperty(entry->orientationProperty);
	entry->velocityProperty.data() = {};
	entry->entity.applyProperty(entry->velocityProperty);
	entry->angularVelocityProperty.data() = {};
	entry->entity.applyProperty(entry->angularVelocityProperty);

	m_steppingEntries.erase(entry.get());

	m_entries.erase(I);

	m_propellingEntries.erase(entity.getIdAsInt());

	std::set<LocatedEntity*> transformedEntities;
	for (auto* attachedEntry: attachedEntities) {
		applyTransformInternal(*attachedEntry,
			{},
			attachedEntry->positionProperty.data(),
			{},
			transformedEntities,
			true);
	}

}

void PhysicalDomain::childEntityPropertyApplied(const std::string& name, const PropertyBase& prop, BulletEntry& bulletEntry) {
	if (name == "pos") {
		//        auto& pos = bulletEntry.positionProperty.data();
		//        if (pos.isValid()) {
		//            applyNewPositionForEntity(bulletEntry, pos);
		//            if (!oldPos.isEqualTo(entity.m_location.m_pos)) {
		//                entity.removeFlags(entity_pos_clean);
		//                hadChange = true;
		//                //Check if there previously wasn't any valid pos, and thus no valid collision instances.
		//                if (entity.m_location.m_pos.isValid() && !oldPos.isValid()) {
		//                    if (entry->collisionObject) {
		//                        short collisionMask;
		//                        short collisionGroup;
		//                        getCollisionFlagsForEntity(entity, collisionGroup, collisionMask);
		//                        if (rigidBody) {
		//                            m_dynamicsWorld->addRigidBody(rigidBody, collisionGroup, collisionMask);
		//                        } else {
		//                            m_dynamicsWorld->addCollisionObject(entry->collisionObject.get(), collisionGroup, collisionMask);
		//                        }
		//                    }
		//                    if (entry->viewSphere) {
		//                        m_visibilityWorld->addCollisionObject(entry->viewSphere.get(),
		//                                                              entity.hasFlags(entity_admin) ? VISIBILITY_MASK_OBSERVABLE | VISIBILITY_MASK_OBSERVABLE_PRIVATE : VISIBILITY_MASK_OBSERVABLE,
		//                                                              VISIBILITY_MASK_OBSERVER);
		//                    }
		//                    if (entry->visibilitySphere) {
		//                        m_visibilityWorld->addCollisionObject(entry->visibilitySphere.get(), VISIBILITY_MASK_OBSERVER,
		//                                                              entity.hasFlags(entity_visibility_protected) || entity.hasFlags(entity_visibility_private) ? VISIBILITY_MASK_OBSERVABLE_PRIVATE
		//                                                                                                                                                         : VISIBILITY_MASK_OBSERVABLE);
		//                    }
		//                }
		//            }
		//        }
	} else if (name == PropelProperty::property_name) {
		bulletEntry.control.propelProperty = dynamic_cast<const PropelProperty*>(&prop);
		m_propelUpdateQueue.insert(&bulletEntry);
	} else if (name == "_direction") {
		bulletEntry.control.directionProperty = dynamic_cast<const QuaternionProperty<LocatedEntity>*>(&prop);
		m_directionUpdateQueue.insert(&bulletEntry);
	} else if (name == "_destination") {
		auto destinationProperty = dynamic_cast<const Vector3Property<LocatedEntity>*>(&prop);
		if (destinationProperty->data().isValid()) {

			bulletEntry.control.destinationProperty = dynamic_cast<const Vector3Property<LocatedEntity>*>(&prop);
		}

		//Use the propel update queue for destination too, as they both concern propelling.
		m_propelUpdateQueue.insert(&bulletEntry);
	} else if (name == "friction") {
		if (bulletEntry.collisionObject) {
			auto frictionProp = dynamic_cast<const Property<double, LocatedEntity>*>(&prop);
			bulletEntry.collisionObject->setFriction(static_cast<btScalar>(frictionProp->data()));
			if (getMassForEntity(bulletEntry.entity) != 0) {
				bulletEntry.collisionObject->activate();
			}
		}
	} else if (name == "friction_roll") {
		if (bulletEntry.collisionObject) {
			auto frictionProp = dynamic_cast<const Property<double, LocatedEntity>*>(&prop);
			bulletEntry.collisionObject->setRollingFriction(static_cast<btScalar>(frictionProp->data()));
			if (getMassForEntity(bulletEntry.entity) != 0) {
				bulletEntry.collisionObject->activate();
			}
		}
	} else if (name == "friction_spin") {
		if (bulletEntry.collisionObject) {
			auto frictionProp = dynamic_cast<const Property<double, LocatedEntity>*>(&prop);
			bulletEntry.collisionObject->setSpinningFriction(static_cast<btScalar>(frictionProp->data()));
			if (getMassForEntity(bulletEntry.entity) != 0) {
				bulletEntry.collisionObject->activate();
			}
		}
	} else if (name == "mode") {
		if (bulletEntry.collisionObject) {
			auto modeProp = dynamic_cast<const ModeProperty*>(&prop);
			//Check if the mode change came from "outside", i.e. wasn't made because of the physics simulation (such as being submerged).
			if (modeProp->getMode() != bulletEntry.mode) {
				applyNewPositionForEntity(bulletEntry, bulletEntry.positionProperty.data());

				if (auto rigidBody = btRigidBody::upcast(bulletEntry.collisionObject.get())) {
					if (modeProp->getMode() == ModeProperty::Mode::Projectile) {
						rigidBody->setCollisionFlags(rigidBody->getCollisionFlags() | btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
					} else {
						rigidBody->setCollisionFlags(rigidBody->getCollisionFlags() & ~btCollisionObject::CF_CUSTOM_MATERIAL_CALLBACK);
					}

					//If there's a rigid body, there's a valid bbox, otherwise something else is broken
					auto& bbox = bulletEntry.bbox;

					//When altering mass we need to first remove and then re-add the body.
					m_dynamicsWorld->removeCollisionObject(bulletEntry.collisionObject.get());

					float mass = getMassForEntity(bulletEntry.entity);
					//"fixed" mode means that the entity stays in place, always
					//"planted" mode means it's planted in the ground
					//Zero mass makes the rigid body static
					if (modeProp->getMode() == ModeProperty::Mode::Planted || modeProp->getMode() == ModeProperty::Mode::Fixed || mass == 0) {

						if ((rigidBody->getCollisionFlags() & btCollisionObject::CF_STATIC_OBJECT) == 0
							&& rigidBody->getCollisionShape()->getShapeType() == CONVEX_HULL_SHAPE_PROXYTYPE) {
							//If the shape is a mesh, and it previously wasn't static, we need to replace the shape with an optimized one.
							bulletEntry.collisionShape = createCollisionShapeForEntry(bulletEntry.entity, bbox, mass, bulletEntry.centerOfMassOffset);
							rigidBody->setCollisionShape(bulletEntry.collisionShape.get());
						}

						rigidBody->setMassProps(0, btVector3(0, 0, 0));

					} else {
						//Detach from any attached entity

						plantOnEntity(bulletEntry, nullptr);

						//The entity is free
						if (rigidBody->getCollisionFlags() & btCollisionObject::CF_STATIC_OBJECT
							&& rigidBody->getCollisionShape()->getShapeType() == SCALED_TRIANGLE_MESH_SHAPE_PROXYTYPE) {
							//If the shape is a mesh, and it previously was static, we need to replace the shape with an optimized one.
							bulletEntry.collisionShape = createCollisionShapeForEntry(bulletEntry.entity, bbox, mass, bulletEntry.centerOfMassOffset);
							rigidBody->setCollisionShape(bulletEntry.collisionShape.get());
						}

						btVector3 inertia(0, 0, 0);
						bulletEntry.collisionShape->calculateLocalInertia(mass, inertia);

						rigidBody->setMassProps(mass, inertia);

						//If there are attached entities, we must also make them free.
						auto attachedEntitiesCopy = bulletEntry.attachedEntities;
						for (auto attachedEntry: attachedEntitiesCopy) {
							attachedEntry->entity.setAttrValue("mode", modeProp->data());
						}
						if (!bulletEntry.attachedEntities.empty()) {
							spdlog::warn("Set of attached entities isn't empty after changing all of them to free mode.");
						}
					}
					//It's crucial we call this when changing mass, otherwise we might get divide-by-zero in the simulation
					rigidBody->updateInertiaTensor();
					short collisionMask;
					short collisionGroup;
					getCollisionFlagsForEntity(bulletEntry, collisionGroup, collisionMask);

					m_dynamicsWorld->addRigidBody(rigidBody, collisionGroup, collisionMask);

					bulletEntry.collisionObject->activate();
				}
				//Since we've deactivated automatic updating of all aabbs each tick we need to do it ourselves when updating the position.
				m_dynamicsWorld->updateSingleAabb(bulletEntry.collisionObject.get());

				bulletEntry.mode = modeProp->getMode();
			}


			//sendMoveSight(*bulletEntry);
		}
		if (!bulletEntry.addedToMovingList) {
			m_movingEntities.emplace_back(&bulletEntry);
			bulletEntry.addedToMovingList = true;
			bulletEntry.markedAsMovingThisFrame = false;
			bulletEntry.markedAsMovingLastFrame = false;
		}
		return;
	} else if (name == SolidProperty<LocatedEntity>::property_name) {
		auto solidProp = dynamic_cast<const SolidProperty<LocatedEntity>*>(&prop);

		bulletEntry.isSolid = solidProp->isTrue();
		if (bulletEntry.collisionObject) {
			if (auto rigidBody = btRigidBody::upcast(bulletEntry.collisionObject.get())) {
				short collisionMask;
				short collisionGroup;
				getCollisionFlagsForEntity(bulletEntry, collisionGroup, collisionMask);
				m_dynamicsWorld->removeRigidBody(rigidBody);
				m_dynamicsWorld->addRigidBody(rigidBody, collisionGroup, collisionMask);

				bulletEntry.collisionObject->activate();
			}
		}
	} else if (name == "mass") {
		const auto* modeProp = bulletEntry.entity.getPropertyClassFixed<ModeProperty>();

		if (modeProp && (modeProp->getMode() == ModeProperty::Mode::Planted || modeProp->getMode() == ModeProperty::Mode::Fixed)) {
			//"fixed" mode means that the entity stays in place, always
			//"planted" mode means it's planted in the ground
			//Zero mass makes the rigid body static
		} else {
			if (bulletEntry.collisionObject) {
				if (auto rigidBody = btRigidBody::upcast(bulletEntry.collisionObject.get())) {
					//When altering mass we need to first remove and then re-add the body.
					m_dynamicsWorld->removeRigidBody(rigidBody);

					short collisionMask;
					short collisionGroup;
					getCollisionFlagsForEntity(bulletEntry, collisionGroup, collisionMask);

					float mass = getMassForEntity(bulletEntry.entity);
					btVector3 inertia;
					bulletEntry.collisionShape->calculateLocalInertia(mass, inertia);

					rigidBody->setMassProps(mass, inertia);
					//It's crucial we call this when changing mass, otherwise we might get divide-by-zero in the simulation
					rigidBody->updateInertiaTensor();

					m_dynamicsWorld->addRigidBody(rigidBody, collisionGroup, collisionMask);
				}
			}
		}

	} else if (name == BBoxProperty<LocatedEntity>::property_name || name == ScaleProperty<LocatedEntity>::property_name) {
		auto& bbox = bulletEntry.bbox;
		bbox = ScaleProperty<LocatedEntity>::scaledBbox(bulletEntry.entity);
		if (bbox.isValid()) {

			if (bulletEntry.visibilitySphere) {
				auto radius = calculateVisibilitySphereRadius(bulletEntry);

				if (radius != bulletEntry.visibilityShape->getImplicitShapeDimensions().x()) {
					bulletEntry.visibilityShape->setUnscaledRadius(radius);
					if (!bulletEntry.markedForVisibilityRecalculation) {
						m_visibilityRecalculateQueue.emplace_back(&bulletEntry);
						bulletEntry.markedForVisibilityRecalculation = true;
					}
				}
			}

			if (bulletEntry.collisionObject) {
				//When changing shape dimensions we must first remove the object, do the change, and then add it back again.

				//Note that we can't just call setLocalScaling since it doesn't seem to work well with mesh shapes.
				auto rigidBody = btRigidBody::upcast(bulletEntry.collisionObject.get());
				if (rigidBody) {
					m_dynamicsWorld->removeRigidBody(rigidBody);
				} else {
					m_dynamicsWorld->removeCollisionObject(bulletEntry.collisionObject.get());
				}
				float mass = getMassForEntity(bulletEntry.entity);

				bulletEntry.collisionShape = createCollisionShapeForEntry(bulletEntry.entity, bbox, mass, bulletEntry.centerOfMassOffset);
				bulletEntry.collisionObject->setCollisionShape(bulletEntry.collisionShape.get());
				if (bulletEntry.motionState) {
					bulletEntry.motionState->m_centerOfMassOffset = btTransform(btQuaternion::getIdentity(), bulletEntry.centerOfMassOffset);
				}
				bulletEntry.collisionObject->activate();

				short collisionMask;
				short collisionGroup;
				getCollisionFlagsForEntity(bulletEntry, collisionGroup, collisionMask);

				if (rigidBody) {
					m_dynamicsWorld->addRigidBody(rigidBody, collisionGroup, collisionMask);
				} else {
					m_dynamicsWorld->addCollisionObject(bulletEntry.collisionObject.get(), collisionGroup, collisionMask);
				}

				applyNewPositionForEntity(bulletEntry, bulletEntry.positionProperty.data());

				m_dynamicsWorld->updateSingleAabb(bulletEntry.collisionObject.get());
			}


		}
	} else if (name == "planted_offset" || name == "planted_scaled_offset") {
		applyNewPositionForEntity(bulletEntry, bulletEntry.positionProperty.data());
		//bulletEntry.entity.m_location.update(BaseWorld::instance().getTimeAsSeconds());
		bulletEntry.entity.removeFlags(entity_clean);
		if (bulletEntry.collisionObject) {
			m_dynamicsWorld->updateSingleAabb(bulletEntry.collisionObject.get());
		}
		sendMoveSight(bulletEntry, true, false, false, false, false);
	} else if (name == TerrainModProperty::property_name) {
		updateTerrainMod(bulletEntry, true);
	} else if (name == AreaProperty::property_name) {
		updateTerrainArea(bulletEntry, true);
	} else if (name == "speed_ground") {
		bulletEntry.speedGround = dynamic_cast<const Property<double, LocatedEntity>*>(&prop)->data();
	} else if (name == "speed_water") {
		bulletEntry.speedWater = dynamic_cast<const Property<double, LocatedEntity>*>(&prop)->data();
	} else if (name == "speed_flight") {
		bulletEntry.speedFlight = dynamic_cast<const Property<double, LocatedEntity>*>(&prop)->data();
	} else if (name == "floats") {
		//TODO: find a better way to handle entities which are set to float. Perhaps it should just be handled by density?
		//        applyNewPositionForEntity(bulletEntry, bulletEntry.positionProperty.data());
		//        bulletEntry.entity.m_location.update(BaseWorld::instance().getTimeAsSeconds());
		//        bulletEntry.entity.removeFlags(entity_clean);
		//        if (bulletEntry.collisionObject) {
		//            m_dynamicsWorld->updateSingleAabb(bulletEntry.collisionObject.get());
		//        }
		//        sendMoveSight(*bulletEntry, true, false, false, false, false);
	} else if (name == "step_factor") {
		auto stepFactorProp = dynamic_cast<const Property<double, LocatedEntity>*>(&prop);
		if (stepFactorProp) {
			bulletEntry.step_factor = stepFactorProp->data();
		} else {
			bulletEntry.step_factor = 0;
		}
		auto I = m_steppingEntries.find(&bulletEntry);
		if (stepFactorProp && stepFactorProp->data() > 0) {
			if (I == m_steppingEntries.end()) {
				m_steppingEntries.emplace(&bulletEntry);
			}
		} else {
			if (I != m_steppingEntries.end()) {
				m_steppingEntries.erase(I);
			}
		}
	} else if (name == PerceptionSightProperty::property_name) {
		toggleChildPerception(bulletEntry.entity);
	} else if (name == ModeDataProperty::property_name) {
		applyNewPositionForEntity(bulletEntry, bulletEntry.positionProperty.data(), true);
		sendMoveSight(bulletEntry, true, false, false, false, false);
	}
}

void PhysicalDomain::updateTerrainMod(const BulletEntry& entry, bool forceUpdate) {
	//TODO store as flag in BulletEntry so we don't need to do lookup
	if (auto modeProp = entry.entity.getPropertyClassFixed<ModeProperty>()) {
		if (modeProp->getMode() == ModeProperty::Mode::Planted) {
			auto terrainModProperty = entry.entity.getPropertyClassFixed<TerrainModProperty>();
			if (terrainModProperty && m_terrain) {
				auto& pos = entry.positionProperty.data();
				//We need to get the vertical position in the terrain, without any mods.
				Mercator::Segment* segment = m_terrain->getSegmentAtPos((float)pos.x(), (float)pos.z());
				auto modPos = pos;
				if (segment) {
					std::vector<WFMath::AxisBox<2>> terrainAreas;

					float height;
					//If there's no mods we can just use position right away
					if (segment->getMods().empty()) {
						if (!segment->isValid()) {
							segment->populate();
						}
						segment->getHeight(modPos.x() - (segment->getXRef()), modPos.z() - (segment->getZRef()), height);
					} else {
						Mercator::HeightMap heightMap(segment->getResolution());
						heightMap.allocate();
						segment->populateHeightMap(heightMap);

						heightMap.getHeight(modPos.x() - (segment->getXRef()), modPos.z() - (segment->getZRef()), height);
					}

					modPos.y() = height;

					auto I = m_terrainMods.find(entry.entity.getIdAsInt());
					if (I != m_terrainMods.end()) {
						auto& oldPos = I->second.modPos;
						auto& oldOrient = I->second.modOrientation;

						if (!oldOrient.isEqualTo(entry.orientationProperty.data()) || !oldPos.isEqualTo(modPos)) {
							//Need to update terrain mod
							forceUpdate = true;
							const WFMath::AxisBox<2>& oldArea = I->second.area;
							if (oldArea.isValid()) {
								terrainAreas.push_back(oldArea);
							}
						}
					} else {
						forceUpdate = true;
					}

					if (forceUpdate) {
						auto modifier = terrainModProperty->parseModData(modPos, entry.orientationProperty.data());

						if (modifier) {
							auto bbox = modifier->bbox();
							terrainAreas.push_back(bbox);
							m_terrainMods[entry.entity.getIdAsInt()] = TerrainModEntry{
								.modPos = modPos,
								.modOrientation = entry.orientationProperty.data(),
								.area = bbox
							};
						} else {
							m_terrainMods.erase(entry.entity.getIdAsInt());
						}
						auto oldAreas = m_terrain->updateMod(entry.entity.getIdAsInt(), std::move(modifier));
						if (oldAreas.isValid()) {
							terrainAreas.emplace_back(oldAreas);
						}
						refreshTerrain(terrainAreas);
					}
				}
			}
		} else {
			//Make sure the terrain mod is removed if the entity isn't planted
			auto I = m_terrainMods.find(entry.entity.getIdAsInt());
			if (I != m_terrainMods.end()) {
				std::vector<WFMath::AxisBox<2>> terrainAreas;
				terrainAreas.emplace_back(I->second.area);
				m_terrain->updateMod(entry.entity.getIdAsInt(), nullptr);
				m_terrainMods.erase(I);
				refreshTerrain(terrainAreas);
			}
		}
	}
}

void PhysicalDomain::updateTerrainArea(const BulletEntry& entry, bool forceUpdate) {
	//TODO store as flag in BulletEntry so we don't need to do lookup
	if (auto modeProp = entry.entity.getPropertyClassFixed<ModeProperty>()) {
		if (modeProp->getMode() == ModeProperty::Mode::Planted) {
			if (auto terrainAreaProperty = entry.entity.getPropertyClassFixed<const AreaProperty>(); terrainAreaProperty && m_terrain) {
				auto& pos = entry.positionProperty.data();
				auto& orientation = entry.orientationProperty.data();
				if (pos.isValid()) {
					auto& state = terrainAreaProperty->getData();


					auto shape = state.shape->shape();

					if (state.scaled) {
						auto scaleProperty = entry.entity.getPropertyClassFixed<const ScaleProperty<LocatedEntity>>();
						if (scaleProperty && scaleProperty->data().isValid()) {
							MathShape<WFMath::Polygon, 2>::scaleInPlace(shape, {scaleProperty->data().x(), scaleProperty->data().z()});
						}
					}

					if (orientation.isValid()) {
						/// rotation about Y axis
						WFMath::Vector<3> xVec = WFMath::Vector<3>(1.0, 0.0, 0.0).rotate(orientation);
						WFMath::CoordType theta = std::atan2(xVec.z(), xVec.x());
						WFMath::RotMatrix<2> rm;
						shape.rotatePoint(rm.rotation(theta), WFMath::Point<2>(0, 0));
					}
					shape.shift({pos.x(), pos.z()});
					auto area = std::make_unique<Mercator::Area>(state.layer, false);
					area->setShape(state.shape->shape());
					auto newAffectedArea = area->bbox();
					if (newAffectedArea.isValid()) {
						m_dirtyTerrainSurfaceAreas.emplace_back(newAffectedArea);
					}
					auto oldAffectedArea = m_terrain->updateArea(entry.entity.getIdAsInt(), std::move(area));
					if (oldAffectedArea.isValid() && oldAffectedArea != newAffectedArea) {
						m_dirtyTerrainSurfaceAreas.emplace_back(oldAffectedArea);
					}
				}
			}
		} else {
			//Make sure the terrain mod is removed if the entity isn't planted
			auto I = m_terrainMods.find(entry.entity.getIdAsInt());
			if (I != m_terrainMods.end()) {
				std::vector<WFMath::AxisBox<2>> terrainAreas;
				terrainAreas.emplace_back(I->second.area);
				m_terrain->updateMod(entry.entity.getIdAsInt(), nullptr);
				m_terrainMods.erase(I);
				refreshTerrain(terrainAreas);
			}
		}
	}
}

void PhysicalDomain::getCollisionFlagsForEntity(const BulletEntry& entry, short& collisionGroup, short& collisionMask) {
	//The "group" defines the features of this object, which other bodies can mask out.
	//The "mask" defines the other kind of object this body will react with.

	//Water bodies behave in a special way, so check for that.
	auto& entity = entry.entity;
	if (auto waterBodyProp = entity.getPropertyClass<BoolProperty<LocatedEntity>>("water_body"); waterBodyProp && waterBodyProp->isTrue()) {
		//A body of water should behave like terrain, and interact with both physical and non-physical entities.
		collisionGroup = COLLISION_MASK_TERRAIN;
		collisionMask = COLLISION_MASK_NON_PHYSICAL | COLLISION_MASK_PHYSICAL;
	} else {
		auto modeProp = entity.getPropertyClassFixed<ModeProperty>();
		if (modeProp && modeProp->getMode() == ModeProperty::Mode::Fixed) {
			if (entry.isSolid) {
				collisionGroup = COLLISION_MASK_STATIC;
				//Fixed entities shouldn't collide with anything themselves.
				//Other physical entities should however collide with them.
				collisionMask = COLLISION_MASK_PHYSICAL;
			} else {
				//The object is both fixed and not solid. It shouldn't collide with anything at all.
				collisionGroup = 0;
				collisionMask = 0;
			}
		} else if (modeProp && modeProp->getMode() == ModeProperty::Mode::Planted) {
			if (entry.isSolid) {
				collisionGroup = COLLISION_MASK_STATIC;
				//Planted entities shouldn't collide with anything themselves except the terrain.
				//Other physical entities should however collide with them.
				collisionMask = COLLISION_MASK_PHYSICAL | COLLISION_MASK_TERRAIN;
			} else {
				//The object is both planted and not solid. It shouldn't collide with anything at all except terrain.
				collisionGroup = 0;
				collisionMask = COLLISION_MASK_TERRAIN;
			}
		} else {
			if (entry.isSolid) {
				//This is a physical object
				collisionGroup = COLLISION_MASK_PHYSICAL;
				//In this case other physical moving objects, the terrain and all static objects.
				collisionMask = COLLISION_MASK_PHYSICAL | COLLISION_MASK_TERRAIN | COLLISION_MASK_STATIC;
			} else {
				//Non solid objects should collide with the terrain only.
				//Mark the object as non-physical
				collisionGroup = COLLISION_MASK_NON_PHYSICAL;
				//And only collide with the terrain.
				collisionMask = COLLISION_MASK_TERRAIN;
			}
		}
	}
}

void PhysicalDomain::entityPropertyApplied(const std::string& name, const PropertyBase& prop) {
	if (name == "friction") {
		auto frictionProp = dynamic_cast<const Property<double, LocatedEntity>*>(&prop);
		for (auto& entry: m_terrainSegments) {
			entry.second.rigidBody->setFriction(static_cast<btScalar>(frictionProp->data()));
		}
	} else if (name == "friction_roll") {
		auto frictionRollingProp = dynamic_cast<const Property<double, LocatedEntity>*>(&prop);
		for (auto& entry: m_terrainSegments) {
			entry.second.rigidBody->setRollingFriction(static_cast<btScalar>(frictionRollingProp->data()));
		}
	} else if (name == "friction_spin") {

		auto frictionSpinningProp = dynamic_cast<const Property<double, LocatedEntity>*>(&prop);
		for (auto& entry: m_terrainSegments) {
			entry.second.rigidBody->setSpinningFriction(static_cast<btScalar>(frictionSpinningProp->data()));
		}
	} else if (name == TerrainProperty::property_name) {
		if (m_entity.getPropertyClassFixed<TerrainProperty>()) {
			m_terrain = &TerrainProperty::getData(m_entity);
		}
	}
}

void PhysicalDomain::calculatePositionForEntity(ModeProperty::Mode mode, PhysicalDomain::BulletEntry& entry, WFMath::Point<3>& pos) {
	struct PlantedOnCallback : public btCollisionWorld::ContactResultCallback {
		btVector3 highestPoint;
		bool hadHit = false;
		const btCollisionObject* highestObject = nullptr;

		explicit PlantedOnCallback(btVector3 highestPoint_in)
			: btCollisionWorld::ContactResultCallback(), highestPoint(highestPoint_in) {
		}

		btScalar addSingleResult(btManifoldPoint& cp,
								 const btCollisionObjectWrapper* colObj0,
								 int partId0,
								 int index0,
								 const btCollisionObjectWrapper* colObj1,
								 int partId1,
								 int index1) override {

			//B will be the existing planted object, A will be the object being planted.
			btVector3 point = cp.getPositionWorldOnB();

			if (point.y() > highestPoint.y()) {
				highestPoint = point;
				highestObject = colObj1->m_collisionObject;
				hadHit = true;
			}

			//Returned result is ignored.
			return 0;
		}
	};

	struct PlaceOnManyCallback : public btCollisionWorld::ConvexResultCallback {
		btVector3 highestPoint;
		const btCollisionObject* highestObject = nullptr;
		btCollisionObject* testedCollisionObject = nullptr;

		bool needsCollision(btBroadphaseProxy* proxy0) const override {
			//Check that we don't collide with ourselves.
			if (testedCollisionObject == static_cast<btCollisionObject*>(proxy0->m_clientObject)) {
				return false;
			}
			return ConvexResultCallback::needsCollision(proxy0);
		}

		btScalar addSingleResult(btCollisionWorld::LocalConvexResult& convexResult, bool normalInWorldSpace) override {
			if (convexResult.m_hitPointLocal.y() > highestPoint.y()) {
				highestPoint = convexResult.m_hitPointLocal;
				highestObject = convexResult.m_hitCollisionObject;
			}

			//Returned result is ignored.
			return 0;
		}
	};

	auto& entity = entry.entity;

	if (mode == ModeProperty::Mode::Planted || mode == ModeProperty::Mode::Free || mode == ModeProperty::Mode::Submerged) {
		auto collisionObject = entry.collisionObject.get();
		float h = pos.y();
		getTerrainHeight(pos.x(), pos.z(), h);

		if (mode == ModeProperty::Mode::Planted) {

			bool plantedOn = false;
			if (collisionObject) {

				if (!plantedOn) {

					auto modeDataProp = entity.getPropertyClassFixed<ModeDataProperty>();
					if (modeDataProp && modeDataProp->getMode() == ModeProperty::Mode::Planted) {
						if (modeDataProp->getPlantedOnData().entityId) {
							auto plantedOnId = *modeDataProp->getPlantedOnData().entityId;
							if (plantedOnId == entity.getIdAsInt()) {
								spdlog::warn("Entity {} was marked to be planted on itself.", entity.describeEntity());
							} else {
								//If it's desired that the entity should be planted on the ground then make it so.
								//Since the ground is everywhere.
								if (plantedOnId == m_entity.getIdAsInt()) {
									plantOnEntity(entry, &mContainingEntityEntry);

									pos.y() = h;
									plantedOn = true;
								} else {
									//Otherwise we need to check that it really can be planted on what it's planted on.
									auto I = m_entries.find(plantedOnId);
									if (I != m_entries.end()) {
										auto& plantedOnBulletEntry = I->second;

										//Check if it's a water body it's planted on first.
										if (plantedOnBulletEntry->collisionObject->getUserIndex() == USER_INDEX_WATER_BODY) {

											//We're planted on a water body, we should place ourselves on top of it.
											float newHeight = plantedOnBulletEntry->positionProperty.data().y();
											if (plantedOnBulletEntry->bbox.isValid()) {
												newHeight += plantedOnBulletEntry->bbox.highCorner().y();
											}

											if (newHeight > h) {
												//Make sure it's not under the terrain.
												pos.y() = newHeight;
												plantedOn = true;
												plantOnEntity(entry, plantedOnBulletEntry.get());
											}
										} else {

											//Check if it collides with the entity it's marked as being planted on by moving it downwards until
											//it collides with the entity or the ground.
											btVector3 centerOfMassOffset;
											auto placementShape = createCollisionShapeForEntry(entry.entity, entry.bbox, 1, centerOfMassOffset);
											collisionObject->setCollisionShape(placementShape.get());

											btVector3 aabbMin, aabbMax;
											collisionObject->getCollisionShape()->getAabb(collisionObject->getWorldTransform(), aabbMin, aabbMax);
											float height = aabbMax.y() - aabbMin.y();

											float yPos = pos.y();

											btQuaternion orientation = entry.orientationProperty.data().isValid() ? Convert::toBullet(entry.orientationProperty.data()) : btQuaternion::getIdentity();
											btTransform transform(orientation, Convert::toBullet(entry.positionProperty.data()));
											transform *= btTransform(btQuaternion::getIdentity(), entry.centerOfMassOffset).inverse();

											collisionObject->setWorldTransform(transform);

											while (yPos > h) {
												PlantedOnCallback callback(btVector3(pos.x(), h, pos.z()));
												callback.m_collisionFilterGroup = COLLISION_MASK_PHYSICAL;
												callback.m_collisionFilterMask = COLLISION_MASK_STATIC;

												//Test if the shape collides, otherwise move it downwards until it reaches the ground.
												collisionObject->getWorldTransform().getOrigin().setY(yPos);
												m_dynamicsWorld->contactPairTest(collisionObject, plantedOnBulletEntry->collisionObject.get(), callback);

												if (callback.hadHit) {
													pos.y() = std::max(callback.highestPoint.y(), (btScalar)h);
													plantedOn = true;
													plantOnEntity(entry, plantedOnBulletEntry.get());
													break;
												}

												yPos -= height;
											}

											collisionObject->setCollisionShape(entry.collisionShape.get());
										}
									}
								}
							}
						}
					}

					if (!plantedOn) {

						btQuaternion orientation = entry.orientationProperty.data().isValid() ? Convert::toBullet(entry.orientationProperty.data()) : btQuaternion::getIdentity();
						btTransform transformFrom(orientation, Convert::toBullet(pos));
						btTransform transformTo(transformFrom);
						transformTo.getOrigin().setY(h);
						transformFrom *= btTransform(btQuaternion::getIdentity(), entry.centerOfMassOffset).inverse();
						transformTo *= btTransform(btQuaternion::getIdentity(), entry.centerOfMassOffset).inverse();

						if (!((transformFrom.getOrigin() - transformTo.getOrigin()).fuzzyZero())) {

							btVector3 centerOfMassOffset;
							auto placementShape = createCollisionShapeForEntry(entry.entity, entry.bbox, 1, centerOfMassOffset);
							if (auto convexShape = dynamic_cast<btConvexShape*>(placementShape.get())) {

								PlaceOnManyCallback callback{};
								callback.m_collisionFilterGroup = COLLISION_MASK_PHYSICAL;
								callback.m_collisionFilterMask = COLLISION_MASK_STATIC;
								callback.highestPoint = btVector3(pos.x(), h, pos.z());
								callback.testedCollisionObject = collisionObject;

								m_dynamicsWorld->convexSweepTest(convexShape, transformFrom, transformTo, callback);

								if (callback.highestObject) {
									if (auto plantedOnEntry = static_cast<BulletEntry*>(callback.highestObject->getUserPointer())) {
										assert(plantedOnEntry->entity.getIdAsString() != entity.getIdAsString());
										pos.y() = std::max(callback.highestPoint.y(), (btScalar)h);
										plantedOn = true;

										plantOnEntity(entry, plantedOnEntry);
									}
								}
							} else {
								spdlog::warn("Did not get a convex shape when creating placement shape, for entity {}.", entity.describeEntity());
							}
						}
					}
				}
			}


			//If we couldn't find anything to plant on, make sure it's planted on the ground.
			if (!plantedOn) {
				plantOnEntity(entry, &mContainingEntityEntry);

				pos.y() = h;
			}

			auto plantedOffsetProp = entity.getPropertyType<double>("planted_offset");
			if (plantedOffsetProp) {
				pos.y() += plantedOffsetProp->data();
			}
			auto plantedScaledOffsetProp = entity.getPropertyType<double>("planted_scaled_offset");
			if (plantedScaledOffsetProp && entry.bbox.isValid()) {
				auto size = entry.bbox.highCorner() - entry.bbox.lowCorner();

				pos.y() += (plantedScaledOffsetProp->data() * size.y());
			}
		} else if (mode == ModeProperty::Mode::Free || mode == ModeProperty::Mode::Submerged) {
			//For free entities we only want to clamp to terrain if the entity is below it
			pos.y() = std::max(pos.y(), static_cast<double>(h));
		}
	} else if (mode == ModeProperty::Mode::Free) {
		float h = pos.y();
		getTerrainHeight(pos.x(), pos.z(), h);
		pos.y() = h;
	} else if (mode == ModeProperty::Mode::Fixed || mode == ModeProperty::Mode::Projectile) {
		//Don't do anything to adjust height
	} else {
		spdlog::warn("Unknown mode for entity " + entity.describeEntity());
	}
}

void PhysicalDomain::applyNewPositionForEntity(BulletEntry& entry, const WFMath::Point<3>& pos, bool calculatePosition) {
	if (!pos.isValid()) {
		return;
	}
	btCollisionObject* collObject = entry.collisionObject.get();
	LocatedEntity& entity = entry.entity;

	ModeProperty::Mode mode = ModeProperty::Mode::Free;
	if (auto modeProp = entity.getPropertyClassFixed<ModeProperty>()) {
		mode = modeProp->getMode();
	}

	WFMath::Point<3> newPos = pos;

	if (calculatePosition) {
		calculatePositionForEntity(mode, entry, newPos);
	}

	entry.positionProperty.data() = newPos;
	entry.entity.applyProperty(entry.positionProperty);
	//    entity.m_location.m_pos = newPos;

	if (collObject) {
		btTransform& transform = collObject->getWorldTransform();

		cy_debug_print("PhysicalDomain::new pos " << entity.describeEntity() << " " << pos)

		transform.setOrigin(Convert::toBullet(newPos));
		transform *= btTransform(btQuaternion::getIdentity(), entry.centerOfMassOffset).inverse();

		collObject->setWorldTransform(transform);
	}

	if (entry.viewSphere) {
		entry.viewSphere->setWorldTransform(btTransform(btQuaternion::getIdentity(), Convert::toBullet(entry.positionProperty.data()) * VISIBILITY_SCALING_FACTOR));
		m_visibilityWorld->updateSingleAabb(entry.viewSphere.get());
	}
	if (entry.visibilitySphere) {
		entry.visibilitySphere->setWorldTransform(btTransform(btQuaternion::getIdentity(), Convert::toBullet(entry.positionProperty.data()) * VISIBILITY_SCALING_FACTOR));
		m_visibilityWorld->updateSingleAabb(entry.visibilitySphere.get());
	}

	//If the entity is an admin make a special case and do an observer check immediately.
	//This is to allow admin clients to move around the map even when the world is suspended.
	if (entry.entity.hasFlags(entity_admin)) {
		OpVector res;
		updateObserverEntry(entry, res);
		for (auto& op: res) {
			BaseWorld::instance().message(op, m_entity);
		}
	}
	if (!entry.markedForVisibilityRecalculation) {
		m_visibilityRecalculateQueue.emplace_back(&entry);
		entry.markedForVisibilityRecalculation = true;
	}

}

void PhysicalDomain::applyDestination(std::chrono::milliseconds tickSize,
									  BulletEntry& entry,
									  const PropelProperty* propelProp,
									  const Vector3Property<LocatedEntity>& destinationProp) {
	bool hasDestination = destinationProp.data().isValid();
	bool hasPropel = propelProp && propelProp->data().isValid() && propelProp->data() != WFMath::Vector<3>::ZERO();

	if (hasDestination && entry.collisionObject) {
		auto currentPos = entry.collisionObject->getWorldTransform().getOrigin();
		auto btDestination = Convert::toBullet(destinationProp.data());
		auto distance = currentPos.distance(btDestination);

		//Check if we're overlapping destination. Do this by getting the vertical cross section of the entity.
		//        auto entityRadius = std::max(std::max(entry.bbox.highCorner().x(), entry.bbox.highCorner().z()), std::max(std::abs(entry.bbox.lowCorner().x()), std::abs(entry.bbox.lowCorner().z())));

		//If we're within 0.1 meter we're already there.
		if (distance < 0.1f) {
			return;
		}

		double speed;
		if (entry.mode == ModeProperty::Mode::Submerged) {
			speed = entry.speedWater;
		} else {
			speed = entry.speedGround;
		}

		double propelSpeed = 1.0;
		if (hasPropel) {
			propelSpeed = propelProp->data().mag();
		}
		auto ticksAsSeconds = to_seconds(tickSize);

		//Check if we should lower our speed to arrive within the next tick.
		if (propelSpeed * speed * ticksAsSeconds > distance) {
			propelSpeed = speed / (distance * ticksAsSeconds);
		}

		auto direction = btDestination - currentPos;
		direction[1] = 0;
		direction.normalize();


		applyPropel(entry, direction * (float)propelSpeed);
	} else if (hasPropel) {
		applyPropel(entry, Convert::toBullet(propelProp->data()));
	}

}

void PhysicalDomain::applyPropel(BulletEntry& entry, btVector3 propel) {
	/**
	 * A callback which checks if the instance is "grounded", i.e. that there's a contact point which is below its center.
	 */
	struct IsGroundedCallback : public btCollisionWorld::ContactResultCallback {
		const btCollisionObject& m_body;
		bool& m_isGrounded;

		IsGroundedCallback(const btCollisionObject& body, bool& isGrounded)
			: btCollisionWorld::ContactResultCallback(), m_body(body), m_isGrounded(isGrounded) {
			m_collisionFilterGroup = body.getBroadphaseHandle()->m_collisionFilterGroup;
			m_collisionFilterMask = body.getBroadphaseHandle()->m_collisionFilterMask;
		}

		bool needsCollision(btBroadphaseProxy* proxy0) const override {
			bool collides = (proxy0->m_collisionFilterGroup & m_collisionFilterMask) != 0;
			collides = collides && (m_collisionFilterGroup & proxy0->m_collisionFilterMask);
			//Discount water bodies.
			return collides && ((btCollisionObject*)proxy0->m_clientObject)->getUserIndex() != USER_INDEX_WATER_BODY;
		}

		btScalar addSingleResult(btManifoldPoint& cp,
								 const btCollisionObjectWrapper* colObj0,
								 int partId0,
								 int index0,
								 const btCollisionObjectWrapper* colObj1,
								 int partId1,
								 int index1) override {
			//Local collision point, in the body's space
			btVector3 point;
			if (colObj0->m_collisionObject == &m_body) {
				point = cp.m_localPointA;
			} else {
				point = cp.m_localPointB;
			}

			if (point.y() <= 0) {
				m_isGrounded = true;
			}

			//Returned result is ignored.
			return 0;
		}
	};

	if (entry.collisionObject) {
		if (auto rigidBody = btRigidBody::upcast(entry.collisionObject.get())) {
			LocatedEntity& entity = entry.entity;


			//TODO: add support for flying and swimming
			if (!propel.isZero()) {
				cy_debug_print("PhysicalDomain::applyPropel " << entity.describeEntity() << " " << propel << " " << propel.length())

				//Check if we're trying to jump
				if (propel.m_floats[1] > 0) {
					auto jumpSpeedProp = entity.getPropertyType<double>("speed_jump");
					if (jumpSpeedProp && jumpSpeedProp->data() > 0) {

						bool isGrounded = false;
						IsGroundedCallback groundedCallback(*rigidBody, isGrounded);
						m_dynamicsWorld->contactTest(rigidBody, groundedCallback);
						if (isGrounded) {
							//If the entity is grounded, allow it to jump by setting the vertical velocity.
							btVector3 newVelocity = rigidBody->getLinearVelocity();
							newVelocity.m_floats[1] = static_cast<btScalar>(propel.m_floats[1] * jumpSpeedProp->data());
							rigidBody->setLinearVelocity(newVelocity);
							//We'll mark the entity as actively jumping here, and rely on the post-tick callback to reset it when it's not jumping anymore.
							entry.isJumping = true;
						}
					}
				}
				propel.m_floats[1] = 0; //Don't allow vertical velocity to be set for the continuous velocity.

				auto K = m_propellingEntries.find(entity.getIdAsInt());
				if (K == m_propellingEntries.end()) {
					if (entry.step_factor > 0) {
						auto height = entry.bbox.upperBound(1) - entry.bbox.lowerBound(1);
						m_propellingEntries.emplace(entity.getIdAsInt(),
							PropelEntry{
								.rigidBody = rigidBody,
								.bulletEntry = &entry,
								.velocity = propel,
								.stepHeight = (float)(height * entry.step_factor)
							});
					} else {
						m_propellingEntries.emplace(entity.getIdAsInt(),
							PropelEntry{
								.rigidBody = rigidBody,
								.bulletEntry = &entry,
								.velocity = propel,
								.stepHeight = 0
							});
					}
				} else {
					K->second.velocity = propel;
				}
			} else {
				btVector3 bodyVelocity = rigidBody->getLinearVelocity();
				bodyVelocity.setX(0);
				bodyVelocity.setZ(0);

				if (rigidBody->getCenterOfMassPosition().y() <= 0) {
					bodyVelocity.setY(0);
				}

				rigidBody->setLinearVelocity(bodyVelocity);
				double friction = 1.0; //Default to 1 if no "friction" prop is present.
				auto frictionProp = entity.getPropertyType<double>("friction");
				if (frictionProp) {
					friction = frictionProp->data();
				}
				rigidBody->setFriction(static_cast<btScalar>(friction));

				m_propellingEntries.erase(entity.getIdAsInt());

			}
		}
	}
}

void PhysicalDomain::applyTransform(LocatedEntity& entity,
									const TransformData& transformData,
									std::set<LocatedEntity*>& transformedEntities) {
	//First handle any changes to the "mode_data" property, to see if entities are planted.
	auto I = m_entries.find(entity.getIdAsInt());
	auto& entry = I->second;
	BulletEntry* entryPlantedOn = nullptr;

	if (transformData.plantedOnEntity) {
		if (transformData.plantedOnEntity == &entity) {
			spdlog::warn("Tried to plant entity {} on itself.", entity.describeEntity());
		} else {
			auto plantedOnI = m_entries.find(transformData.plantedOnEntity->getIdAsInt());
			if (plantedOnI != m_entries.end()) {
				entryPlantedOn = plantedOnI->second.get();
			}
		}
	}
	plantOnEntity(*entry, entryPlantedOn);

	applyTransformInternal(*entry, transformData.orientation, transformData.pos, transformData.impulseVelocity, transformedEntities, true);
}

void PhysicalDomain::applyTransformInternal(BulletEntry& entry,
											const WFMath::Quaternion& orientation,
											const WFMath::Point<3>& pos,
											const WFMath::Vector<3>& impulseVelocity,
											std::set<LocatedEntity*>& transformedEntities,
											bool calculatePosition) {
	WFMath::Point<3> oldPos = entry.positionProperty.data();

	bool hadChange = false;

	btRigidBody* rigidBody = nullptr;
	if (entry.collisionObject) {
		rigidBody = btRigidBody::upcast(entry.collisionObject.get());
	}
	WFMath::Quaternion rotationChange = WFMath::Quaternion::IDENTITY();
	if (orientation.isValid() && !orientation.isEqualTo(entry.orientationProperty.data(), 0.001)) {
		cy_debug_print("PhysicalDomain::new orientation " << entry.entity.describeEntity() << " " << orientation)

		if (entry.collisionShape) {
			btTransform& transform = entry.collisionObject->getWorldTransform();

			transform.setRotation(Convert::toBullet(orientation));
			transform.setOrigin(Convert::toBullet(entry.positionProperty.data()));
			transform *= btTransform(btQuaternion::getIdentity(), entry.centerOfMassOffset).inverse();

			entry.collisionObject->setWorldTransform(transform);
		}
		if (entry.orientationProperty.data().isValid()) {
			rotationChange = orientation * entry.orientationProperty.data().inverse();
		} else {
			rotationChange = orientation;
		}
		entry.orientationProperty.data() = orientation;
		entry.entity.applyProperty(entry.orientationProperty);
		//        entity.m_location.m_orientation = orientation;
		//        entity.removeFlags(entity_orient_clean);
		hadChange = true;
	}
	if (pos.isValid()) {
		applyNewPositionForEntity(entry, pos, calculatePosition);
		if (!oldPos.isEqualTo(entry.positionProperty.data())) {
			//entity.removeFlags(entity_pos_clean);
			hadChange = true;
			//Check if there previously wasn't any valid pos, and thus no valid collision instances.
			if (entry.positionProperty.data().isValid() && !oldPos.isValid()) {
				if (entry.collisionObject) {
					short collisionMask;
					short collisionGroup;
					getCollisionFlagsForEntity(entry, collisionGroup, collisionMask);
					if (rigidBody) {
						m_dynamicsWorld->addRigidBody(rigidBody, collisionGroup, collisionMask);
					} else {
						m_dynamicsWorld->addCollisionObject(entry.collisionObject.get(), collisionGroup, collisionMask);
					}
				}
				if (entry.viewSphere) {
					m_visibilityWorld->addCollisionObject(entry.viewSphere.get(),
						entry.entity.hasFlags(entity_admin) ? VISIBILITY_MASK_OBSERVABLE | VISIBILITY_MASK_OBSERVABLE_PRIVATE : VISIBILITY_MASK_OBSERVABLE,
						VISIBILITY_MASK_OBSERVER);
				}
				if (entry.visibilitySphere) {
					m_visibilityWorld->addCollisionObject(entry.visibilitySphere.get(),
						VISIBILITY_MASK_OBSERVER,
						entry.entity.hasFlags(entity_visibility_protected) || entry.entity.hasFlags(entity_visibility_private) ? VISIBILITY_MASK_OBSERVABLE_PRIVATE : VISIBILITY_MASK_OBSERVABLE);
				}
			}
		}
	}
	if (impulseVelocity.isValid() && rigidBody) {
		rigidBody->applyCentralImpulse(Convert::toBullet(impulseVelocity));
	}

	if (hadChange) {

		if (entry.collisionObject && entry.collisionObject->getUserIndex() == USER_INDEX_WATER_BODY) {
			//We moved a water body, check with all entities that might be contained.
			//This is quite expensive since we're iterating through all entities (on an unordered map).
			//However, since we normally never expect a water body to move on a live server we do it this way, since we then avoid
			//having to keep track of which entities are in water and not.
			for (auto& entityEntry: m_entries) {
				auto childEntry = entityEntry.second.get();
				if (childEntry->waterNearby == &entry) {
					if (!childEntry->addedToMovingList) {
						m_movingEntities.emplace_back(childEntry);
						childEntry->addedToMovingList = true;
						childEntry->markedAsMovingThisFrame = false;
						childEntry->markedAsMovingLastFrame = false;
					}
				}
			}
		}

		//Only check for resting entities if the entity has been moved; not if the velocity has changed.
		if (pos.isValid() || orientation.isValid()) {
			transformedEntities.insert(&entry.entity);
			transformRestingEntities(entry, entry.positionProperty.data() - oldPos, rotationChange, transformedEntities);
		}
		if (entry.collisionShape) {
			//Since we've deactivated automatic updating of all aabbs each tick we need to do it ourselves when updating the position.
			m_dynamicsWorld->updateSingleAabb(entry.collisionObject.get());

			if (rigidBody && rigidBody->getInvMass() != 0) {
				rigidBody->activate();
			}
		}
		processMovedEntity(entry, std::chrono::milliseconds{0});
	}
}

void PhysicalDomain::refreshTerrain(const std::vector<WFMath::AxisBox<2>>& areas) {
	//Schedule dirty terrain areas for update in processDirtyTerrainAreas() which is called for each tick.
	m_dirtyTerrainSegmentAreas.insert(m_dirtyTerrainSegmentAreas.end(), areas.begin(), areas.end());
}

void PhysicalDomain::processDirtyTerrainAreas() {
	if (!m_terrain) {
		m_dirtyTerrainSegmentAreas.clear();
		return;
	}

	if (m_dirtyTerrainSegmentAreas.empty()) {
		return;
	}
	rmt_ScopedCPUSample(PhysicalDomain_processDirtyTerrainAreas, 0)

	std::set<Mercator::Segment*> dirtySegments;
	for (auto& area: m_dirtyTerrainSegmentAreas) {
		m_terrain->processSegments(area, [&](Mercator::Segment& s, int, int) { dirtySegments.insert(&s); });
	}
	m_dirtyTerrainSegmentAreas.clear();

	std::optional<float> friction;
	auto frictionProp = m_entity.getPropertyType<double>("friction");
	if (frictionProp) {
		friction = (float)frictionProp->data();
	}
	std::optional<float> frictionRolling;
	auto frictionRollingProp = m_entity.getPropertyType<double>("friction_roll");
	if (frictionRollingProp) {
		frictionRolling = (float)frictionRollingProp->data();
	}
	std::optional<float> frictionSpinning;
	auto frictionSpinningProp = m_entity.getPropertyType<double>("friction_spin");
	if (frictionSpinningProp) {
		frictionSpinning = (float)frictionSpinningProp->data();
	}

	auto worldHeight = mContainingEntityEntry.bbox.highCorner().y() - mContainingEntityEntry.bbox.lowCorner().y();

	cy_debug_print("dirty segments: " << dirtySegments.size())
	for (auto& segment: dirtySegments) {
		cy_debug_print("rebuilding segment at x: " << segment->getXRef() << " z: " << segment->getZRef())

		auto& terrainEntry = buildTerrainPage(*segment);
		if (friction) {
			terrainEntry.rigidBody->setFriction(*friction);
		}
		if (frictionRolling) {
			terrainEntry.rigidBody->setRollingFriction(*frictionRolling);
		}
		if (frictionSpinning) {
			terrainEntry.rigidBody->setSpinningFriction(*frictionSpinning);
		}

		struct : public btCollisionWorld::ContactResultCallback {
			std::vector<PhysicalDomain::BulletEntry*> m_entries;

			btScalar addSingleResult(btManifoldPoint& cp,
									 const btCollisionObjectWrapper* colObj0Wrap,
									 int partId0,
									 int index0,
									 const btCollisionObjectWrapper* colObj1Wrap,
									 int partId1,
									 int index1) override {
				auto* bulletEntry = static_cast<BulletEntry*>(colObj1Wrap->m_collisionObject->getUserPointer());
				if (bulletEntry && (m_entries.empty() || *m_entries.rbegin() != bulletEntry)) {
					m_entries.emplace_back(bulletEntry);
				}
				return btScalar{1.0};
			}
		} callback;

		callback.m_collisionFilterGroup = COLLISION_MASK_TERRAIN;
		callback.m_collisionFilterMask = COLLISION_MASK_PHYSICAL | COLLISION_MASK_NON_PHYSICAL | COLLISION_MASK_STATIC;

		auto area = segment->getRect();
		WFMath::Vector<2> size = area.highCorner() - area.lowCorner();

		btBoxShape boxShape(btVector3((float)size.x() * 0.5f, (float)worldHeight, (float)size.y() * 0.5f));
		btCollisionObject collObject;
		collObject.setCollisionShape(&boxShape);
		auto center = area.getCenter();
		collObject.setWorldTransform(btTransform(btQuaternion::getIdentity(), btVector3((float)center.x(), 0, (float)center.y())));
		m_dynamicsWorld->contactTest(&collObject, callback);

		cy_debug_print("Matched " << callback.m_entries.size() << " entries")
		for (BulletEntry* entry: callback.m_entries) {
			cy_debug_print("Adjusting " << entry->entity.describeEntity())
			Anonymous anon;
			anon->setId(entry->entity.getIdAsString());
			std::vector<double> posList;
			addToEntity(entry->positionProperty.data(), posList);
			anon->setPos(posList);
			Move move;
			move->setTo(entry->entity.getIdAsString());
			move->setFrom(entry->entity.getIdAsString());
			move->setArgs1(anon);
			entry->entity.sendWorld(move);
		}
	}
}


void PhysicalDomain::processDirtyTerrainSurfaces() {
	if (!m_terrain) {
		m_dirtyTerrainSurfaceAreas.clear();
		return;
	}

	if (m_dirtyTerrainSurfaceAreas.empty()) {
		return;
	}
	rmt_ScopedCPUSample(PhysicalDomain_processDirtyTerrainSurfaces, 0)

	std::set<Mercator::Segment*> dirtySegments;
	for (auto& area: m_dirtyTerrainSurfaceAreas) {
		m_terrain->processSegments(area, [&](Mercator::Segment& s, int, int) { dirtySegments.insert(&s); });
	}
	m_dirtyTerrainSurfaceAreas.clear();


	cy_debug_print("dirty segments: " << dirtySegments.size())
	for (auto& segment: dirtySegments) {
		cy_debug_print("rebuilding segment at x: " << segment->getXRef() << " z: " << segment->getZRef())

		segment->populateSurfaces();
	}
}

void PhysicalDomain::sendMoveSight(BulletEntry& entry, bool posChange, bool velocityChange, bool orientationChange, bool angularChange, bool modeChange) {
	if (!entry.observingThis.empty()) {
		LocatedEntity& entity = entry.entity;
		auto& lastSentLocation = entry.lastSentLocation;
		bool shouldSendOp = false;
		Anonymous move_arg;
		if (velocityChange) {
			::addToEntity(entry.velocityProperty.data(), move_arg->modifyVelocity());
			shouldSendOp = true;
			lastSentLocation.velocity = entry.velocityProperty.data();
		}
		if (angularChange) {
			move_arg->setAttr("angular", entry.angularVelocityProperty.data().toAtlas());
			shouldSendOp = true;
			lastSentLocation.angularVelocity = entry.angularVelocityProperty.data();
		}
		if (orientationChange) {
			move_arg->setAttr("orientation", entry.orientationProperty.data().toAtlas());
			shouldSendOp = true;
			lastSentLocation.orientation = entry.orientationProperty.data();
		}
		//If the velocity changes we should also send the position, to make it easier for clients to project position.
		if (posChange || velocityChange) {
			::addToEntity(entry.positionProperty.data(), move_arg->modifyPos());
			shouldSendOp = true;
			lastSentLocation.pos = entry.positionProperty.data();
		}
		if (modeChange) {
			auto prop = entity.getPropertyClassFixed<ModeProperty>();
			if (prop) {
				Atlas::Message::Element element;
				if (prop->get(element) == 0) {
					move_arg->setAttr("mode", element);
					shouldSendOp = true;
				}
			}
		}

		if (shouldSendOp) {
			Set setOp;
			move_arg->setId(entity.getIdAsString());
			if (debug_flag) {
				cy_debug_print("Sending set op for movement.")
				if (entry.velocityProperty.data().isValid()) {
					cy_debug_print("new velocity: " << entry.velocityProperty.data() << " " << entry.velocityProperty.data().mag())
				}
			}

			//entity.m_location.addToEntity(move_arg);

			setOp->setArgs1(move_arg);
			setOp->setFrom(entity.getIdAsString());
			setOp->setTo(entity.getIdAsString());
			auto now = BaseWorld::instance().getTimeAsMilliseconds();
			setOp->setStamp(now.count());

			for (BulletEntry* observer: entry.observingThis) {
				Sight s;
				s->setArgs1(setOp);
				s->setTo(observer->entity.getIdAsString());
				s->setFrom(entity.getIdAsString());
				s->setStamp(now.count());

				entity.sendWorld(s);
			}
		}
	}
}

void PhysicalDomain::processMovedEntity(BulletEntry& bulletEntry, std::chrono::milliseconds timeSinceLastUpdate) {
	auto& pos = bulletEntry.positionProperty.data();
	auto& orientation = bulletEntry.orientationProperty.data();
	auto& velocity = bulletEntry.velocityProperty.data();
	auto& angular = bulletEntry.angularVelocityProperty.data();
	LocatedEntity& entity = bulletEntry.entity;
	auto& lastSentLocation = bulletEntry.lastSentLocation;
	//This should normally not happen, but let's check nonetheless.
	if (!pos.isValid()) {
		return;
	}

	bool orientationChange = orientation.isValid() && (!lastSentLocation.orientation.isValid() || !orientation.isEqualTo(lastSentLocation.orientation, 0.1f));
	bool posChange = false;
	if (!lastSentLocation.pos.isValid()) {
		posChange = true;
	} else {
		//If position differs from last sent position, and there's no velocity, then send position update
		if (lastSentLocation.pos != pos && (!velocity.isValid() || velocity == WFMath::Vector<3>::ZERO())) {
			posChange = true;
		} else {
			if (lastSentLocation.velocity.isValid()) {
				auto projectedPosition = lastSentLocation.pos + (lastSentLocation.velocity * to_seconds(timeSinceLastUpdate));
				if (WFMath::Distance(pos, projectedPosition) > 0.5) {
					posChange = true;
				}
			}
		}
	}

	if (false) {
		sendMoveSight(bulletEntry, true, true, true, true, true);
	} else {

		bool velocityChange = false;

		if (bulletEntry.velocityProperty.data().isValid()) {
			bool hadValidVelocity = lastSentLocation.velocity.isValid();
			//Send an update if either the previous velocity was invalid, or any of the velocity components have changed enough, or if either the new or the old velocity is zero.
			if (!hadValidVelocity) {
				cy_debug_print("No previous valid velocity " << entity.describeEntity() << " " << lastSentLocation.velocity)
				velocityChange = true;
				lastSentLocation.velocity = bulletEntry.velocityProperty.data();
			} else {
				bool xChange = !fuzzyEquals(velocity.x(), lastSentLocation.velocity.x(), 0.01);
				bool yChange = !fuzzyEquals(velocity.y(), lastSentLocation.velocity.y(), 0.01);
				bool zChange = !fuzzyEquals(velocity.z(), lastSentLocation.velocity.z(), 0.01);
				bool hadZeroVelocity = lastSentLocation.velocity.isEqualTo(WFMath::Vector<3>::ZERO());
				if (xChange || yChange || zChange) {
					cy_debug_print("Velocity changed " << entity.describeEntity() << " " << velocity)
					velocityChange = true;
					lastSentLocation.velocity = bulletEntry.velocityProperty.data();
				} else if (bulletEntry.velocityProperty.data().isEqualTo(WFMath::Vector<3>::ZERO()) && !hadZeroVelocity) {
					cy_debug_print("Old or new velocity zero " << entity.describeEntity() << " " << velocity)
					velocityChange = true;
					lastSentLocation.velocity = bulletEntry.velocityProperty.data();
				}
			}
		}
		bool angularChange = false;

		if (bulletEntry.angularVelocityProperty.data().isValid()) {
			bool hadZeroAngular = lastSentLocation.angularVelocity.isEqualTo(WFMath::Vector<3>::ZERO());
			angularChange = !fuzzyEquals(lastSentLocation.angularVelocity, angular, 0.01);
			if (!angularChange && bulletEntry.angularVelocityProperty.data().isEqualTo(WFMath::Vector<3>::ZERO()) && !hadZeroAngular) {
				cy_debug_print("Angular changed " << entity.describeEntity() << " " << angular)
				angularChange = true;
				lastSentLocation.angularVelocity = bulletEntry.angularVelocityProperty.data();
			}
		}

		if (posChange || velocityChange || orientationChange || angularChange || bulletEntry.modeChanged) {
			//Increase sequence number as properties have changed.
			entity.increaseSequenceNumber();
			sendMoveSight(bulletEntry, posChange, velocityChange, orientationChange, angularChange, bulletEntry.modeChanged);
			lastSentLocation.pos = bulletEntry.positionProperty.data();
			bulletEntry.modeChanged = false;
		}

	}
	//If the entity has moved and there are observations attached to it, we need to check if these still are valid (like a character
	// having opened a chest, and then moving away from it).
	if (posChange && !bulletEntry.closenessObservations.empty()) {
		//Since callbacks can remove observations we need to first collect att invalid observations, and then remove them carefully.
		std::vector<ClosenessObserverEntry*> invalidEntries;
		for (auto& observation: bulletEntry.closenessObservations) {
			if (!isWithinReach(*observation->reacher, *observation->target, (float)observation->reach, {})) {
				invalidEntries.emplace_back(observation);
			}
		}


		for (auto& observation: invalidEntries) {
			//It's important that we check that the observation still is valid, since it's possible that callbacks alters the collections.
			auto I = bulletEntry.closenessObservations.find(observation);
			if (I != bulletEntry.closenessObservations.end()) {
				if (&bulletEntry == observation->reacher) {
					observation->target->closenessObservations.erase(observation);
				} else {
					observation->reacher->closenessObservations.erase(observation);
				}
				bulletEntry.closenessObservations.erase(I);

				auto J = m_closenessObservations.find(observation);
				//Hold on to an instance while we call callbacks and erase it.
				auto observerInstance = std::move(J->second);
				m_closenessObservations.erase(J);

				if (observation->callback) {
					observation->callback();
				}
			}
		}
	}
	updateTerrainMod(bulletEntry);
}

void PhysicalDomain::tick(std::chrono::milliseconds tickSize, OpVector& res) {
	//    CProfileManager::Reset();
	//    CProfileManager::Increment_Frame_Counter();
	rmt_ScopedCPUSample(PhysicalDomain_tick, 0)


	auto start = std::chrono::steady_clock::now();
	/**
	 * An entry of a projectile hitting another entry.
	 */
	struct BulletCollisionEntry {
		/**
		 * The entry that was hit.
		 */
		BulletEntry* bulletEntry;
		/**
		 * The position in the world where the hit occurred.
		 */
		btVector3 pos;
	};

	//  static std::map<BulletEntry*, BulletCollisionEntry> projectileCollisions;
	static std::vector<std::pair<BulletEntry*, BulletCollisionEntry>> projectileCollisions;

	gContactProcessedCallback = [](btManifoldPoint& cp, void* body0, void* body1) -> bool {
		auto object0 = static_cast<btCollisionObject*>(body0);
		auto bulletEntry0 = static_cast<BulletEntry*>(object0->getUserPointer());
		auto object1 = static_cast<btCollisionObject*>(body1);
		auto bulletEntry1 = static_cast<BulletEntry*>(object1->getUserPointer());

		if (bulletEntry0->mode == ModeProperty::Mode::Projectile) {
			projectileCollisions.emplace_back(bulletEntry0, BulletCollisionEntry{.bulletEntry = bulletEntry1, .pos = cp.getPositionWorldOnB()});
		}
		if (bulletEntry1->mode == ModeProperty::Mode::Projectile) {
			projectileCollisions.emplace_back(bulletEntry1, BulletCollisionEntry{.bulletEntry = bulletEntry0, .pos = cp.getPositionWorldOnA()});
		}
		return true;
	};

	auto simulationSpeedProp = m_entity.getPropertyClassFixed<SimulationSpeedProperty>();
	if (simulationSpeedProp) {
		// Need to do some casts instead of "tickSize *= simulationSpeedProp->data();" to get guarantees of correct conversions.
		tickSize = std::chrono::milliseconds{(long)((double)tickSize.count() * simulationSpeedProp->data())};
	}
	auto tickSizeInSeconds = to_seconds(tickSize);

	projectileCollisions.clear();

	postDuration = {};

	for (auto& bulletEntry: m_propelUpdateQueue) {
		//We'll use the "m_propelUpdateQueue" also for entities with _destination set, so it's not always they have a "_propel" property.
		if (bulletEntry->control.propelProperty) {
			auto propel = bulletEntry->control.propelProperty->data().isValid() ? Convert::toBullet(bulletEntry->control.propelProperty->data()) : btVector3(0, 0, 0);
			applyPropel(*bulletEntry, propel);
		}
	}
	m_propelUpdateQueue.clear();
	for (auto& bulletEntry: m_directionUpdateQueue) {
		auto directionProperty = bulletEntry->control.directionProperty;
		auto& direction = directionProperty->data();
		if (direction.isValid()) {
			if (bulletEntry->collisionShape) {
				btTransform transform = bulletEntry->collisionObject->getWorldTransform();
				auto existingRotation = transform.getRotation();
				auto newRotation = Convert::toBullet(direction);
				if (existingRotation != newRotation) {
					transform.setRotation(newRotation);
					bulletEntry->collisionObject->setWorldTransform(transform);

					bulletEntry->orientationProperty.data() = direction;
					bulletEntry->orientationProperty.flags().removeFlags(prop_flag_persistence_clean);
					bulletEntry->entity.applyProperty(bulletEntry->orientationProperty);
					bulletEntry->entity.removeFlags(entity_orient_clean);

					if (!bulletEntry->addedToMovingList) {
						m_movingEntities.emplace_back(bulletEntry);
						bulletEntry->addedToMovingList = true;
						bulletEntry->markedAsMovingThisFrame = false;
						bulletEntry->markedAsMovingLastFrame = false;
					}
				}
				//                    transform.setRotation(Convert::toBullet(orientation));
				//                    transform.setOrigin(Convert::toBullet(entry.positionProperty.data()));
				//                    transform *= btTransform(btQuaternion::getIdentity(), entry.centerOfMassOffset).inverse();

				//entry.collisionObject->setWorldTransform(transform);
			}
		}

	}
	m_directionUpdateQueue.clear();

	for (auto& entry: m_entriesWithDestination) {
		applyDestination(tickSize, *entry, entry->control.propelProperty, *entry->control.destinationProperty);
	}


	//Step simulations with 60 hz.
	m_dynamicsWorld->stepSimulation(tickSizeInSeconds, static_cast<int>(60 * tickSizeInSeconds));
	auto interim = std::chrono::steady_clock::now() - start;

	//CProfileManager::dumpAll();

	//The list of projectilecollisions will contain duplicates, so we need to keep track of the last
	//processed and check that it does not repeat.
	BulletEntry* lastCollisionEntry = nullptr;
	for (const auto& entry: projectileCollisions) {
		auto projectileEntry = entry.first;
		if (lastCollisionEntry == projectileEntry) {
			continue;
		}
		lastCollisionEntry = projectileEntry;
		auto& collisionEntry = entry.second;
		const auto modeDataProperty = projectileEntry->entity.getPropertyClassFixed<ModeDataProperty>();
		Atlas::Objects::Entity::Anonymous ent;
		//If the projectile data contained information on which entity caused the projectile to fly away, copy that.
		//This is needed for game rules to determine which entity hit another.
		if (modeDataProperty && modeDataProperty->getProjectileData().entity) {
			ent->setId(modeDataProperty->getProjectileData().entity->getIdAsString());
		} else {
			ent->setId(collisionEntry.bulletEntry->entity.getIdAsString());
		}
		std::vector<double> posList;
		addToEntity(Convert::toWF<WFMath::Point<3>>(collisionEntry.pos), posList);
		ent->setPos(std::move(posList));
		ent->setLoc(m_entity.getIdAsString());
		if (modeDataProperty && modeDataProperty->getMode() == ModeProperty::Mode::Projectile) {
			auto& projectileData = modeDataProperty->getProjectileData();
			//Copy any data found in "mode_data".
			for (const auto& projectile_entry: projectileData.extra) {
				ent->setAttr(projectile_entry.first, projectile_entry.second, &AtlasFactories::factories);
			}
		}
		Atlas::Objects::Operation::Hit hit;
		hit->setArgs1(std::move(ent));
		hit->setTo(projectileEntry->entity.getIdAsString());
		hit->setFrom(collisionEntry.bulletEntry->entity.getIdAsString());

		auto hitCopy = hit.copy();
		hitCopy->setTo(collisionEntry.bulletEntry->entity.getIdAsString());
		hitCopy->setFrom(projectileEntry->entity.getIdAsString());

		//We need to make sure that Hit ops gets their correct "from".
		collisionEntry.bulletEntry->entity.sendWorld(std::move(hit));
		projectileEntry->entity.sendWorld(std::move(hitCopy));
	}

	auto visStart = std::chrono::steady_clock::now();
	updateVisibilityOfDirtyEntities(res);
	auto visDuration = std::chrono::steady_clock::now() - visStart;

	processWaterBodies();

	//We process the vector of moving entities as efficient as possible by not doing
	//any reallocations during the processing. If any entry should be removed (because it's
	//not moving anymore) we relocate the last entry to the position we're at.
	//Once we're done with processing we'll shrink the vector if any element was removed.
	//Note that during this phase "last frame" refers to this frame, and "this frame" refers
	//to the future frame.
	size_t movingSize = m_movingEntities.size();
	for (size_t i = 0; i < movingSize;) {
		if (auto movedEntry = m_movingEntities[i]; !movedEntry->markedAsMovingThisFrame) {
			//Stopped moving
			if (movedEntry->angularVelocityProperty.data().isValid()) {
				movedEntry->angularVelocityProperty.data().zero();
			}
			if (movedEntry->velocityProperty.data().isValid()) {
				cy_debug_print("Stopped moving " << movedEntry->entity.describeEntity())
				movedEntry->velocityProperty.data().zero();
			}
			processMovedEntity(*movedEntry, tickSize);
			movedEntry->markedAsMovingLastFrame = false;
			movedEntry->addedToMovingList = false;

			//If we're removing the last entry just skip
			if (i == movingSize - 1) {
				--movingSize;
			} else {
				//Move the last element to this position and don't advance i
				m_movingEntities[i] = m_movingEntities[movingSize - 1];
				--movingSize;
			}
		} else if (!movedEntry->markedAsMovingLastFrame) {
			//Started moving
			movedEntry->markedAsMovingLastFrame = true;
			movedEntry->markedAsMovingThisFrame = false;
			processMovedEntity(*movedEntry, tickSize);
			++i;
		} else {
			//Moved previously and has continued to move
			movedEntry->markedAsMovingLastFrame = true;
			movedEntry->markedAsMovingThisFrame = false;
			processMovedEntity(*movedEntry, tickSize);
			++i;
		}
	}
	//If we've removed any entries we need to shrink the vector
	if (movingSize != m_movingEntities.size()) {
		m_movingEntities.resize(movingSize);
	}

	processDirtyTerrainAreas();
	processDirtyTerrainSurfaces();

	auto duration = std::chrono::steady_clock::now() - start;
	auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration);
	if (debug_flag) {
		spdlog::log(microseconds.count() > 3000 ? spdlog::level::warn : spdlog::level::info,
			"Physics took {} μs (just stepSimulation {} μs, visibility {} μs, tick size {} μs, visibility queue: {}, postTick: {} μs, moving count: {}).",
			microseconds.count(),
			std::chrono::duration_cast<std::chrono::microseconds>(interim).count(),
			std::chrono::duration_cast<std::chrono::microseconds>(visDuration).count(),
			std::chrono::duration_cast<std::chrono::microseconds>(tickSize).count(),
			m_visibilityRecalculateQueue.size(),
			std::chrono::duration_cast<std::chrono::microseconds>(postDuration).count(),
			movingSize
		);
	}
	s_processTimeUs += microseconds.count();
}

void PhysicalDomain::processWaterBodies() {
	rmt_ScopedCPUSample(PhysicalDomain_processWaterBodies, 0)
	auto testEntityIsSubmergedFn = [&](BulletEntry* bulletEntry, BulletEntry* waterEntry) -> bool {
		bool isInside;
		//If the water body entity has been deleted it will have been set to null.
		if (waterEntry == nullptr) {
			isInside = false;
		} else if (waterEntry == bulletEntry) {
			isInside = false;
			bulletEntry->waterNearby = nullptr;
		} else {
			auto waterBody = waterEntry->collisionObject.get();
			auto shapeType = waterBody->getCollisionShape()->getShapeType();
			auto overlappingObject = bulletEntry->collisionObject.get();
			if (shapeType == BOX_SHAPE_PROXYTYPE) {
				auto boxShape = dynamic_cast<btBoxShape*>(waterBody->getCollisionShape());
				//Translate position of overlapping shape into the water body's space
				auto testPos = overlappingObject->getWorldTransform().getOrigin() - waterBody->getWorldTransform().getOrigin();
				testPos = testPos * waterBody->getWorldTransform().getBasis().inverse();
				isInside = boxShape->isInside(testPos, 0);
			} else if (shapeType == STATIC_PLANE_PROXYTYPE) {
				isInside = overlappingObject->getWorldTransform().getOrigin().y() <= waterBody->getWorldTransform().getOrigin().y();
			} else {
				//We only support planes and boxes
				return false;
			}
		}
		if (isInside) {
			if (bulletEntry->mode != ModeProperty::Mode::Submerged) {
				if (auto rigidBody = btRigidBody::upcast(bulletEntry->collisionObject.get())) {
					rigidBody->setGravity(btVector3(0, 0, 0));
					rigidBody->setDamping(0.8, 0);
				}
				bulletEntry->mode = ModeProperty::Mode::Submerged;
				auto& prop = bulletEntry->entity.requirePropertyClassFixed<ModeProperty>("submerged");
				prop.set("submerged");
				bulletEntry->modeChanged = true;
				if (!bulletEntry->addedToMovingList) {
					m_movingEntities.emplace_back(bulletEntry);
					bulletEntry->addedToMovingList = true;
					bulletEntry->markedAsMovingThisFrame = false;
					bulletEntry->markedAsMovingLastFrame = false;
				}
			}
			return true;
		} else {
			if (bulletEntry->mode == ModeProperty::Mode::Submerged) {
				if (auto rigidBody = btRigidBody::upcast(bulletEntry->collisionObject.get())) {
					rigidBody->setGravity(m_dynamicsWorld->getGravity());
					rigidBody->setDamping(0, 0);
				}
				bulletEntry->mode = ModeProperty::Mode::Free;
				auto& prop = bulletEntry->entity.requirePropertyClassFixed<ModeProperty>("free");
				prop.set("free");
				bulletEntry->modeChanged = true;
				if (!bulletEntry->addedToMovingList) {
					m_movingEntities.emplace_back(bulletEntry);
					bulletEntry->addedToMovingList = true;
					bulletEntry->markedAsMovingThisFrame = false;
					bulletEntry->markedAsMovingLastFrame = false;
				}
			}
			return false;
		}
	};

	for (auto entry: m_movingEntities) {
		if (entry->waterNearby) {
			testEntityIsSubmergedFn(entry, entry->waterNearby);
		}
	}

}

bool PhysicalDomain::getTerrainHeight(float x, float y, float& height) const {
	if (m_terrain) {
		Mercator::Segment* s = m_terrain->getSegmentAtPos(x, y);
		if (s != nullptr && !s->isValid()) {
			s->populate();
		}
		return m_terrain->getHeight(x, y, height);
	} else {
		height = (float)mContainingEntityEntry.bbox.lowCorner().y();
		return false;
	}
}

void PhysicalDomain::transformRestingEntities(const BulletEntry& entry,
											  const WFMath::Vector<3>& posTransform,
											  const WFMath::Quaternion& orientationChange,
											  std::set<LocatedEntity*>& transformedEntities) {
	if (auto* collObject = entry.collisionObject.get()) {

		//Check if there are any objects resting on us, and move them along too.
		std::set<BulletEntry*> objectsRestingOnOurObject = entry.attachedEntities;
		int numManifolds = m_dynamicsWorld->getDispatcher()->getNumManifolds();
		for (int i = 0; i < numManifolds; i++) {
			btPersistentManifold* contactManifold = m_dynamicsWorld->getDispatcher()->getManifoldByIndexInternal(i);
			const btCollisionObject* obA = contactManifold->getBody0();
			const btCollisionObject* obB = contactManifold->getBody1();

			const btCollisionObject* otherObject;
			bool aIsOurObject = false;

			if (obA == collObject) {
				otherObject = obB;
				aIsOurObject = true;
			} else if (obB == collObject) {
				otherObject = obA;
			} else {
				continue;
			}

			if (otherObject->getInternalType() != btCollisionObject::CO_GHOST_OBJECT && !otherObject->isStaticObject()) {

				if (auto* restingEntry = static_cast<BulletEntry*>(otherObject->getUserPointer())) {
					int numContacts = contactManifold->getNumContacts();
					for (int j = 0; j < numContacts; j++) {
						btManifoldPoint& pt = contactManifold->getContactPoint(j);
						if (pt.getDistance() < 0.f) {
							const btVector3& ptA = pt.getPositionWorldOnA();
							const btVector3& ptB = pt.getPositionWorldOnB();
							//Check if the other object rests on our object.
							//Note that due to how collision is handled, the vertical check is "inversed".
							//I.e. if object A rests on object B, the collision point on A will actually
							// be _below_ the point on B, since they overlap.
							if (aIsOurObject) {
								if (ptA.y() > ptB.y()) {
									objectsRestingOnOurObject.emplace(restingEntry);
									break;
								}
							} else {
								if (ptB.y() > ptA.y()) {
									objectsRestingOnOurObject.emplace(restingEntry);
									break;
								}
							}
						}
					}
				}
			}
		}

		auto childTransform = posTransform;
		if (orientationChange.isValid()) {
			childTransform.rotate(orientationChange);
		}
		//Move all of the objects that were resting on our object.
		for (auto& restingEntry: objectsRestingOnOurObject) {
			//Check that we haven't already handled this entry, to avoid infinite loop with complex shapes resting on each other.
			if (transformedEntities.contains(&restingEntry->entity)) {
				continue;
			}
			auto relativePos = restingEntry->positionProperty.data() - (entry.positionProperty.data() - posTransform);

			if (orientationChange.isValid()) {
				relativePos.rotate(orientationChange);
			}

			applyTransformInternal(*restingEntry,
				restingEntry->orientationProperty.data() * orientationChange,
				entry.positionProperty.data() + relativePos,
				WFMath::Vector<3>(),
				transformedEntities,
				false);

		}
	}
}

void PhysicalDomain::plantOnEntity(BulletEntry& plantedEntry, BulletEntry* entryPlantedOn) {
	auto existingModeDataProp = plantedEntry.entity.getPropertyClassFixed<ModeDataProperty>();

	if (existingModeDataProp && existingModeDataProp->getMode() == ModeProperty::Mode::Planted) {
		auto& plantedOnData = existingModeDataProp->getPlantedOnData();
		//Check if we're already planted, and perhaps should be detached.
		if (plantedOnData.entityId) {
			if (entryPlantedOn && *plantedOnData.entityId == entryPlantedOn->entity.getIdAsInt()) {
				//Already planted on entity, nothing to do
				return;
			}
			if (!entryPlantedOn && !plantedOnData) {
				//Not planted already, and not being planted, nothing to do.
				return;
			}

			auto I = m_entries.find(*plantedOnData.entityId);
			if (I != m_entries.end()) {
				I->second->attachedEntities.erase(&plantedEntry);
			}
		}

	} else {
		if (!entryPlantedOn) {
			//No prop exists, and we shouldn't be planted, just return.
			return;
		}
	}

	//We need to change the property for this
	auto& newModeDataProp = plantedEntry.entity.requirePropertyClassFixed<ModeDataProperty>();

	if (entryPlantedOn) {
		newModeDataProp.setPlantedData({.entityId = entryPlantedOn->entity.getIdAsInt()});
		entryPlantedOn->attachedEntities.insert(&plantedEntry);
	} else {
		newModeDataProp.clearData();
	}

	newModeDataProp.addFlags(prop_flag_unsent);
}

bool PhysicalDomain::isEntityReachable(const LocatedEntity& reachingEntity, double reach, const LocatedEntity& queriedEntity, const WFMath::Point<3>& positionOnQueriedEntity) const {
	if (&reachingEntity == &m_entity) {
		//If the entity to which the domain belongs itself is reaching for a contained entities it's allowed.
		return true;
	}

	//For physical domains we require "reach" to be set in order for anything to reach.
	if (reach == 0) {
		return false;
	}

	auto reachingEntryI = m_entries.find(reachingEntity.getIdAsInt());
	if (reachingEntryI != m_entries.end()) {
		auto& reachingEntityEntry = reachingEntryI->second;
		auto& positionOfReachingEntity = reachingEntityEntry->positionProperty.data();
		//Bail out if the reaching entity hasn't a valid position.
		if (!positionOfReachingEntity.isValid()) {
			return false;
		}

		//If a contained entity tries to touch the domain entity we must check the optional position.
		if (&queriedEntity == &m_entity) {
			if (!positionOnQueriedEntity.isValid()) {
				return false;
			}
			//Try to get the collision objects positions, if there are any. Otherwise use the entities positions. This is because some entities don't have collision objects.
			btVector3 reachingEntityPos = reachingEntityEntry->collisionObject ? reachingEntityEntry->collisionObject->getWorldTransform().getOrigin() : Convert::toBullet(positionOfReachingEntity);

			auto distance = reachingEntityPos.distance(Convert::toBullet(positionOnQueriedEntity));
			distance -= (float)reachingEntityEntry->bbox.boundingSphere().radius();
			return distance <= reach;

		} else {
			//Both the reaching entity and the queried entity must be contained in the domain.
			auto queriedBulletEntryI = m_entries.find(queriedEntity.getIdAsInt());
			if (queriedBulletEntryI != m_entries.end()) {
				auto& queriedBulletEntry = queriedBulletEntryI->second;
				//Bail out if the entity being reached for hasn't a valid position.
				if (!queriedBulletEntry->positionProperty.data().isValid()) {
					return false;
				}
				return isWithinReach(*reachingEntityEntry, *queriedBulletEntry, reach, positionOnQueriedEntity);
			}
		}
	}
	//If either the reaching of queried entity doesn't belong to the domain we won't allow it.
	//The most likely case if the reaching entity not belonging (i.e. reaching into a domain).
	return false;
}

bool PhysicalDomain::isWithinReach(BulletEntry& reacherEntry, BulletEntry& targetEntry, float reach, const WFMath::Point<3>& positionOnQueriedEntity) {


	btVector3 queriedEntityPos;
	//Try to get the collision objects positions, if there are any. Otherwise use the entities positions. This is because some entities don't have collision objects.
	btVector3 reachingEntityPos = reacherEntry.collisionObject ? reacherEntry.collisionObject->getWorldTransform().getOrigin() : Convert::toBullet(reacherEntry.positionProperty.data());

	if (positionOnQueriedEntity.isValid()) {
		//Adjust position on queried entity into parent coord system (to get a global pos)
		queriedEntityPos = Convert::toBullet(
			positionOnQueriedEntity.toParentCoords(targetEntry.positionProperty.data(),
				targetEntry.orientationProperty.data().isValid() ? targetEntry.orientationProperty.data() : WFMath::Quaternion::IDENTITY()));
	} else {
		//Try to get the collision objects positions, if there are any. Otherwise use the entities positions. This is because some entities don't have collision objects.
		queriedEntityPos = targetEntry.collisionObject ? targetEntry.collisionObject->getWorldTransform().getOrigin() : Convert::toBullet(targetEntry.positionProperty.data());
	}

	//Start with the simple case by checking if the centers are close
	//We measure from the edge of one entity to the edge of another.
	if (reachingEntityPos.distance(queriedEntityPos) - reacherEntry.bbox.boundingSphere().radius() - targetEntry.bbox.boundingSphere().radius() <= reach) {
		return true;
	}

	if (targetEntry.collisionObject) {
		//Check how we collide with the target.
		btCollisionWorld::ClosestRayResultCallback rayResultCallback(reachingEntityPos, queriedEntityPos);
		auto extendedQueriedPos = queriedEntityPos + ((queriedEntityPos - reachingEntityPos) * 0.1); //Extend a bit
		btCollisionWorld::rayTestSingle(btTransform(btQuaternion::getIdentity(), reachingEntityPos),
			btTransform(btQuaternion::getIdentity(), extendedQueriedPos),
			targetEntry.collisionObject.get(),
			targetEntry.collisionShape.get(),
			targetEntry.collisionObject->getWorldTransform(),
			rayResultCallback);

		if (!rayResultCallback.hasHit()) {
			return false;
		}

		//Start with the full distance.
		auto distance = reachingEntityPos.distance(rayResultCallback.m_hitPointWorld);

		//We measure from the edge of one entity to the edge of another.
		distance -= (float)reacherEntry.bbox.boundingSphere().radius();

		return distance <= reach;
	}
	return false;
}

std::vector<Domain::CollisionEntry> PhysicalDomain::queryCollision(const WFMath::Ball<3>& sphere) const {
	struct : btCollisionWorld::ContactResultCallback {
		std::map<BulletEntry*, btManifoldPoint> m_entries;

		btScalar addSingleResult(btManifoldPoint& cp,
								 const btCollisionObjectWrapper* colObj0Wrap,
								 int partId0,
								 int index0,
								 const btCollisionObjectWrapper* colObj1Wrap,
								 int partId1,
								 int index1) override {
			if (auto* bulletEntry = static_cast<BulletEntry*>(colObj1Wrap->m_collisionObject->getUserPointer())) {
				m_entries.emplace(bulletEntry, cp);
			}
			return btScalar{1.0};
		}

	} callback;

	btTransform pos(btQuaternion::getIdentity(), Convert::toBullet(sphere.center()));

	btSphereShape shape(static_cast<btScalar>(sphere.radius()));

	btCollisionObject collisionObject;
	collisionObject.setCollisionShape(&shape);
	collisionObject.setWorldTransform(pos);

	m_dynamicsWorld->contactTest(&collisionObject, callback);

	std::vector<Domain::CollisionEntry> result;
	result.reserve(callback.m_entries.size());
	for (auto& entry: callback.m_entries) {
		result.emplace_back(Domain::CollisionEntry{
			.entity = &entry.first->entity,
			.collisionPoint = Convert::toWF<WFMath::Point<3>>(entry.second.getPositionWorldOnA()),
			//                                                   std::abs(entry.second.getDistance())});
			.distance = (float)entry.second.getPositionWorldOnB().distance(pos.getOrigin())
		});
	}
	return result;
}

std::optional<std::function<void()>> PhysicalDomain::observeCloseness(LocatedEntity& reacher, LocatedEntity& target, double reach, std::function<void()> callback) {
	auto reacherEntryI = m_entries.find(reacher.getIdAsInt());
	auto targetEntryI = m_entries.find(target.getIdAsInt());
	if (reacherEntryI != m_entries.end() && targetEntryI != m_entries.end()) {
		auto* reacherEntry = reacherEntryI->second.get();
		auto* targetEntry = targetEntryI->second.get();
		auto obs = new ClosenessObserverEntry{.reacher = reacherEntry, .target = targetEntry, .reach = reach, .callback = callback};
		reacherEntry->closenessObservations.insert(obs);
		targetEntry->closenessObservations.insert(obs);
		m_closenessObservations.emplace(obs, std::unique_ptr<ClosenessObserverEntry>(obs));
		return {
			([this, reacherEntry, targetEntry, obs]() {
				reacherEntry->closenessObservations.erase(obs);
				targetEntry->closenessObservations.erase(obs);
				m_closenessObservations.erase(obs);
			})
		};
	}
	return {};
}

void PhysicalDomain::removed() {
	//Copy to allow modifications to the field during callbacks.
	auto observations = std::move(m_closenessObservations);
	for (auto& entry: observations) {
		entry.second->callback();
	}
}

float PhysicalDomain::calculateVisibilitySphereRadius(const BulletEntry& entry) {
	auto visProp = entry.entity.getPropertyClassFixed<VisibilityDistanceProperty>();

	double radius;
	//If there's a vis_dist prop set, use that directly to set the radius.
	//If not, check the radius of the entity as apply the VISIBILITY_RATIO, unless there's no physical object.
	//In that case apply a default settings based on the entity having a radius of 0.25 meters
	if (visProp) {
		radius = visProp->data();
	} else if (entry.bbox.isValid() && entry.bbox.boundingSphere().radius() > 0) {
		radius = (entry.bbox.boundingSphere().radius() * VISIBILITY_RATIO);
	} else {
		//Handle it as if it was 0.25 meter in radius
		radius = (0.25f * VISIBILITY_RATIO);
	}
	for (size_t i = 0; i < VISIBILITY_DISTANCE_THRESHOLDS.size(); ++i) {
		if (radius < VISIBILITY_DISTANCE_THRESHOLDS[i]) {
			if (i == 0) {
				return (float)VISIBILITY_DISTANCE_THRESHOLDS[0] * VISIBILITY_SCALING_FACTOR;
			} else {
				return (float)VISIBILITY_DISTANCE_THRESHOLDS[i - 1] * VISIBILITY_SCALING_FACTOR;
			}
		}
	}
	//If we get here the visibility is beyond the largest threshold, so return that.
	return (float)VISIBILITY_DISTANCE_THRESHOLDS.back() * VISIBILITY_SCALING_FACTOR;

}
