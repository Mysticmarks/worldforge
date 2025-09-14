// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2008 Alistair Riddoch
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA


#include "StorageManager.h"

#include "EntityBuilder.h"
#include "MindProperty.h"

#include "rules/simulation/LocatedEntity.h"
#include "rules/simulation/Domain.h"
#include "rules/simulation/WorldRouter.h"

#include "common/Database.h"
#include "common/debug.h"
#include "common/Monitors.h"
#include "common/PropertyManager.h"
#include "common/id.h"
#include "common/Variable.h"


#include <Atlas/Objects/Anonymous.h>

#include <wfmath/atlasconv.h>

#include <sigc++/adaptors/bind.h>

#include <unordered_set>
#include "Remotery.h"
#include <fmt/format.h>

using Atlas::Message::MapType;
using Atlas::Message::Element;


static constexpr auto debug_flag = false;

StorageManager::StorageManager(WorldRouter& world,
							   Database& db,
							   EntityBuilder& entityBuilder,
							   PropertyManager<LocatedEntity>& propertyManager) :
		m_world(world),
		m_db(db),
		m_entityBuilder(entityBuilder),
		m_propertyManager(propertyManager),
		m_insertEntityCount(0), m_updateEntityCount(0),
		m_insertPropertyCount(0), m_updatePropertyCount(0),
		m_insertQps(0), m_updateQps(0),
		m_insertQpsNow(0), m_updateQpsNow(0),
		m_insertQpsAvg(0), m_updateQpsAvg(0),
		m_insertQpsIndex(0), m_updateQpsIndex(0),
		m_insertQpsRing(), m_updateQpsRing() {

	world.inserted.connect(sigc::mem_fun(*this,
										 &StorageManager::entityInserted));

	Monitors::instance().watch("storage_entity_inserts",
							   std::make_unique<Variable<int>>(m_insertEntityCount));
	Monitors::instance().watch("storage_entity_updates",
							   std::make_unique<Variable<int>>(m_updateEntityCount));
	Monitors::instance().watch("storage_property_inserts",
							   std::make_unique<Variable<int>>(m_insertPropertyCount));
	Monitors::instance().watch("storage_property_updates",
							   std::make_unique<Variable<int>>(m_updatePropertyCount));

	Monitors::instance().watch(R"(storage_qps{{qtype="inserts",t="1"}})",
							   std::make_unique<Variable<int>>(m_insertQpsNow));
	Monitors::instance().watch(R"(storage_qps{{qtype="updates",t="1"}})",
							   std::make_unique<Variable<int>>(m_updateQpsNow));

	Monitors::instance().watch(R"(storage_qps{{qtype="inserts",t="32"}})",
							   std::make_unique<Variable<int>>(m_insertQpsAvg));
	Monitors::instance().watch(R"(storage_qps{{qtype="updates",t="32"}})",
							   std::make_unique<Variable<int>>(m_updateQpsAvg));

	for (int i = 0; i < 32; ++i) {
		m_insertQpsRing[i] = 0;
		m_updateQpsRing[i] = 0;
	}
}

StorageManager::~StorageManager() = default;

/// \brief Called when a new Entity is inserted in the world
void StorageManager::entityInserted(LocatedEntity& ent) {
	if (ent.hasFlags(entity_ephem)) {
		// This entity is not persisted.
		return;
	}
	if (ent.hasFlags(entity_clean)) {
		// This entity has just been restored from the database, so does
		// not need to be inserted, but will need to be updated.
		// For non-restored entities that have been newly created this
		// signal will be connected later once the initial database insert
		// has been done.
		ent.updated.connect([&]() { entityUpdated(ent); });
		ent.containered.connect([&](const Ref<LocatedEntity>& container) { entityUpdated(ent); });
		return;
	}
	// Queue the entity to be inserted into the persistence tables.
	m_unstoredEntities.emplace_back(&ent);
	ent.addFlags(entity_queued);
}

/// \brief Called when an Entity is modified
void StorageManager::entityUpdated(LocatedEntity& ent) {
	if (ent.isDestroyed()) {
		m_destroyedEntities.push_back(ent.getIdAsInt());
		return;
	}
	// Is it already in the dirty Entities queue?
	// Perhaps we need to modify the semantics of the updated signal
	// so it is only emitted if the entity was not marked as dirty.
	if (ent.hasFlags(entity_queued)) {
		// std::cout << "Already queued " << ent.getId() << std::endl;
		return;
	}
	m_dirtyEntities.emplace_back(&ent);
	// std::cout << "Updated fired " << ent.getId() << std::endl;
	ent.addFlags(entity_queued);
}

void StorageManager::encodeProperty(const PropertyBase& prop, std::string& store) {
	Atlas::Message::MapType map;
	prop.get(map["val"]);
	m_db.encodeObject(map, store);
}

void StorageManager::encodeElement(const Atlas::Message::Element& element, std::string& store) {
	Atlas::Message::MapType map{{"val", element}};
	m_db.encodeObject(map, store);
}

void StorageManager::restorePropertiesRecursively(LocatedEntity& ent) {
	DatabaseResult res = m_db.selectProperties(ent.getIdAsString());

	//Keep track of those properties that have been set on the instance, so we'll know what
	//type properties we should ignore.
	std::unordered_set<std::string> instanceProperties;

	auto I = res.begin();
	auto Iend = res.end();
	for (; I != Iend; ++I) {
		const std::string name = I.column("name");
		if (name.empty()) {
			spdlog::error("No name column in property row for {}",
						  ent.describeEntity());
			continue;
		}
		const std::string val_string = I.column("value");
		if (name.empty()) {
			spdlog::error("No value column in property row for {},{}",
						  ent.describeEntity(), name);
			continue;
		}
		MapType prop_data;
		m_db.decodeMessage(val_string, prop_data);
		auto J = prop_data.find("val");
		if (J == prop_data.end()) {
			spdlog::error("No property value data for {}:{}",
						  ent.describeEntity(), name);
			continue;
		}
		assert(ent.getType() != nullptr);
		auto& val = J->second;

		Element existingVal;
		if (ent.getAttr(name, existingVal) == 0) {
			if (existingVal == val) {
				//If the existing property, either on the instance or the type, is equal to the persisted one just skip it.
				continue;
			}
		}


		auto* prop = ent.modProperty(name, val);
		if (!prop) {
			auto newProp = m_propertyManager.addProperty(name);
			prop = ent.setProperty(name, std::move(newProp));
		}

		//If we get to here the property either doesn't exist, or have a different value than the default or existing property.
		prop->set(val);
		prop->addFlags(prop_flag_persistence_clean | prop_flag_persistence_seen);
		prop->apply(ent);
		ent.propertyApplied(name, *prop);
		instanceProperties.insert(name);
	}

	if (ent.getType()) {
		for (auto& propIter: ent.getType()->defaults()) {
			if (instanceProperties.find(propIter.first) == instanceProperties.end()) {
				auto& prop = propIter.second;
				// If a property is in the class it won't have been installed
				// as setAttrValue() checks
				prop->install(ent, propIter.first);
				// The property will have been applied if it has an overridden
				// value, so we only apply it the value is still default.
				prop->apply(ent);
				ent.propertyApplied(propIter.first, *prop);
			}
		}
	}

	if (ent.m_parent) {
		auto domain = ent.m_parent->getDomain();
		if (domain) {
			domain->addEntity(ent);
		}
	}


	//Now restore all properties of the child entities.
	if (ent.m_contains) {
		//It might be that the contains field gets altered by restoring of children, so we need to operate on a copy.
		auto contains = *ent.m_contains;
		for (auto& childEntity: contains) {
			restorePropertiesRecursively(*childEntity);
		}
	}

//    //We should also send a sight op to the parent entity which owns the entity.
//    //TODO: should this really be necessary or should we rely on other Sight functionality?
//    if (ent.m_parent) {
//        Atlas::Objects::Operation::Sight sight;
//        sight->setTo(ent.m_parent.getId());
//        Atlas::Objects::Entity::Anonymous args;
//        ent.addToEntity(args);
//        sight->setArgs1(args);
//        ent.m_parent.sendWorld(sight);
//    }

}

void StorageManager::insertEntity(LocatedEntity& ent) {


	m_db.insertEntity(ent.getIdAsString(),
					  ent.m_parent ? ent.m_parent->getIdAsString() : "",
					  ent.getType()->name(),
					  ent.getSeq());
	++m_insertEntityCount;
	std::vector<std::tuple<std::string, std::string>> property_tuples;
	const auto& properties = ent.getProperties();
	for (auto& entry: properties) {
		auto& prop = entry.second.property;
		//The property might be empty if there's only modifiers but no property.
		if (!prop) {
			continue;
		}
		if (prop->hasFlags(prop_flag_persistence_ephem)) {
			continue;
		}
		if (entry.second.modifiers.empty()) {
			std::string value;
			encodeProperty(*prop, value);
			property_tuples.emplace_back(entry.first, value);
		} else {
			std::string value;
			encodeElement(entry.second.baseValue, value);
			property_tuples.emplace_back(entry.first, value);
		}
		prop->addFlags(prop_flag_persistence_clean | prop_flag_persistence_seen);
	}
	if (!property_tuples.empty()) {
		m_db.upsertProperties(ent.getIdAsString(), property_tuples);
		++m_insertPropertyCount;
	}
	ent.removeFlags(entity_queued);
	ent.addFlags(entity_clean);
	ent.updated.connect([&]() { entityUpdated(ent); });
	ent.containered.connect([&](const Ref<LocatedEntity>& container) { entityUpdated(ent); });
}

void StorageManager::updateEntity(LocatedEntity& ent) {

	//Under normal circumstances only the top world won't have a parent location.
	if (ent.m_parent) {
		m_db.updateEntity(ent.getIdAsString(),
						  ent.getSeq(),
						  ent.m_parent->getIdAsString());
	} else {
		m_db.updateEntityWithoutLoc(ent.getIdAsString(),
									ent.getSeq());
	}
	++m_updateEntityCount;
	std::vector<std::tuple<std::string, std::string>> tuples;
	auto& properties = ent.getProperties();
	for (const auto& property: properties) {
		auto& prop = property.second.property;
		//There might not be a property if only modifiers are set.
		if (!prop) {
			continue;
		}
		if (prop->hasFlags(prop_flag_persistence_mask)) {
			continue;
		}
                if (property.second.modifiers.empty()) {
                        Element val;
                        prop->get(val);
                        if (val.isNone()) {
                                m_db.scheduleCommand(fmt::format("DELETE FROM properties WHERE id = '{}' AND name = '{}'",
                                                                 ent.getIdAsString(), property.first));
                        } else {
                                std::string value;
                                encodeProperty(*prop, value);
                                tuples.emplace_back(property.first, value);
                        }
                } else {
                        if (property.second.baseValue.isNone()) {
                                m_db.scheduleCommand(fmt::format("DELETE FROM properties WHERE id = '{}' AND name = '{}'",
                                                                 ent.getIdAsString(), property.first));
                        } else {
                                std::string value;
                                encodeElement(property.second.baseValue, value);
                                tuples.emplace_back(property.first, value);
                        }
                }

		// FIXME check if this is new or just modded.
		if (prop->hasFlags(prop_flag_persistence_seen)) {
			++m_updatePropertyCount;
		} else {
			++m_insertPropertyCount;
		}
		prop->addFlags(prop_flag_persistence_clean | prop_flag_persistence_seen);
	}
	if (!tuples.empty()) {
		m_db.upsertProperties(ent.getIdAsString(), tuples);
	}
	ent.addFlags(entity_clean_mask);
}

size_t StorageManager::restoreChildren(LocatedEntity& parent) {
	size_t childCount = 0;
	DatabaseResult res = m_db.selectEntities(parent.getIdAsString());

	// Iterate over res creating entities. Restore children, but don't restore any properties yet.
	auto I = res.begin();
	auto Iend = res.end();
	for (; I != Iend; ++I) {
		RouterId id(I.column("id"));
		auto type = I.column("type");
		//By sending an empty attributes pointer we're telling the builder not to apply any default
		//attributes. We will instead apply all attributes ourselves when we later on restore attributes.
		auto child = m_entityBuilder.newEntity(id, type, {nullptr});
		if (!child) {
			throw std::runtime_error(
					fmt::format("Could not restore entity with id {} of type '{}'"
								", most likely caused by this type missing.",
								id.asString(), type));
		}
		childCount++;

		child->addFlags(entity_clean);
		m_world.addEntity(child, &parent);
		childCount += restoreChildren(*child);
	}
	return childCount;
}

void StorageManager::tick() {
	rmt_ScopedCPUSample(StorageManager_tick, 0)
	int inserts = 0, updates = 0;
	int old_insert_queries = m_insertEntityCount + m_insertPropertyCount;
	int old_update_queries = m_updateEntityCount + m_updatePropertyCount;

	while (!m_destroyedEntities.empty()) {
		long id = m_destroyedEntities.front();
		m_db.dropEntity(id);
		m_destroyedEntities.pop_front();
	}

	while (!m_unstoredEntities.empty()) {
		auto& ent = m_unstoredEntities.front();
		if (ent && !ent->isDestroyed()) {
			cy_debug_print("storing " << ent->getIdAsString())
			insertEntity(*ent);
			++inserts;
		} else {
			cy_debug_print("deleted")
		}
		m_unstoredEntities.pop_front();
	}

	while (!m_dirtyEntities.empty()) {
		if (m_db.queryQueueSize() > 200) {
			cy_debug_print("Too many")
			break;
		}
		auto& ent = m_dirtyEntities.front();
		if (ent) {
			if ((ent->flags().m_flags & entity_clean_mask) != entity_clean_mask) {
				cy_debug_print("updating " << ent->getIdAsString())
				updateEntity(*ent);
				++updates;
			}
			if (ent->hasFlags(entity_dirty_thoughts)) {
				cy_debug_print("updating thoughts " << ent->getIdAsString())
				++updates;
			}
			ent->removeFlags(entity_queued);
		} else {
			cy_debug_print("deleted")
		}
		m_dirtyEntities.pop_front();
	}

	if (inserts > 0 || updates > 0) {
		cy_debug_print("I: " << inserts << " U: " << updates)
	}
	int insert_queries = m_insertEntityCount + m_insertPropertyCount
						 - old_insert_queries;

	if (++m_insertQpsIndex >= 32) {
		m_insertQpsIndex = 0;
	}
	m_insertQps -= m_insertQpsRing[m_insertQpsIndex];
	m_insertQps += insert_queries;
	m_insertQpsRing[m_insertQpsIndex] = insert_queries;
	m_insertQpsAvg = m_insertQps / 32;
	m_insertQpsNow = insert_queries;

	cy_debug(if (insert_queries) {
		std::cout << "Ins: " << insert_queries << ", " << m_insertQps / 32
				  << std::endl;
	})

	int update_queries = m_updateEntityCount + m_updatePropertyCount
						 - old_update_queries;

	if (++m_updateQpsIndex >= 32) {
		m_updateQpsIndex = 0;
	}
	m_updateQps -= m_updateQpsRing[m_updateQpsIndex];
	m_updateQps += update_queries;
	m_updateQpsRing[m_updateQpsIndex] = update_queries;
	m_updateQpsAvg = m_updateQps / 32;
	m_updateQpsNow = update_queries;

	cy_debug(if (update_queries) {
		std::cout << "Ups: " << update_queries << ", " << m_updateQps / 32
				  << std::endl;
	})
}

int StorageManager::initWorld(const Ref<LocatedEntity>& ent) {
	auto entPtr = ent.get();
	ent->updated.connect([this, entPtr]() { entityUpdated(*entPtr); });
	ent->addFlags(entity_clean);
	// FIXME queue it so the initial state gets persisted.
	return 0;
}

int StorageManager::restoreWorld(const Ref<LocatedEntity>& ent) {

	auto entitiesCount = m_db.entitiesCount();
	if (entitiesCount == 0) {
		spdlog::info("No existing entities exist, so we won't restore any world.");
	} else {
		spdlog::info("Starting restoring world from storage, need to restore {} entities.", entitiesCount);

		//The order here is important. We want to restore the children before we restore the properties.
		//The reason for this is that some properties (such as "attached_*") refer to child entities; if
		//the child isn't present when the property is installed there will be issues.
		//We do this by first restoring the children, without any properties, and the assigning the properties to
		//all entities in order.
		auto childCount = restoreChildren(*ent);
		spdlog::debug("Completed initial restoration of {} entities, will now populate with properties.", childCount);

		restorePropertiesRecursively(*ent);

		spdlog::info("Completed restoring world from storage, {} entities restored.", childCount);
	}
	return 0;
}

int StorageManager::shutdown(bool&, const std::map<long, Ref<LocatedEntity>>&) {
	tick();
	m_db.blockUntilAllQueriesComplete();
	return 0;
}

