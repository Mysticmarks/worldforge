#include <components/ogre/AutoGraphicsLevelManager.h>

#include <catch2/catch_test_macros.hpp>

#include <functional>

using Ember::OgreView::Detail::GraphicsLevelChangeResult;
using Ember::OgreView::Detail::processGraphicsLevelChange;

TEST_CASE("processGraphicsLevelChange handles minimum boundary", "[AutoGraphicsLevelManager]") {
        bool atMaximum = false;
        bool atMinimum = false;
        int calls = 0;

        auto result = processGraphicsLevelChange(-10.0f, atMaximum, atMinimum, [&](float) {
                ++calls;
                return false;
        });

        REQUIRE(result == GraphicsLevelChangeResult::ReachedMinimum);
        REQUIRE(calls == 1);
        REQUIRE(atMinimum);
        REQUIRE_FALSE(atMaximum);

        result = processGraphicsLevelChange(-5.0f, atMaximum, atMinimum, [&](float) {
                ++calls;
                return true;
        });

        REQUIRE(result == GraphicsLevelChangeResult::AlreadyMinimum);
        REQUIRE(calls == 1);

        result = processGraphicsLevelChange(5.0f, atMaximum, atMinimum, [&](float) {
                ++calls;
                return false;
        });

        REQUIRE(result == GraphicsLevelChangeResult::ReachedMaximum);
        REQUIRE(calls == 2);
        REQUIRE(atMaximum);
        REQUIRE_FALSE(atMinimum);
}

TEST_CASE("processGraphicsLevelChange handles maximum boundary", "[AutoGraphicsLevelManager]") {
        bool atMaximum = false;
        bool atMinimum = false;
        int calls = 0;

        auto result = processGraphicsLevelChange(8.0f, atMaximum, atMinimum, [&](float) {
                ++calls;
                return false;
        });

        REQUIRE(result == GraphicsLevelChangeResult::ReachedMaximum);
        REQUIRE(calls == 1);
        REQUIRE(atMaximum);
        REQUIRE_FALSE(atMinimum);

        result = processGraphicsLevelChange(3.0f, atMaximum, atMinimum, [&](float) {
                ++calls;
                return true;
        });

        REQUIRE(result == GraphicsLevelChangeResult::AlreadyMaximum);
        REQUIRE(calls == 1);

        result = processGraphicsLevelChange(-3.0f, atMaximum, atMinimum, [&](float) {
                ++calls;
                return true;
        });

        REQUIRE(result == GraphicsLevelChangeResult::Applied);
        REQUIRE(calls == 2);
        REQUIRE_FALSE(atMaximum);
        REQUIRE_FALSE(atMinimum);
}

TEST_CASE("processGraphicsLevelChange skips zero change", "[AutoGraphicsLevelManager]") {
        bool atMaximum = false;
        bool atMinimum = false;
        bool invoked = false;

        auto result = processGraphicsLevelChange(0.0f, atMaximum, atMinimum, [&](float) {
                invoked = true;
                return true;
        });

        REQUIRE(result == GraphicsLevelChangeResult::NoChange);
        REQUIRE_FALSE(invoked);
        REQUIRE_FALSE(atMaximum);
        REQUIRE_FALSE(atMinimum);
}
