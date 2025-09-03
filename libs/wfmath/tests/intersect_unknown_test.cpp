#include "wfmath/intersect.h"
#include "wfmath/point.h"
#include "wfmath/point_funcs.h"
#include "wfmath/axisbox.h"
#include "wfmath/axisbox_funcs.h"
#include "wfmath/polygon.h"
#include "wfmath/polygon_intersect.h"
#include <assert.h>

using namespace WFMath;

struct DummyA {};
struct DummyB {};

int main()
{
    // Known combination but only implemented for reversed order
    Point<2> p(Point<2>().setToOrigin());
    AxisBox<2> box(p, p);
    assert(Intersect(box, p, false));
    assert(Intersect(p, box, false));

    // Polygon combinations are implemented only with the polygon first
    Polygon<2> poly;
    poly.addCorner(0, p);
    poly.addCorner(0, Point<2>(1, 0));
    poly.addCorner(0, Point<2>(1, -1));
    poly.addCorner(0, Point<2>(0, -1));
    poly.isValid();
    assert(Intersect(poly, box, false));
    assert(Intersect(box, poly, false));

    // Completely unknown combinations should be safely handled
    DummyA a;
    DummyB b;
    assert(!Intersect(a, b, false));
    assert(!Intersect(b, a, true));
    assert(!Intersect(a, a, false));
    // Mixed known/unknown combinations
    assert(!Intersect(a, box, false));
    assert(!Intersect(box, a, true));
    return 0;
}
