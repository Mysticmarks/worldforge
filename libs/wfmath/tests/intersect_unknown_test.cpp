#include "wfmath/intersect.h"
#include "wfmath/point.h"
#include "wfmath/point_funcs.h"
#include "wfmath/axisbox.h"
#include "wfmath/axisbox_funcs.h"
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

    // Completely unknown combinations should be safely handled
    DummyA a;
    DummyB b;
    assert(!Intersect(a, b, false));
    assert(!Intersect(b, a, true));
    assert(!Intersect(a, a, false));
    return 0;
}
