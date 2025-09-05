// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2005 Alistair Riddoch
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


#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef DEBUG
#define DEBUG
#endif

#include "../TestBase.h"
#include "../TestWorld.h"

#include "pythonbase/Python_API.h"

#include "server/Ruleset.h"
#include "server/EntityBuilder.h"
#include "server/EntityFactory_impl.h"
#include "server/ArchetypeFactory.h"

#include "rules/simulation/Inheritance.h"
#include "common/TypeNode_impl.h"
#include "../TestPropertyManager.h"
#include "common/Monitors.h"

#include <Atlas/Objects/Anonymous.h>
#include <Atlas/Objects/Operation.h>

#include <cassert>
#include <rules/simulation/LocatedEntity.h>

using Atlas::Message::MapType;
using Atlas::Objects::Entity::Anonymous;
using Atlas::Objects::Entity::RootEntity;
using Atlas::Objects::Root;

Atlas::Objects::Factories factories;


class ExposedEntityBuilder : public EntityBuilder {
public:
	explicit ExposedEntityBuilder() : EntityBuilder() {
	}

	const std::map<std::string, std::unique_ptr<EntityKit>>& factoryDict() const { return m_entityFactories; }

};

/// Integrations to try: Installation of all types of rules into
/// builders and factories. Creation of entity has all the right things.
/// Installation of rules via Admin. Persistence calls from Ruleset.
struct Rulesetintegration : public Cyphesis::TestBase {
	Ref<LocatedEntity> m_entity;
	TestWorld* m_test_world;
	ExposedEntityBuilder* m_entity_builder;

	Rulesetintegration() {
		ADD_TEST(Rulesetintegration::test_init);
		ADD_TEST(Rulesetintegration::test_sequence);
	}


	void setup() {
		m_inheritance = new Inheritance();
		m_entity = new LocatedEntity(0);
		m_test_world = new TestWorld(m_entity);
		m_entity_builder = new ExposedEntityBuilder();
	}

	void teardown() {
		delete m_entity_builder;
		delete m_test_world;
		delete m_inheritance;
	}


	void test_init() {
		assert(!Ruleset::hasInstance());

		boost::asio::io_context io_context;
		TestPropertyManager<LocatedEntity> propertyManager;
		{
			Ruleset ruleset(*m_entity_builder, io_context, propertyManager);

			assert(Ruleset::hasInstance());

		}
		assert(!Ruleset::hasInstance());
	}

	void test_sequence() {
		{
                        Atlas::Message::Element val;
                        Ref<LocatedEntity> existing_custom_type;
                        Ref<LocatedEntity> existing_custom_inherited;

			// Instance of Ruleset with all protected methods exposed
			// for testing
			boost::asio::io_context io_context;
			TestPropertyManager<LocatedEntity> propertyManager;
			Ruleset test_ruleset(*m_entity_builder, io_context, propertyManager);


			{
				auto decl = composeDeclaration("thing", "game_entity", {});
				std::map<const TypeNode<LocatedEntity>*, TypeNode<LocatedEntity>::PropertiesUpdate> changes;
				test_ruleset.installItem(decl->getId(), decl, changes);
			}

			// Attributes for test entities being created
			Anonymous attributes;

			// Create an entity which is an instance of one of the core classes
			auto test_ent = m_entity_builder->newEntity(1,
														"thing",
														attributes);
			assert(test_ent);

			// Check that creating an entity of a type we know we have not yet
			// installed results in a null pointer.
			assert(!m_entity_builder->newEntity(1, "custom_type", attributes));

			// Set up a type description for a new type, and install it.
			{
				Root custom_type_description;
				MapType attrs;
				MapType test_custom_type_attr;
				test_custom_type_attr["default"] = "test_value";
				attrs["test_custom_type_attr"] = test_custom_type_attr;
				custom_type_description->setAttr("attributes", attrs);
				custom_type_description->setId("custom_type");
				custom_type_description->setParent("thing");
				custom_type_description->setObjtype("class");

				int ret = test_ruleset.installRule("custom_type", "custom",
												   custom_type_description);

				assert(ret == 0);
			}

			// Check that the factory dictionary now contains a factory for
			// the custom type we just installed.
			EntityFactoryBase* custom_type_factory = dynamic_cast<EntityFactoryBase*>(m_entity_builder->getClassFactory("custom_type"));
			assert(custom_type_factory != 0);
			assert(custom_type_factory->m_type != 0);
			assert(custom_type_factory->m_type ==
				   Inheritance::instance().getType("custom_type"));

			const Root& check_class = Inheritance::instance().getClass("custom_type", Visibility::PRIVATE);
			assert(check_class.isValid());
			assert(check_class->getId() == "custom_type");
			assert(check_class->getParent() == "thing");

			// Check the factory has the attributes we described on the custom
			// type.
			auto J = custom_type_factory->m_attributes.find("test_custom_type_attr");
			assert(J != custom_type_factory->m_attributes.end());
			assert(J->second.isString());
			assert(J->second.String() == "test_value");

			auto JClass = custom_type_factory->m_classAttributes.find("test_custom_type_attr");
			assert(JClass != custom_type_factory->m_classAttributes.end());
			assert(JClass->second.defaultValue.isString());
			assert(JClass->second.defaultValue.String() == "test_value");

			TypeNode<LocatedEntity>* custom_type_node = custom_type_factory->m_type;
			auto K = custom_type_node->defaults().find("test_custom_type_attr");
			assert(K != custom_type_node->defaults().end());
			Atlas::Message::Element custom_type_val;
			assert(K->second->get(custom_type_val) == 0);
			assert(custom_type_val == "test_value");

			// Create an instance of our custom type, ensuring that it works.
			test_ent = m_entity_builder->newEntity(1, "custom_type", attributes);
			assert(test_ent);

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Check the custom type has the attribute described when the
			// custom type was installed.
			assert(test_ent->getAttr("test_custom_type_attr", val) == 0);
			assert(val.isString());
			assert(val.String() == "test_value");

			// Check that creating an entity of a type we know we have not yet
			// installed results in a null pointer.
			assert(!m_entity_builder->newEntity(1, "custom_inherited_type", attributes));

			// Set up a type description for a second new type which inherits
			// from the first, and install it.
			{
				Root custom_inherited_type_description;
				MapType attrs;
				MapType test_custom_type_attr;
				test_custom_type_attr["default"] = "test_inherited_value";
				attrs["test_custom_inherited_type_attr"] = test_custom_type_attr;
				custom_inherited_type_description->setAttr("attributes", attrs);
				custom_inherited_type_description->setId("custom_inherited_type");
				custom_inherited_type_description->setParent("custom_type");
				custom_inherited_type_description->setObjtype("class");

				std::string dependent, reason;
				int ret = test_ruleset.installRule("custom_inherited_type",
												   "custom",
												   custom_inherited_type_description);

				assert(ret == 0);
				assert(dependent.empty());
				assert(reason.empty());
			}

			// Check that the factory dictionary does contain the factory for
			// the second newly installed type
			EntityFactoryBase* custom_inherited_type_factory = dynamic_cast<EntityFactoryBase*>(m_entity_builder->getClassFactory("custom_inherited_type"));
			assert(custom_inherited_type_factory != 0);

			// Check that the factory has inherited the attributes from the
			// first custom type
			J = custom_inherited_type_factory->m_attributes.find("test_custom_type_attr");
			assert(J != custom_inherited_type_factory->m_attributes.end());
			assert(J->second.isString());
			assert(J->second.String() == "test_value");

			// Check that the factory has the attributes specified when installing
			// it
			J = custom_inherited_type_factory->m_attributes.find("test_custom_inherited_type_attr");
			assert(J != custom_inherited_type_factory->m_attributes.end());
			assert(J->second.isString());
			assert(J->second.String() == "test_inherited_value");

			// Creat an instance of the second custom type, ensuring it works.
			test_ent = m_entity_builder->newEntity(1, "custom_inherited_type", attributes);
			assert(test_ent);

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Check the instance of the second custom type has the attribute
			// described when the first custom type was installed.
			assert(test_ent->getAttr("test_custom_type_attr", val) == 0);
			assert(val.isString());
			assert(val.String() == "test_value");

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Check the custom type has the attribute described when the
			// second custom type was installed
			assert(test_ent->getAttr("test_custom_inherited_type_attr", val) == 0);
			assert(val.isString());
			assert(val.String() == "test_inherited_value");

			// FIXME TODO Modify a type, and ensure attribute propagate to inherited types.

			// Make sure than attempting to modify a non-existent type fails
			{
				Anonymous nonexistant_description;
				MapType attrs;
				MapType test_custom_type_attr;

				test_custom_type_attr["default"] = "no_value";
				attrs["no_custom_type_attr"] = test_custom_type_attr;

				nonexistant_description->setId("nonexistant");
				nonexistant_description->setAttr("attributes", attrs);

				int ret = test_ruleset.modifyRule("nonexistant",
												  nonexistant_description);

				assert(ret != 0);
			}

			// Modify the second custom type removing its custom attribute
			{
				Anonymous new_custom_inherited_type_description;
				new_custom_inherited_type_description->setObjtype("class");
				new_custom_inherited_type_description->setId("custom_inherited_type");
				new_custom_inherited_type_description->setAttr("attributes", MapType());

				// No parent
				int ret = test_ruleset.modifyRule("custom_inherited_type",
												  new_custom_inherited_type_description);
				assert(ret != 0);

				// empty parent
				new_custom_inherited_type_description->setParent("");

				ret = test_ruleset.modifyRule("custom_inherited_type",
											  new_custom_inherited_type_description);
				assert(ret != 0);

				// wrong parent
				new_custom_inherited_type_description->setParent("wrong_parent");

				ret = test_ruleset.modifyRule("custom_inherited_type",
											  new_custom_inherited_type_description);
				assert(ret != 0);

				new_custom_inherited_type_description->setParent("custom_type");

				ret = test_ruleset.modifyRule("custom_inherited_type",
											  new_custom_inherited_type_description);

				assert(ret == 0);
			}

			// Check that the factory dictionary does contain the factory for
			// the second newly installed type
			custom_inherited_type_factory = dynamic_cast<EntityFactoryBase*>(m_entity_builder->getClassFactory("custom_inherited_type"));
			assert(custom_inherited_type_factory != 0);

			// Check that the factory has inherited the attributes from the
			// first custom type
			J = custom_inherited_type_factory->m_attributes.find("test_custom_type_attr");
			assert(J != custom_inherited_type_factory->m_attributes.end());
			assert(J->second.isString());
			assert(J->second.String() == "test_value");

			// Check that the factory no longer has the attributes we removed
			J = custom_inherited_type_factory->m_attributes.find("test_custom_inherited_type_attr");
			assert(J == custom_inherited_type_factory->m_attributes.end());

			// Creat an instance of the second custom type, ensuring it works.
			test_ent = m_entity_builder->newEntity(1, "custom_inherited_type", attributes);
			assert(test_ent);

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Make sure test nonexistant attribute isn't present
			assert(test_ent->getAttr("nonexistant", val) != 0);
			// Make sure nonexistant attribute isn't present
			assert(test_ent->getAttr("nonexistant_attribute", val) != 0);

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Check the instance of the second custom type has the attribute
			// described when the first custom type was installed.
			assert(test_ent->getAttr("test_custom_type_attr", val) == 0);
			assert(val.isString());
			assert(val.String() == "test_value");

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Check the custom type has the attribute described when the
			// second custom type was installed
			assert(test_ent->getAttr("test_custom_inherited_type_attr", val) != 0);

			// Modify the first custom type removing its custom attribute
			{
				Anonymous new_custom_type_description;
				new_custom_type_description->setObjtype("class");
				new_custom_type_description->setId("custom_type");
				new_custom_type_description->setAttr("attributes", MapType());
				new_custom_type_description->setParent("thing");

				int ret = test_ruleset.modifyRule("custom_type", new_custom_type_description);

				assert(ret == 0);
			}

			// Check that the factory dictionary now contains a factory for
			// the custom type we just installed.
			custom_type_factory = dynamic_cast<EntityFactoryBase*>(m_entity_builder->getClassFactory("custom_type"));
			assert(custom_type_factory != 0);

			// Check the factory has the attributes we described on the custom
			// type.
			J = custom_type_factory->m_attributes.find("test_custom_type_attr");
			assert(J == custom_type_factory->m_attributes.end());

			// Create an instance of our custom type, ensuring that it works.
			test_ent = m_entity_builder->newEntity(1, "custom_type", attributes);
			assert(test_ent);

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

                        // Check the custom type no longer has the custom attribute
                        assert(test_ent->getAttr("test_custom_type_attr", val) != 0);
                        existing_custom_type = test_ent;

			// Check that the factory dictionary does contain the factory for
			// the second newly installed type
			custom_inherited_type_factory = dynamic_cast<EntityFactoryBase*>(m_entity_builder->getClassFactory("custom_inherited_type"));
			assert(custom_inherited_type_factory != 0);

			// Check that the factory no longer has inherited the attributes
			// from the first custom type which we removed
			J = custom_inherited_type_factory->m_attributes.find("test_custom_type_attr");
			assert(J == custom_inherited_type_factory->m_attributes.end());

			// Check that the factory no longer has the attributes we removed
			J = custom_inherited_type_factory->m_attributes.find("test_custom_inherited_type_attr");
			assert(J == custom_inherited_type_factory->m_attributes.end());

			// Creat an instance of the second custom type, ensuring it works.
			test_ent = m_entity_builder->newEntity(1, "custom_inherited_type", attributes);
			assert(test_ent);

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Make sure test nonexistant attribute isn't present
			assert(test_ent->getAttr("nonexistant", val) != 0);
			// Make sure nonexistant attribute isn't present
			assert(test_ent->getAttr("nonexistant_attribute", val) != 0);

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Check the instance of the second custom type has the attribute
			// described when the first custom type was installed.
			assert(test_ent->getAttr("test_custom_type_attr", val) != 0);

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

                        // Check the custom type has the attribute described when the
                        // second custom type was installed
                        assert(test_ent->getAttr("test_custom_inherited_type_attr", val) != 0);
                        existing_custom_inherited = test_ent;

                        // Add more custom attributes to the first type
                        {
                                Anonymous new_custom_type_description;
				new_custom_type_description->setObjtype("class");
				MapType attrs;
				MapType test_custom_type_attr;

				test_custom_type_attr["default"] = "test_value";
				attrs["test_custom_type_attr"] = test_custom_type_attr;

				MapType new_custom_type_attr;

				new_custom_type_attr["default"] = "new_value";
				attrs["new_custom_type_attr"] = new_custom_type_attr;

				new_custom_type_description->setId("custom_type");
				new_custom_type_description->setAttr("attributes", attrs);
				new_custom_type_description->setParent("thing");

				int ret = test_ruleset.modifyRule("custom_type", new_custom_type_description);

				assert(ret == 0);

			}

			// Check that the factory dictionary now contains a factory for
			// the custom type we just installed.
			custom_type_factory = dynamic_cast<EntityFactoryBase*>(m_entity_builder->getClassFactory("custom_type"));

			// Check the factory has the attributes we described on the custom
			// type.
			J = custom_type_factory->m_attributes.find("test_custom_type_attr");
			assert(J != custom_type_factory->m_attributes.end());
			assert(J->second.isString());
			assert(J->second.String() == "test_value");

			J = custom_type_factory->m_attributes.find("new_custom_type_attr");
			assert(J != custom_type_factory->m_attributes.end());
			assert(J->second.isString());
			assert(J->second.String() == "new_value");

			// Create an instance of our custom type, ensuring that it works.
			test_ent = m_entity_builder->newEntity(1, "custom_type", attributes);
			assert(test_ent);

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Check the custom type now has the custom attributes
			assert(test_ent->getAttr("test_custom_type_attr", val) == 0);
			assert(val.isString());
			assert(val.String() == "test_value");

                        assert(test_ent->getAttr("new_custom_type_attr", val) == 0);
                        assert(val.isString());
                        assert(val.String() == "new_value");

                        // Existing instances should also pick up the updated attributes.
                        val = Atlas::Message::Element();
                        assert(existing_custom_type->getAttr("test_custom_type_attr", val) == 0);
                        assert(val.isString());
                        assert(val.String() == "test_value");

                        val = Atlas::Message::Element();
                        assert(existing_custom_type->getAttr("new_custom_type_attr", val) == 0);
                        assert(val.isString());
                        assert(val.String() == "new_value");

                        val = Atlas::Message::Element();
                        assert(existing_custom_inherited->getAttr("test_custom_type_attr", val) == 0);
                        assert(val.isString());
                        assert(val.String() == "test_value");

                        val = Atlas::Message::Element();
                        assert(existing_custom_inherited->getAttr("new_custom_type_attr", val) == 0);
                        assert(val.isString());
                        assert(val.String() == "new_value");

                        // Check that the factory dictionary does contain the factory for
                        // the second newly installed type
                        custom_inherited_type_factory = dynamic_cast<EntityFactoryBase*>(m_entity_builder->getClassFactory("custom_inherited_type"));
                        assert(custom_inherited_type_factory != 0);

			// Check that the factory now has inherited the attributes
			// from the first custom type
			J = custom_inherited_type_factory->m_attributes.find("test_custom_type_attr");
			assert(J != custom_inherited_type_factory->m_attributes.end());
			assert(J->second.isString());
			assert(J->second.String() == "test_value");

			J = custom_inherited_type_factory->m_attributes.find("new_custom_type_attr");
			assert(J != custom_inherited_type_factory->m_attributes.end());
			assert(J->second.isString());
			assert(J->second.String() == "new_value");

			// Check that the factory no longer has the attributes we removed
			J = custom_inherited_type_factory->m_attributes.find("test_custom_inherited_type_attr");
			assert(J == custom_inherited_type_factory->m_attributes.end());

			// Creat an instance of the second custom type, ensuring it works.
			test_ent = m_entity_builder->newEntity(1, "custom_inherited_type", attributes);
			assert(test_ent);

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Make sure test nonexistant attribute isn't present
			assert(test_ent->getAttr("nonexistant", val) != 0);
			// Make sure nonexistant attribute isn't present
			assert(test_ent->getAttr("nonexistant_attribute", val) != 0);

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Check the instance of the second custom type has the attribute
			// described when the first custom type was installed.
			assert(test_ent->getAttr("test_custom_type_attr", val) == 0);
			assert(val.isString());
			assert(val.String() == "test_value");

			assert(test_ent->getAttr("new_custom_type_attr", val) == 0);
			assert(val.isString());
			assert(val.String() == "new_value");

			// Reset val.
			val = Atlas::Message::Element();
			assert(val.isNone());

			// Check the custom type no longer has the attribute described when the
			// second custom type was installed
			assert(test_ent->getAttr("test_custom_inherited_type_attr", val) != 0);

		}
	}

	Atlas::Objects::Root composeDeclaration(std::string class_name, std::string parent, Atlas::Message::MapType rawAttributes) {

		Atlas::Objects::Root decl;
		decl->setObjtype("class");
		decl->setId(class_name);
		decl->setParent(parent);

		Atlas::Message::MapType composed;
		for (const auto& entry: rawAttributes) {
			composed[entry.first] = Atlas::Message::MapType{
					{"default", entry.second}
			};
		}

		decl->setAttr("attributes", composed);
		return decl;
	}

	Inheritance* m_inheritance;
};


int main() {
	Monitors m;
	// init_python_api("6525a56d-7139-4016-8c1c-c2e77ab50039");

	Rulesetintegration t;

	return t.run();
}



// stubs

#include "server/Connection.h"

#include "server/Account.h"

#include "rules/python/PythonScriptFactory_impl.h"

#include "rules/python/CyPy_Location_impl.h"
#include "rules/simulation/python/CyPy_LocatedEntity_impl.h"


sigc::signal<void()> python_reload_scripts;
