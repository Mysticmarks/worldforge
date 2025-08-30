// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2005 Alistair Riddoch

#include "Area.h"
#include "Segment.h"

#include <wfmath/intersect.h>
#include <cmath>

#include <cassert>

using WFMath::CoordType;

namespace Mercator {

typedef WFMath::Point<2> Point2;
typedef WFMath::Vector<2> Vector2;

#ifndef NDEBUG

static bool isZero(CoordType d) {
	return (std::fabs(d) < WFMath::numeric_constants<CoordType>::epsilon());
}

#endif

/// \brief Helper to clip points to a given range.
struct TopClip {
	/// Constructor
	///
	/// @param t top of y range
	explicit TopClip(CoordType t) : topZ(t) {}

	/// \brief Check a point is outside this clip.
	///
	/// @param p point to be checked.
	/// @return true if p is outside the clip.
	bool inside(const Point2& p) const {
		return p.y() >= topZ;
	}

	/// \brief Determine the point where a line crosses this clip.
	///
	/// @param u one of of a line that crosses this clip
	/// @param v one of of a line that crosses this clip
	/// @return a point where the line cross this clip.
	Point2 clip(const Point2& u, const Point2& v) const {
		CoordType dy = v.y() - u.y();
		CoordType dx = v.x() - u.x();

		// shouldn't every happen - if dy iz zero, the line is horizontal,
		// so either both points should be inside, or both points should be
		// outside. In either case, we should not call clip()
		assert(!isZero(dy));

		CoordType t = (topZ - u.y()) / dy;
		return Point2(u.x() + t * dx, topZ);
	}

	/// \brief Top of z range.
	CoordType topZ;
};

/// \brief Helper to clip points to a given range.
struct BottomClip {
	/// Constructor
	///
	/// @param t bottom of y range
	explicit BottomClip(CoordType t) : bottomZ(t) {}

	/// \brief Check a point is outside this clip.
	///
	/// @param p point to be checked.
	/// @return true if p is outside the clip.
	bool inside(const Point2& p) const {
		return p.y() < bottomZ;
	}

	/// \brief Determine the point where a line crosses this clip.
	///
	/// @param u one of of a line that crosses this clip
	/// @param v one of of a line that crosses this clip
	/// @return a point where the line cross this clip.
	Point2 clip(const Point2& u, const Point2& v) const {
		CoordType dy = v.y() - u.y();
		CoordType dx = v.x() - u.x();
		assert(!isZero(dy));

		CoordType t = (u.y() - bottomZ) / -dy;
		return Point2(u.x() + t * dx, bottomZ);
	}

	/// \brief Bottom of z range.
	CoordType bottomZ;
};

/// \brief Helper to clip points to a given range.
struct LeftClip {
	/// Constructor
	///
	/// @param t left of x range.
	explicit LeftClip(CoordType t) : leftX(t) {}

	/// \brief Check a point is outside this clip.
	///
	/// @param p point to be checked.
	/// @return true if p is outside the clip.
	bool inside(const Point2& p) const {
		return p.x() >= leftX;
	}

	/// \brief Determine the point where a line crosses this clip.
	///
	/// @param u one of of a line that crosses this clip
	/// @param v one of of a line that crosses this clip
	/// @return a point where the line cross this clip.
	Point2 clip(const Point2& u, const Point2& v) const {
		CoordType dy = v.y() - u.y();
		CoordType dx = v.x() - u.x();

		// shouldn't every happen
		assert(!isZero(dx));

		CoordType t = (leftX - u.x()) / dx;
		return Point2(leftX, u.y() + t * dy);
	}

	/// \brief Left of x range.
	CoordType leftX;
};

/// \brief Helper to clip points to a given range.
struct RightClip {
	/// Constructor
	///
	/// @param t right of x range.
	explicit RightClip(CoordType t) : rightX(t) {}

	/// \brief Check a point is outside this clip.
	///
	/// @param p point to be checked.
	/// @return true if p is outside the clip.
	bool inside(const Point2& p) const {
		return p.x() < rightX;
	}

	/// \brief Determine the point where a line crosses this clip.
	///
	/// @param u one of of a line that crosses this clip
	/// @param v one of of a line that crosses this clip
	/// @return a point where the line cross this clip.
	Point2 clip(const Point2& u, const Point2& v) const {
		CoordType dy = v.y() - u.y();
		CoordType dx = v.x() - u.x();

		// shouldn't every happen
		assert(!isZero(dx));

		CoordType t = (u.x() - rightX) / -dx;
		return Point2(rightX, u.y() + t * dy);
	}

	/// \brief Right of x range.
	CoordType rightX;
};

// Pass clipper by const reference to avoid copying.  A micro-benchmark of a
// simplified kernel showed roughly a 1% improvement compared to passing the
// clipper by value.
template<class Clip>
WFMath::Polygon<2> sutherlandHodgmanKernel(const WFMath::Polygon<2>& inpoly, const Clip& clipper) {
	WFMath::Polygon<2> outpoly;

	if (!inpoly.isValid()) return inpoly;
	std::size_t points = inpoly.numCorners();
	if (points < 3) return outpoly; // i.e an invalid result

	Point2 lastPt = inpoly.getCorner(points - 1);
	bool lastInside = clipper.inside(lastPt);

	for (std::size_t p = 0; p < points; ++p) {

		Point2 curPt = inpoly.getCorner(p);
		bool inside = clipper.inside(curPt);

		if (lastInside) {
			if (inside) {
				// emit curPt
				outpoly.addCorner(outpoly.numCorners(), curPt);
			} else {
				// emit intersection of edge with clip line
				outpoly.addCorner(outpoly.numCorners(), clipper.clip(lastPt, curPt));
			}
		} else {
			if (inside) {
				// emit both
				outpoly.addCorner(outpoly.numCorners(), clipper.clip(lastPt, curPt));
				outpoly.addCorner(outpoly.numCorners(), curPt);
			} else {
				// don't emit anything
			}
		} // last was outside

		lastPt = curPt;
		lastInside = inside;
	}

	return outpoly;
}

Area::Area(int layer, bool hole) :
		m_layer(layer),
		m_hole(hole) {
}

void Area::setShape(const WFMath::Polygon<2>& p) {
	assert(p.isValid());
	m_shape = p;
	m_box = p.boundingBox();
}

bool Area::contains(CoordType x, CoordType z) const {
	if (!WFMath::Contains(m_box, Point2(x, z), false)) return false;

	return WFMath::Contains(m_shape, Point2(x, z), false);
}

WFMath::Polygon<2> Area::clipToSegment(const Segment& s) const {
	// box reject
	if (!checkIntersects(s)) return WFMath::Polygon<2>();

        WFMath::AxisBox<2> segBox(s.getRect());

        // Construct clippers once and pass them by reference to avoid creating
        // temporary objects for each call.
        TopClip top(segBox.lowCorner().y());
        BottomClip bottom(segBox.highCorner().y());
        LeftClip left(segBox.lowCorner().x());
        RightClip right(segBox.highCorner().x());

        WFMath::Polygon<2> clipped = sutherlandHodgmanKernel(m_shape, top);

        clipped = sutherlandHodgmanKernel(clipped, bottom);
        clipped = sutherlandHodgmanKernel(clipped, left);
        clipped = sutherlandHodgmanKernel(clipped, right);

	return clipped;
}

bool Area::checkIntersects(const Segment& s) const {
	return m_shape.numCorners() && (WFMath::Intersect(m_shape, s.getRect(), false) ||
									WFMath::Contains(s.getRect(), m_shape.getCorner(0), false));
}

} // of namespace
