#include <varconf/varconf.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("VarBase recognizes case-insensitive boolean tokens", "[varconf]") {
        varconf::VarBase uppercase_true("TRUE");
        REQUIRE(uppercase_true.is_bool());
        REQUIRE(static_cast<bool>(uppercase_true));

        varconf::VarBase mixed_false("Off");
        REQUIRE(mixed_false.is_bool());
        REQUIRE_FALSE(static_cast<bool>(mixed_false));
}

TEST_CASE("VarBase trims whitespace when parsing booleans", "[varconf]") {
        varconf::VarBase padded_yes("  yes\t");
        REQUIRE(padded_yes.is_bool());
        REQUIRE(static_cast<bool>(padded_yes));

        varconf::VarBase padded_no("\nNo \r");
        REQUIRE(padded_no.is_bool());
        REQUIRE_FALSE(static_cast<bool>(padded_no));
}

TEST_CASE("VarBase rejects non-boolean tokens", "[varconf]") {
        varconf::VarBase invalid("truthy");
        REQUIRE_FALSE(invalid.is_bool());
        REQUIRE_FALSE(static_cast<bool>(invalid));
}
