#include "wfmath/intersect.h"
#include "wfmath/point.h"
#include "wfmath/point_funcs.h"
#include "wfmath/axisbox.h"
#include "wfmath/axisbox_funcs.h"
#include "wfmath/polygon.h"
#include "wfmath/polygon_intersect.h"
#include <catch2/catch_test_macros.hpp>

using namespace WFMath;

struct DummyA {};
struct DummyB {};

TEST_CASE("intersect_unknown_test")
{
    // Known combination but only implemented for reversed order
    Point<2> p(Point<2>().setToOrigin());
    AxisBox<2> box(p, p);
    REQUIRE(Intersect(box, p, false));
    REQUIRE(Intersect(p, box, false));

    // Polygon combinations are implemented only with the polygon first
    Polygon<2> poly;
    poly.addCorner(0, p);
    poly.addCorner(0, Point<2>(1, 0));
    poly.addCorner(0, Point<2>(1, -1));
    poly.addCorner(0, Point<2>(0, -1));
    poly.isValid();
    REQUIRE(Intersect(poly, box, false));
    REQUIRE(Intersect(box, poly, false));

    // Completely unknown combinations should be safely handled
    DummyA a;
    DummyB b;
    REQUIRE(!Intersect(a, b, false));
    REQUIRE(!Intersect(b, a, true));
    REQUIRE(!Intersect(a, a, false));
    // Mixed known/unknown combinations
    REQUIRE(!Intersect(a, box, false));
    REQUIRE(!Intersect(box, a, true));
    }
