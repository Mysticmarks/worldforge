#include "wfmath/point.h"
#include "wfmath/point_funcs.h"

#include <vector>
#include <list>
#include <cassert>

using namespace WFMath;

int main() {
    // Unweighted barycenter of three 2D points
    std::vector<Point<2>> pts2 = {Point<2>(0, 0), Point<2>(6, 0), Point<2>(0, 6)};
    Point<2> bc2 = Barycenter(pts2);
    assert(bc2.isEqualTo(Point<2>(2, 2)));

    // Weighted barycenter of three 3D points
    std::vector<Point<3>> pts3 = {Point<3>(0, 0, 0), Point<3>(3, 3, 3), Point<3>(6, 0, 0)};
    std::list<CoordType> weights = {1, 2, 3};
    Point<3> bc3 = Barycenter(pts3, weights);
    assert(bc3.isEqualTo(Point<3>(4, 1, 1)));

    return 0;
}
