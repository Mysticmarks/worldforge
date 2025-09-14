#pragma once

#include "rules/ai/MemEntity.h"
#include <wfmath/axisbox.h>
#include <wfmath/rotbox.h>
#include <wfmath/point.h>
#include <wfmath/vector.h>
#include <wfmath/quaternion.h>

#include <rules/BBoxProperty.h>
#include <rules/PhysicalProperties.h>
#include <rules/ScaleProperty.h>

#include <unordered_map>
#include <set>
#include <map>
#include <vector>
#include <chrono>

struct EntityConnections {
        bool isMoving;
        bool isIgnored;
};

template<typename T>
struct TimestampedProperty {
        T data;
        std::chrono::milliseconds timestamp = std::chrono::milliseconds{-1};
};

struct EntityEntry {
        long entityId;
        int numberOfObservers;

        TimestampedProperty<WFMath::Point<3>> pos;
        TimestampedProperty<WFMath::Vector<3>> velocity;

        TimestampedProperty<WFMath::Quaternion> orientation;
        TimestampedProperty<WFMath::Vector<3>> angular;

        TimestampedProperty<WFMath::AxisBox<3>> bbox;
        TimestampedProperty<WFMath::Vector<3>> scale;
        WFMath::AxisBox<3> scaledBbox;

        bool isActorOwned;
        bool isMoving;
        bool isIgnored;
        bool isSolid;
};

class EntityTracker {
public:
        EntityEntry& addEntity(const MemEntity& observer, const MemEntity& entity, bool isDynamic);
        void removeEntity(const MemEntity& observer, const MemEntity& entity);
        void updateEntity(const MemEntity& observer, const MemEntity& entity, const Atlas::Objects::Entity::RootEntity& ent);

        bool projectPosition(long entityId, WFMath::Point<3>& pos, std::chrono::milliseconds currentServerTimestamp) const;
        WFMath::Point<3> projectPosition(long entityId, std::chrono::milliseconds currentServerTimestamp) const;

        const std::unordered_map<long, std::unique_ptr<EntityEntry>>& getObservedEntities() const { return mObservedEntities; }
        std::unordered_map<long, std::unique_ptr<EntityEntry>>& getObservedEntities() { return mObservedEntities; }
        const std::set<const EntityEntry*>& getMovingEntities() const { return mMovingEntities; }
        std::set<const EntityEntry*>& getMovingEntities() { return mMovingEntities; }
        const std::map<const EntityEntry*, WFMath::RotBox<2>>& getEntityAreas() const { return mEntityAreas; }
        std::map<const EntityEntry*, WFMath::RotBox<2>>& getEntityAreas() { return mEntityAreas; }

        WFMath::RotBox<2> buildEntityAreas(const EntityEntry& entity);
        void findEntityAreas(const WFMath::AxisBox<2>& extent, std::vector<WFMath::RotBox<2>>& areas);

        bool processEntityUpdate(EntityEntry& entry, const MemEntity& entity, const Atlas::Objects::Entity::RootEntity* ent, std::chrono::milliseconds timestamp);

private:
        std::unordered_map<long, std::unique_ptr<EntityEntry>> mObservedEntities;
        std::set<const EntityEntry*> mMovingEntities;
        std::map<const EntityEntry*, WFMath::RotBox<2>> mEntityAreas;
};


inline EntityEntry& EntityTracker::addEntity(const MemEntity& observer, const MemEntity& entity, bool isDynamic) {
        auto I = mObservedEntities.find(entity.getIdAsInt());
        if (I == mObservedEntities.end()) {
                std::unique_ptr<EntityEntry> entityEntry(new EntityEntry());
                entityEntry->entityId = entity.getIdAsInt();
                entityEntry->numberOfObservers = 1;
                auto bboxProp = entity.getPropertyClassFixed<BBoxProperty<MemEntity>>();
                entityEntry->isIgnored = !bboxProp || !bboxProp->data().isValid();
                entityEntry->isMoving = isDynamic;
                entityEntry->isActorOwned = false;
                auto solidPropery = entity.getPropertyClassFixed<SolidProperty<MemEntity>>();
                entityEntry->isSolid = !solidPropery || solidPropery->isTrue();
                if (isDynamic) {
                        mMovingEntities.insert(entityEntry.get());
                }
                I = mObservedEntities.emplace(entity.getIdAsInt(), std::move(entityEntry)).first;
        } else {
                I->second->numberOfObservers++;
        }
        if (I->first == observer.getIdAsInt()) {
                I->second->isActorOwned = true;
        }
        return *I->second;
}

inline void EntityTracker::removeEntity(const MemEntity& observer, const MemEntity& entity) {
        auto I = mObservedEntities.find(entity.getIdAsInt());
        if (I != mObservedEntities.end()) {
                auto& entityEntry = I->second;
                entityEntry->numberOfObservers--;
                if (entityEntry->numberOfObservers == 0) {
                        if (entityEntry->isMoving) {
                                mMovingEntities.erase(entityEntry.get());
                        }
                        auto areasI = mEntityAreas.find(entityEntry.get());
                        if (areasI != mEntityAreas.end()) {
                                mEntityAreas.erase(areasI);
                        }
                        mObservedEntities.erase(I);
                } else {
                        if (observer.getIdAsInt() == entity.getIdAsInt()) {
                                entityEntry->isActorOwned = false;
                        }
                }
        }
}

inline void EntityTracker::updateEntity(const MemEntity& observer, const MemEntity& entity, const Atlas::Objects::Entity::RootEntity& ent) {
        auto I = mObservedEntities.find(entity.getIdAsInt());
        if (I != mObservedEntities.end()) {
                processEntityUpdate(*I->second, entity, &ent, entity.m_lastUpdated);
        } else {
                addEntity(observer, entity, true);
        }
}

inline bool EntityTracker::projectPosition(long entityId, WFMath::Point<3>& pos, std::chrono::milliseconds currentServerTimestamp) const {
        auto entityI = mObservedEntities.find(entityId);
        if (entityI != mObservedEntities.end()) {
                auto& entry = entityI->second;
                pos = projectPosition(entityId, currentServerTimestamp);
                return true;
        }
        return false;
}

inline WFMath::Point<3> EntityTracker::projectPosition(long entityId, std::chrono::milliseconds currentServerTimestamp) const {
        WFMath::Point<3> pos;
        auto entityI = mObservedEntities.find(entityId);
        if (entityI != mObservedEntities.end()) {
                auto& entry = entityI->second;
                pos = entry->pos.data;
                if (entry->velocity.data.isValid()) {
                        auto time_diff = std::chrono::duration_cast<std::chrono::duration<float>>(currentServerTimestamp - entry->velocity.timestamp).count();
                        pos += (entry->velocity.data * time_diff);
                }
        }
        return pos;
}

inline WFMath::RotBox<2> EntityTracker::buildEntityAreas(const EntityEntry& entity) {
        if (!entity.scaledBbox.isValid() || !entity.pos.data.isValid()) {
                return {};
        }
        auto low = entity.scaledBbox.lowCorner();
        auto high = entity.scaledBbox.highCorner();
        WFMath::Point<3> c = entity.pos.data;
        WFMath::RotBox<2> box;
        box.highCorner() = {c.x() + high.x(), c.z() + high.z()};
        box.lowCorner() = {c.x() + low.x(), c.z() + low.z()};
        return box;
}

inline void EntityTracker::findEntityAreas(const WFMath::AxisBox<2>& extent, std::vector<WFMath::RotBox<2>>& areas) {
        for (auto& entry: mEntityAreas) {
                if (WFMath::Intersects(entry.second.boundingBox(), extent)) {
                        areas.push_back(entry.second);
                }
        }
}

inline bool EntityTracker::processEntityUpdate(EntityEntry& entityEntry, const MemEntity& entity, const Atlas::Objects::Entity::RootEntity* ent, std::chrono::milliseconds timestamp) {
        bool hasNewPosition = false;
        bool hasNewBbox = false;
        if (!ent || ent->hasAttr("pos")) {
                if (timestamp > entityEntry.pos.timestamp) {
                        if (auto prop = entity.getPropertyClassFixed<PositionProperty<MemEntity>>()) {
                                entityEntry.pos.data = prop->data();
                                entityEntry.pos.timestamp = timestamp;
                                hasNewPosition = true;
                        }
                }
        }
        if (!ent || ent->hasAttrFlag(Atlas::Objects::Entity::VELOCITY_FLAG)) {
                if (timestamp > entityEntry.velocity.timestamp) {
                        if (auto prop = entity.getPropertyClassFixed<VelocityProperty<MemEntity>>()) {
                                entityEntry.velocity.data = prop->data();
                                entityEntry.velocity.timestamp = timestamp;
                        }
                }
        }
        if (!ent || ent->hasAttr("orientation")) {
                if (timestamp > entityEntry.orientation.timestamp) {
                        if (auto prop = entity.getPropertyClassFixed<OrientationProperty<MemEntity>>()) {
                                entityEntry.orientation.data = prop->data();
                                entityEntry.orientation.timestamp = timestamp;
                                hasNewPosition = true;
                        }
                }
        }
        if (!ent || ent->hasAttr("angular")) {
                if (timestamp > entityEntry.angular.timestamp) {
                        if (auto prop = entity.getPropertyClassFixed<AngularVelocityProperty<MemEntity>>()) {
                                entityEntry.angular.data = prop->data();
                                entityEntry.angular.timestamp = timestamp;
                        }
                }
        }
        if (!ent || ent->hasAttr("bbox")) {
                if (timestamp > entityEntry.bbox.timestamp) {
                                if (auto prop = entity.getPropertyClassFixed<BBoxProperty<MemEntity>>()) {
                                        entityEntry.bbox.data = prop->data();
                                        entityEntry.bbox.timestamp = timestamp;
                                        hasNewBbox = true;
                                }
                }
        }
        if (!ent || ent->hasAttr("scale")) {
                if (timestamp > entityEntry.scale.timestamp) {
                        if (auto prop = entity.getPropertyClassFixed<ScaleProperty<MemEntity>>()) {
                                entityEntry.scale.data = prop->data();
                                entityEntry.scale.timestamp = timestamp;
                                hasNewBbox = true;
                        }
                }
        }
        if (hasNewBbox) {
                if (entityEntry.bbox.data.isValid()) {
                        if (entityEntry.scale.data.isValid()) {
                                entityEntry.scaledBbox = ScaleProperty<MemEntity>::scaledBbox(entityEntry.bbox.data, entityEntry.scale.data);
                        } else {
                                entityEntry.scaledBbox = entityEntry.bbox.data;
                        }
                }
        }
        return hasNewBbox || hasNewPosition;
}

