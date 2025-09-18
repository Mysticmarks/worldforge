// Cyphesis Online RPG Server and AI Engine
// Copyright (C) 2024
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

#include "../TestBaseWithContext.h"
#include "../TestWorld.h"

#include "rules/simulation/PhysicalDomain.h"
#include "rules/simulation/PropelProperty.h"
#include "rules/simulation/ModeProperty.h"
#include "rules/PhysicalProperties.h"
#include "rules/Location_impl.h"
#include "rules/BBoxProperty_impl.h"
#include "common/Property_impl.h"
#include "common/TypeNode_impl.h"

#include <Atlas/Objects/Operation.h>
#include <chrono>

using namespace std::chrono_literals;

class TestPhysicalDomain : public PhysicalDomain {
public:
        explicit TestPhysicalDomain(LocatedEntity& entity) : PhysicalDomain(entity) {}

        btRigidBody* getRigidBody(long id) {
                auto it = m_entries.find(id);
                if (it == m_entries.end()) {
                        return nullptr;
                }
                return btRigidBody::upcast(it->second->collisionObject.get());
        }
};

struct SpeedTestContext {
        long idCounter = 0;

        long newId() {
                return ++idCounter;
        }
};

struct PhysicalDomainSpeedTest : public Cyphesis::TestBaseWithContext<SpeedTestContext> {
        PhysicalDomainSpeedTest() {
                ADD_TEST(PhysicalDomainSpeedTest::test_ground_modifier);
                ADD_TEST(PhysicalDomainSpeedTest::test_water_modifier);
                ADD_TEST(PhysicalDomainSpeedTest::test_air_modifier);
        }

        static void setupRootEntity(LocatedEntity& rootEntity) {
                rootEntity.incRef();
                rootEntity.requirePropertyClassFixed<PositionProperty<LocatedEntity>>().data() = WFMath::Point<3>::ZERO();
                rootEntity.requirePropertyClassFixed<BBoxProperty<LocatedEntity>>().data() = WFMath::AxisBox<3>(WFMath::Point<3>(-64, -64, -64), WFMath::Point<3>(64, 64, 64));
        }

        static void addGroundPlane(TestPhysicalDomain& domain, SpeedTestContext& context, TypeNode<LocatedEntity>& type) {
                ModeProperty fixedMode{};
                fixedMode.set("fixed");

                LocatedEntity ground(context.newId());
                ground.setType(&type);
                ground.setProperty(ModeProperty::property_name, std::unique_ptr<PropertyBase>(fixedMode.copy()));
                ground.requirePropertyClassFixed<PositionProperty<LocatedEntity>>().data() = WFMath::Point<3>(0, 0, 0);
                ground.requirePropertyClassFixed<OrientationProperty<LocatedEntity>>().data() = WFMath::Quaternion::IDENTITY();
                ground.requirePropertyClassFixed<BBoxProperty<LocatedEntity>>().data() = WFMath::AxisBox<3>(WFMath::Point<3>(-10, -1, -10), WFMath::Point<3>(10, 0, 10));
                domain.addEntity(ground);
        }

        static void setCommonMovementProperties(LocatedEntity& entity,
                                                double speedGround,
                                                double speedWater,
                                                double speedFlight,
                                                double mass = 10.0) {
                Property<double, LocatedEntity> massProp{};
                massProp.data() = mass;
                entity.setProperty("mass", std::unique_ptr<PropertyBase>(massProp.copy()));

                Property<double, LocatedEntity> speedGroundProp{};
                speedGroundProp.data() = speedGround;
                entity.setProperty("speed_ground", std::unique_ptr<PropertyBase>(speedGroundProp.copy()));

                Property<double, LocatedEntity> speedWaterProp{};
                speedWaterProp.data() = speedWater;
                entity.setProperty("speed_water", std::unique_ptr<PropertyBase>(speedWaterProp.copy()));

                Property<double, LocatedEntity> speedFlightProp{};
                speedFlightProp.data() = speedFlight;
                entity.setProperty("speed_flight", std::unique_ptr<PropertyBase>(speedFlightProp.copy()));
        }

        static void setPropel(LocatedEntity& entity, const WFMath::Vector<3>& direction) {
                PropelProperty propel{};
                propel.data() = direction;
                entity.setProperty(PropelProperty::property_name, std::unique_ptr<PropertyBase>(propel.copy()));
        }

        void test_ground_modifier(SpeedTestContext& context) {
                TypeNode<LocatedEntity> rootType("root");
                LocatedEntity rootEntity(context.newId());
                rootEntity.setType(&rootType);
                setupRootEntity(rootEntity);

                TestWorld testWorld(&rootEntity);
                TestPhysicalDomain domain(rootEntity);

                PhysicalDomain::EnvironmentSpeedModifiers modifiers{};
                modifiers.ground = 1.5;
                modifiers.water = 0.5;
                modifiers.air = 2.0;
                domain.setEnvironmentSpeedModifiers(modifiers);

                TypeNode<LocatedEntity> groundType("ground");
                addGroundPlane(domain, context, groundType);

                TypeNode<LocatedEntity> moverType("mover");
                LocatedEntity mover(context.newId());
                mover.setType(&moverType);
                ModeProperty freeMode{};
                freeMode.set("free");
                mover.setProperty(ModeProperty::property_name, std::unique_ptr<PropertyBase>(freeMode.copy()));
                setCommonMovementProperties(mover, 5.0, 3.0, 4.0);
                mover.requirePropertyClassFixed<PositionProperty<LocatedEntity>>().data() = WFMath::Point<3>(0, 1, 0);
                mover.requirePropertyClassFixed<OrientationProperty<LocatedEntity>>().data() = WFMath::Quaternion::IDENTITY();
                mover.requirePropertyClassFixed<BBoxProperty<LocatedEntity>>().data() = WFMath::AxisBox<3>(WFMath::Point<3>(-0.5, 0, -0.5), WFMath::Point<3>(0.5, 1, 0.5));

                domain.addEntity(mover);
                setPropel(mover, WFMath::Vector<3>(1, 0, 0));

                OpVector res;
                domain.tick(100ms, res);

                auto* body = domain.getRigidBody(mover.getIdAsInt());
                ASSERT_NOT_NULL(body);
                double expected = 5.0 * modifiers.ground;
                ASSERT_FUZZY_EQUAL(body->getLinearVelocity().x(), expected, 0.01);
        }

        void test_water_modifier(SpeedTestContext& context) {
                TypeNode<LocatedEntity> rootType("root");
                LocatedEntity rootEntity(context.newId());
                rootEntity.setType(&rootType);
                setupRootEntity(rootEntity);

                TestWorld testWorld(&rootEntity);
                TestPhysicalDomain domain(rootEntity);

                PhysicalDomain::EnvironmentSpeedModifiers modifiers{};
                modifiers.ground = 0.8;
                modifiers.water = 1.6;
                modifiers.air = 1.0;
                domain.setEnvironmentSpeedModifiers(modifiers);

                TypeNode<LocatedEntity> groundType("ground");
                addGroundPlane(domain, context, groundType);

                TypeNode<LocatedEntity> swimmerType("swimmer");
                LocatedEntity swimmer(context.newId());
                swimmer.setType(&swimmerType);
                ModeProperty submergedMode{};
                submergedMode.set("submerged");
                swimmer.setProperty(ModeProperty::property_name, std::unique_ptr<PropertyBase>(submergedMode.copy()));
                setCommonMovementProperties(swimmer, 2.0, 2.5, 3.0);
                swimmer.requirePropertyClassFixed<PositionProperty<LocatedEntity>>().data() = WFMath::Point<3>(0, 0.5, 0);
                swimmer.requirePropertyClassFixed<OrientationProperty<LocatedEntity>>().data() = WFMath::Quaternion::IDENTITY();
                swimmer.requirePropertyClassFixed<BBoxProperty<LocatedEntity>>().data() = WFMath::AxisBox<3>(WFMath::Point<3>(-0.5, 0, -0.5), WFMath::Point<3>(0.5, 1, 0.5));

                domain.addEntity(swimmer);
                setPropel(swimmer, WFMath::Vector<3>(1, 0, 0));

                OpVector res;
                domain.tick(100ms, res);

                auto* body = domain.getRigidBody(swimmer.getIdAsInt());
                ASSERT_NOT_NULL(body);
                double expected = 2.5 * modifiers.water;
                ASSERT_FUZZY_EQUAL(body->getLinearVelocity().x(), expected, 0.01);
        }

        void test_air_modifier(SpeedTestContext& context) {
                TypeNode<LocatedEntity> rootType("root");
                LocatedEntity rootEntity(context.newId());
                rootEntity.setType(&rootType);
                setupRootEntity(rootEntity);

                TestWorld testWorld(&rootEntity);
                TestPhysicalDomain domain(rootEntity);

                PhysicalDomain::EnvironmentSpeedModifiers modifiers{};
                modifiers.ground = 1.0;
                modifiers.water = 0.5;
                modifiers.air = 1.7;
                domain.setEnvironmentSpeedModifiers(modifiers);

                TypeNode<LocatedEntity> groundType("ground");
                addGroundPlane(domain, context, groundType);

                TypeNode<LocatedEntity> flyerType("flyer");
                LocatedEntity flyer(context.newId());
                flyer.setType(&flyerType);
                ModeProperty freeMode{};
                freeMode.set("free");
                flyer.setProperty(ModeProperty::property_name, std::unique_ptr<PropertyBase>(freeMode.copy()));
                setCommonMovementProperties(flyer, 3.0, 1.0, 6.0);
                flyer.requirePropertyClassFixed<PositionProperty<LocatedEntity>>().data() = WFMath::Point<3>(0, 10, 0);
                flyer.requirePropertyClassFixed<OrientationProperty<LocatedEntity>>().data() = WFMath::Quaternion::IDENTITY();
                flyer.requirePropertyClassFixed<BBoxProperty<LocatedEntity>>().data() = WFMath::AxisBox<3>(WFMath::Point<3>(-0.5, 0, -0.5), WFMath::Point<3>(0.5, 1, 0.5));

                domain.addEntity(flyer);
                setPropel(flyer, WFMath::Vector<3>(1, 0, 0));

                OpVector res;
                domain.tick(100ms, res);

                auto* body = domain.getRigidBody(flyer.getIdAsInt());
                ASSERT_NOT_NULL(body);
                double expected = 6.0 * modifiers.air;
                ASSERT_FUZZY_EQUAL(body->getLinearVelocity().x(), expected, 0.01);
        }
};

int main() {
        PhysicalDomainSpeedTest test;
        return test.run();
}
