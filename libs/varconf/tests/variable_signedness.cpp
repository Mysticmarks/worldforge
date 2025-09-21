#include <varconf/varconf.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("VarBase recognizes signed integers", "[varconf]") {
        varconf::VarBase positive("+123");
        REQUIRE(positive.is_int());
        REQUIRE(static_cast<int>(positive) == 123);

        varconf::VarBase negative("-42");
        REQUIRE(negative.is_int());
        REQUIRE(static_cast<int>(negative) == -42);

        varconf::VarBase lone_minus("-");
        REQUIRE_FALSE(lone_minus.is_int());

        varconf::VarBase embedded_sign("12-3");
        REQUIRE_FALSE(embedded_sign.is_int());
}
