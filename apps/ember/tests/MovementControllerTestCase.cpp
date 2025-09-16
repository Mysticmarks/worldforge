#include "MovementControllerTestCase.h"
#include "components/ogre/MovementController.h"
#include "domain/IHeightProvider.h"

#include <OgreVector3.h>

#include <vector>

using namespace Ember::OgreView;

namespace Ember {
namespace {

struct MockHeightProvider : IHeightProvider {
        mutable std::vector<TerrainPosition> queries;
        bool shouldSucceed = true;
        float providedHeight = 0.f;

        bool getHeight(const TerrainPosition& atPosition, float& height) const override {
                queries.push_back(atPosition);
                if (shouldSucceed) {
                        height = providedHeight;
                        return true;
                }
                return false;
        }

        void blitHeights(int, int, int, int, std::vector<float>&) const override {}
};

} // namespace

void MovementControllerTestCase::testProjectsPointOntoTerrain() {
        MockHeightProvider provider;
        provider.shouldSucceed = true;
        provider.providedHeight = 42.f;

        Ogre::Vector3 point(5.f, 1.f, -3.f);
        Ogre::Vector3 projected = projectPointOntoTerrain(provider, point);

        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), provider.queries.size());
        CPPUNIT_ASSERT_DOUBLES_EQUAL(point.x, provider.queries.front().x(), 0.0001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(point.z, provider.queries.front().y(), 0.0001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(point.x, projected.x, 0.0001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(provider.providedHeight, projected.y, 0.0001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(point.z, projected.z, 0.0001);
}

void MovementControllerTestCase::testKeepsOriginalHeightWhenLookupFails() {
        MockHeightProvider provider;
        provider.shouldSucceed = false;
        provider.providedHeight = 12.f;

        Ogre::Vector3 point(2.f, 7.f, 9.f);
        Ogre::Vector3 projected = projectPointOntoTerrain(provider, point);

        CPPUNIT_ASSERT_EQUAL(static_cast<size_t>(1), provider.queries.size());
        CPPUNIT_ASSERT_DOUBLES_EQUAL(point.x, projected.x, 0.0001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(point.y, projected.y, 0.0001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(point.z, projected.z, 0.0001);
}

} // namespace Ember
