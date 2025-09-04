// polygon_intersect.cpp (Polygon<2> intersection functions)
//
//  The WorldForge Project
//  Copyright (C) 2002  Ron Steinke and The WorldForge Project
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
//
//  For information about WorldForge and its authors, please contact
//  the Worldforge Web Site at http://www.worldforge.org.

// Author: Ron Steinke
// Created: 2002-2-20

#include "polygon_intersect.h"

#include "segment.h"
#include "rotbox.h"

#include <algorithm>
#include <list>
#include <cassert>
#include <cmath>

namespace WFMath {


template<int dim>
inline Vector<dim> Poly2Orient<dim>::offset(const Point<dim>& pd, Point<2>& p2) const {
	assert(m_origin.isValid()); // Check for empty polygon before calling this

	Vector<dim> out = pd - m_origin;

	for (int j = 0; j < 2; ++j) {
		p2[j] = Dot(out, m_axes[j]);
		out -= p2[j] * m_axes[j];
	}

	return out;
}

template<int dim>
inline bool Poly2Orient<dim>::checkContained(const Point<dim>& pd, Point<2>& p2) const {
	Vector<dim> off = offset(pd, p2);

	CoordType sqrsum = 0;
	for (int i = 0; i < dim; ++i)
		sqrsum += pd[i] * pd[i];

	return off.sqrMag() < numeric_constants<CoordType>::epsilon() * sqrsum;
}

template<>
bool Poly2Orient<3>::checkIntersectPlane(const AxisBox<3>& b, Point<2>& p2,
										 bool proper) const;

template<int dim>
bool Poly2Orient<dim>::checkIntersect(const AxisBox<dim>& b, Point<2>& p2,
									  bool proper) const {
	assert(m_origin.isValid());

	if (!m_axes[0].isValid()) {
		// Single point
		p2[0] = p2[1] = 0;
		return Intersect(b, convert(p2), proper);
	}

	if (m_axes[1].isValid()) {
		// A plane

		// I only know how to do this in 3D, so write a function which will
		// specialize to different dimensions

		return checkIntersectPlane(b, p2, proper);
	}

	// A line

	// This is a modified version of AxisBox<>/Segment<> intersection

	CoordType min = 0, max = 0; // Initialize to avoid compiler warnings
	bool got_bounds = false;

	for (int i = 0; i < dim; ++i) {
		const CoordType dist = (m_axes[0])[i]; // const may optimize away better
		if (dist == 0) {
			if (_Less(m_origin[i], b.lowCorner()[i], proper)
				|| _Greater(m_origin[i], b.highCorner()[i], proper))
				return false;
		} else {
			CoordType low = (b.lowCorner()[i] - m_origin[i]) / dist;
			CoordType high = (b.highCorner()[i] - m_origin[i]) / dist;
			if (low > high) {
				CoordType tmp = high;
				high = low;
				low = tmp;
			}
			if (got_bounds) {
				if (low > min)
					min = low;
				if (high < max)
					max = high;
			} else {
				min = low;
				max = high;
				got_bounds = true;
			}
		}
	}

	assert(got_bounds); // We can't be parallel in _all_ dimensions

	if (_LessEq(min, max, proper)) {
		p2[0] = (max - min) / 2;
		p2[1] = 0;
		return true;
	} else
		return false;
}

template<int dim>
int Intersect(const Poly2Orient<dim>& o1, const Poly2Orient<dim>& o2,
			  Poly2OrientIntersectData& data) {
	if (!o1.m_origin.isValid() || !o2.m_origin.isValid()) { // No points
		return -1;
	}

	// Check for single point basis

	if (!o1.m_axes[0].isValid()) {
		if (!o2.checkContained(o1.m_origin, data.p2))
			return -1; // no intersect

		//Poly2OrientIntersectData data;

		data.p1[0] = data.p1[1] = 0;

		return 0; // point intersect
	}

	if (!o2.m_axes[0].isValid()) {
		if (!o1.checkContained(o2.m_origin, data.p1))
			return -1; // no intersect

		data.p2[0] = data.p2[1] = 0;

		return 0; // point intersect
	}

	// Find a common basis for the plane's orientations
	// by projecting out the part of o1's basis that lies
	// in o2's basis

	Vector<dim> basis1, basis2;
	CoordType sqrmag1, sqrmag2;
	int basis_size = 0;

	basis1 = o2.m_axes[0] * Dot(o2.m_axes[0], o1.m_axes[0]);
	if (o2.m_axes[1].isValid())
		basis1 += o2.m_axes[1] * Dot(o2.m_axes[1], o1.m_axes[0]);

	// Don't need to scale, the m_axes are unit vectors
	sqrmag1 = basis1.sqrMag();
	if (sqrmag1 > numeric_constants<CoordType>::epsilon() * numeric_constants<CoordType>::epsilon())
		basis_size = 1;

	if (o1.m_axes[1].isValid()) {
		basis2 = o2.m_axes[0] * Dot(o2.m_axes[0], o1.m_axes[1]);
		if (o2.m_axes[1].isValid())
			basis2 += o2.m_axes[1] * Dot(o2.m_axes[1], o1.m_axes[1]);

		// Project out part parallel to basis1
		if (basis_size == 1)
			basis2 -= basis1 * (Dot(basis1, basis2) / sqrmag1);

		sqrmag2 = basis2.sqrMag();
		if (sqrmag2 > numeric_constants<CoordType>::epsilon() * numeric_constants<CoordType>::epsilon()) {
			if (basis_size++ == 0) {
				basis1 = basis2;
				sqrmag1 = sqrmag2;
			}
		}
	}

	Vector<dim> off = o2.m_origin - o1.m_origin;

	switch (basis_size) {
		case 0: {
			// All vectors are orthogonal, check for a common point in the plane
			// This can happen even in 3d for degenerate bases

			data.p1[0] = Dot(o1.m_axes[0], off);
			Vector<dim> off1 = o1.m_axes[0] * data.p1[0];
			if (o1.m_axes[1].isValid()) {
				data.p1[1] = Dot(o1.m_axes[1], off);
				off1 += o1.m_axes[1] * data.p1[1];
			} else
				data.p1[1] = 0;

			data.p2[0] = -Dot(o2.m_axes[0], off);
			Vector<dim> off2 = o2.m_axes[0] * data.p2[0];
			if (o1.m_axes[1].isValid()) {
				data.p2[1] = -Dot(o2.m_axes[1], off);
				off2 += o1.m_axes[1] * data.p2[1];
			} else
				data.p2[1] = 0;

			if (off1 - off2 != off) // No common point
				return -1;
			else  // Got a point
				return 1;
		}
		case 1: {
			// Check for an intersection line

			data.o1_is_line = !o1.m_axes[1].isValid();
			data.o2_is_line = !o2.m_axes[1].isValid();

			if (!o1.m_axes[1].isValid() && !o2.m_axes[1].isValid()) {
				CoordType proj = Dot(off, o2.m_axes[0]);
				if (off != o2.m_axes[0] * proj)
					return -1;

				data.v1[0] = 1;
				data.v1[1] = 0;
				data.p1[0] = data.p1[1] = 0;
				data.v2[0] = (Dot(o1.m_axes[0], o2.m_axes[0]) > 0) ? 1 : -1;
				data.v2[1] = 0;
				data.p2[0] = -proj;
				data.p2[1] = 0;

				return 1;
			}

			if (!o1.m_axes[1].isValid()) {
				data.p2[0] = -Dot(off, o2.m_axes[0]);
				data.p2[1] = -Dot(off, o2.m_axes[1]);

				if (off != -data.p2[0] * o2.m_axes[0] - data.p2[1] * o2.m_axes[1])
					return -1;

				data.v1[0] = 1;
				data.v1[1] = 0;
				data.p1[0] = data.p1[1] = 0;
				data.v2[0] = Dot(o1.m_axes[0], o2.m_axes[0]);
				data.v2[1] = Dot(o1.m_axes[0], o2.m_axes[1]);

				return 1;
			}

			if (!o2.m_axes[1].isValid()) {
				data.p1[0] = Dot(off, o1.m_axes[0]);
				data.p1[1] = Dot(off, o1.m_axes[1]);

				if (off != data.p1[0] * o1.m_axes[0] + data.p1[1] * o1.m_axes[1])
					return -1;

				data.v2[0] = 1;
				data.v2[1] = 0;
				data.p2[0] = data.p2[1] = 0;
				data.v1[0] = Dot(o1.m_axes[0], o2.m_axes[0]);
				data.v1[1] = Dot(o1.m_axes[1], o2.m_axes[0]);

				return 1;
			}

			data.p1[0] = Dot(off, o1.m_axes[0]);
			data.p1[1] = Dot(off, o1.m_axes[1]);
			data.p2[0] = -Dot(off, o2.m_axes[0]);
			data.p2[1] = -Dot(off, o2.m_axes[1]);

			if (off != data.p1[0] * o1.m_axes[0] + data.p1[1] * o1.m_axes[1]
					   - data.p2[0] * o2.m_axes[0] - data.p2[1] * o2.m_axes[1])
				return -1;

			basis1 /= std::sqrt(sqrmag1);

			data.v1[0] = Dot(o1.m_axes[0], basis1);
			data.v1[1] = Dot(o1.m_axes[1], basis1);
			data.v2[0] = Dot(o2.m_axes[0], basis1);
			data.v2[1] = Dot(o2.m_axes[1], basis1);

			return 1;
		}
		case 2: {
			assert(o1.m_axes[1].isValid() && o2.m_axes[1].isValid());

			// The planes are parallel, check if they are the same plane
			CoordType off_sqr_mag = data.off.sqrMag();

			// Find the offset between the origins in o2's coordnates

			if (off_sqr_mag != 0) { // The offsets aren't identical
				Vector<dim> off_copy = off;

				data.off[0] = Dot(o2.m_axes[0], off);
				off_copy -= o1.m_axes[0] * data.off[0];
				data.off[1] = Dot(o2.m_axes[1], off);
				off_copy -= o1.m_axes[1] * data.off[1];

				if (off_copy.sqrMag() > off_sqr_mag * numeric_constants<CoordType>::epsilon())
					return -1; // The planes are different
			} else
				data.off[0] = data.off[1] = 0;

			// Define o2's basis vectors in o1's coordinates
			data.v1[0] = Dot(o2.m_axes[0], o1.m_axes[0]);
			data.v1[1] = Dot(o2.m_axes[0], o1.m_axes[1]);
			data.v2[0] = Dot(o2.m_axes[1], o1.m_axes[0]);
			data.v2[1] = Dot(o2.m_axes[1], o1.m_axes[1]);

			return 2;
		}
		default:
			assert(false);
			return -1;
	}
}

template<int dim>
inline bool Intersect(const Polygon<dim>& r, const Point<dim>& p, bool proper) {
	Point<2> p2;

	return r.m_poly.numCorners() > 0 && r.m_orient.checkContained(p, p2)
		   && Intersect(r.m_poly, p2, proper);
}

template<int dim>
inline bool Contains(const Point<dim>& p, const Polygon<dim>& r, bool proper) {
	if (r.m_poly.numCorners() == 0)
		return true;

	if (proper)
		return false;

	for (size_t i = 1; i < r.m_poly.numCorners(); ++i)
		if (r.m_poly[i] != r.m_poly[0])
			return false;

	Point<2> p2;

	return r.m_orient.checkContained(p, p2) && p2 == r.m_poly[0];
}

template<int dim>
bool Intersect(const Polygon<dim>& p, const AxisBox<dim>& b, bool proper) {
	size_t corners = p.m_poly.numCorners();

	if (corners == 0)
		return false;

	Point<2> p2;

	if (!p.m_orient.checkIntersect(b, p2, proper))
		return false;

	Segment<dim> s;
	s.endpoint(0) = p.m_orient.convert(p.m_poly.getCorner(corners - 1));
	int next_end = 1;

	for (size_t i = 0; i < corners; ++i) {
		s.endpoint(next_end) = p.m_orient.convert(p.m_poly.getCorner(i));
		if (Intersect(b, s, proper))
			return true;
		next_end = next_end ? 0 : 1;
	}

	return Contains(p, p2, proper);
}

template<int dim>
bool _PolyContainsBox(const Poly2Orient<dim>& orient, const Polygon<2>& poly,
					  const Point<dim>& corner, const Vector<dim>& size, bool proper) {
	int num_dim = 0, nonzero_dim = -1;

	for (int i = 0; i < dim; ++i) {
		if (size[i] == 0)
			continue;
		if (num_dim == 2)
			return false;
		if (nonzero_dim == -1 || std::fabs(size[nonzero_dim]) < std::fabs(size[i]))
			nonzero_dim = i;
		++num_dim;
	}

	Point<2> corner1;

	if (!orient.checkContained(corner, corner1))
		return false;

	if (num_dim == 0)
		return Contains(poly, corner1, proper);

	Point<2> corner2;

	if (!orient.checkContained(corner + size, corner2))
		return false;

	if (num_dim == 1)
		return Contains(poly, Segment<2>(corner1, corner2), proper);

	Point<dim> other_corner = corner;
	other_corner[nonzero_dim] += size[nonzero_dim];

	Point<2> corner3;
	if (!orient.checkContained(other_corner, corner3))
		return false;

	// Create a RotBox<2>

	Vector<2> vec1(corner2 - corner1), vec2(corner3 - corner1);

	RotMatrix<2> m; // A matrix which gives the rotation from the x-axis to vec1

	try {
		m.rotation(Vector<2>(1, 0), vec1);
	}
	catch (const ColinearVectors<2>&) { // vec1 is parallel to (-1, 0), so we're fine
		m.identity();
	}

	RotBox<2> box(corner1, ProdInv(vec2, m), m);

	return Contains(poly, box, proper);
}

template<int dim>
inline bool Contains(const Polygon<dim>& p, const AxisBox<dim>& b, bool proper) {
	return _PolyContainsBox(p.m_orient, p.m_poly, b.m_low, b.m_high - b.m_low, proper);
}

template<int dim>
inline bool Contains(const AxisBox<dim>& b, const Polygon<dim>& p, bool proper) {
	for (size_t i = 0; i < p.m_poly.numCorners(); ++i)
		if (!Contains(b, p.getCorner(i), proper))
			return false;

	return true;
}

template<int dim>
inline bool Intersect(const Polygon<dim>& p, const Ball<dim>& b, bool proper) {
	if (p.m_poly.numCorners() == 0)
		return false;

	Point<2> c2;
	CoordType dist;

	dist = b.m_radius * b.m_radius - p.m_orient.offset(b.m_center, c2).sqrMag();

	if (_Less(dist, 0, proper))
		return false;

	return Intersect(p.m_poly, Ball<2>(c2, std::sqrt(dist)), proper);
}

template<int dim>
inline bool Contains(const Polygon<dim>& p, const Ball<dim>& b, bool proper) {
	if (p.m_poly.numCorners() == 0)
		return false;

	if (b.m_radius > 0)
		return false;

	Point<2> c2;

	if (!p.m_orient.checkContained(b.m_center, c2))
		return false;

	return Contains(p.m_poly, c2, proper);
}

template<int dim>
inline bool Contains(const Ball<dim>& b, const Polygon<dim>& p, bool proper) {
	if (p.m_poly.numCorners() == 0)
		return true;

	Point<2> c2;
	CoordType dist;

	dist = b.m_radius * b.m_radius - p.m_orient.offset(b.m_center, c2).sqrMag();

	if (_Less(dist, 0, proper))
		return false;

	for (size_t i = 0; i != p.m_poly.numCorners(); ++i)
		if (_Less(dist, SquaredDistance(c2, p.m_poly[i]), proper))
			return false;

	return true;
}

template<int dim>
bool Intersect(const Polygon<dim>& p, const Segment<dim>& s, bool proper) {
	if (p.m_poly.numCorners() == 0)
		return false;

	Point<2> p1, p2;
	CoordType d1, d2;
	Vector<dim> v1, v2;

	v1 = p.m_orient.offset(s.m_p1, p1);
	v2 = p.m_orient.offset(s.m_p2, p2);

	if (Dot(v1, v2) > 0) // Both points on same side of sheet
		return false;

	d1 = v1.mag();
	d2 = v2.mag();
	Point<2> p_intersect;

	if (d1 + d2 == 0) // Avoid divide by zero later
		return Intersect(p.m_poly, Segment<2>(p1, p2), proper);

	for (int i = 0; i < 2; ++i)
		p_intersect[i] = (p1[i] * d2 + p2[i] * d1) / (d1 + d2);

	return Intersect(p.m_poly, p_intersect, proper);
}

template<int dim>
inline bool Contains(const Polygon<dim>& p, const Segment<dim>& s, bool proper) {
	if (p.m_poly.numCorners() == 0)
		return false;

	Segment<2> s2;

	if (!p.m_orient.checkContained(s.m_p1, s2.endpoint(0)))
		return false;
	if (!p.m_orient.checkContained(s.m_p2, s2.endpoint(1)))
		return false;

	return Contains(p.m_poly, s2, proper);
}

template<int dim>
inline bool Contains(const Segment<dim>& s, const Polygon<dim>& p, bool proper) {
	if (p.m_poly.numCorners() == 0)
		return true;

	// Expand the basis to include the segment, this deals well with
	// degenerate polygons

	Segment<2> s2;
	Poly2Orient<dim> orient(p.m_orient);

	for (int i = 0; i < 2; ++i)
		if (!orient.expand(s.endpoint(i), s2.endpoint(i)))
			return false;

	return Contains(s2, p.m_poly, proper);
}

template<int dim>
bool Intersect(const Polygon<dim>& p, const RotBox<dim>& r, bool proper) {
	size_t corners = p.m_poly.numCorners();

	if (corners == 0)
		return false;

        Poly2Orient<dim> orient(p.m_orient);
        orient.rotateInverse(r.m_orient, r.m_corner0);

	AxisBox<dim> b(r.m_corner0, r.m_corner0 + r.m_size);

	Point<2> p2;

	if (!orient.checkIntersect(b, p2, proper))
		return false;

	Segment<dim> s;
	s.endpoint(0) = orient.convert(p.m_poly.getCorner(corners - 1));
	int next_end = 1;

	for (size_t i = 0; i < corners; ++i) {
		s.endpoint(next_end) = orient.convert(p.m_poly.getCorner(i));
		if (Intersect(b, s, proper))
			return true;
		next_end = next_end ? 0 : 1;
	}

	return Contains(p, p2, proper);
}

template<int dim>
inline bool Contains(const Polygon<dim>& p, const RotBox<dim>& r, bool proper) {
        Poly2Orient<dim> orient(p.m_orient);
        orient.rotateInverse(r.m_orient, r.m_corner0);

	return _PolyContainsBox(orient, p.m_poly, r.m_corner0, r.m_size, proper);
}

template<int dim>
inline bool Contains(const RotBox<dim>& r, const Polygon<dim>& p, bool proper) {
	if (p.m_poly.numCorners() == 0)
		return true;

	AxisBox<dim> b(r.m_corner0, r.m_corner0 + r.m_size);

        Poly2Orient<dim> orient(p.m_orient);
        orient.rotateInverse(r.m_orient, r.m_corner0);

	for (size_t i = 0; i < p.m_poly.numCorners(); ++i)
		if (!Contains(b, orient.convert(p.m_poly[i]), proper))
			return false;

	return true;
}

bool PolyPolyIntersect(const Polygon<2>& poly1, const Polygon<2>& poly2,
					   int intersect_dim,
					   const Poly2OrientIntersectData& data, bool proper);

template<int dim>
inline bool Intersect(const Polygon<dim>& p1, const Polygon<dim>& p2, bool proper) {
	Poly2OrientIntersectData data;

	int intersect_dim = Intersect(p1.m_orient, p2.m_orient, data);

	return PolyPolyIntersect(p1.m_poly, p2.m_poly, intersect_dim, data, proper);
}

bool PolyPolyContains(const Polygon<2>& outer, const Polygon<2>& inner,
					  int intersect_dim,
					  const Poly2OrientIntersectData& data, bool proper);

template<int dim>
inline bool Contains(const Polygon<dim>& outer, const Polygon<dim>& inner, bool proper) {
	if (outer.m_poly.numCorners() == 0)
		return !proper && inner.m_poly.numCorners() == 0;

	if (inner.m_poly.numCorners() == 0)
		return true;

	Poly2OrientIntersectData data;

	int intersect_dim = Intersect(outer.m_orient, inner.m_orient, data);

	return PolyPolyContains(outer.m_poly, inner.m_poly, intersect_dim, data, proper);
}

// instantiations, only need 3d because 2d is a specialization,
// except for the reverse-order intersect

template bool Intersect<Point<2>, Polygon<2> >(const Point<2>&, const Polygon<2>&, bool);

template bool Intersect<Point<3>, Polygon<3> >(const Point<3>&, const Polygon<3>&, bool);

template bool Contains<3>(const Point<3>&, const Polygon<3>&, bool);

template bool Intersect<3>(const Polygon<3>&, const Point<3>&, bool);

template bool Contains<3>(const Polygon<3>&, const Point<3>&, bool);

template bool Intersect<AxisBox<2>, Polygon<2> >(const AxisBox<2>&, const Polygon<2>&, bool);

template bool Intersect<AxisBox<3>, Polygon<3> >(const AxisBox<3>&, const Polygon<3>&, bool);

template bool Contains<3>(const AxisBox<3>&, const Polygon<3>&, bool);

template bool Intersect<3>(const Polygon<3>&, const AxisBox<3>&, bool);

template bool Contains<3>(const Polygon<3>&, const AxisBox<3>&, bool);

template bool Intersect<Ball<2>, Polygon<2> >(const Ball<2>&, const Polygon<2>&, bool);

template bool Intersect<Ball<3>, Polygon<3> >(const Ball<3>&, const Polygon<3>&, bool);

template bool Contains<3>(const Ball<3>&, const Polygon<3>&, bool);

template bool Intersect<3>(const Polygon<3>&, const Ball<3>&, bool);

template bool Contains<3>(const Polygon<3>&, const Ball<3>&, bool);

template bool Intersect<Segment<2>, Polygon<2> >(const Segment<2>&, const Polygon<2>&, bool);

template bool Intersect<Segment<3>, Polygon<3> >(const Segment<3>&, const Polygon<3>&, bool);

template bool Contains<3>(const Segment<3>&, const Polygon<3>&, bool);

template bool Intersect<3>(const Polygon<3>&, const Segment<3>&, bool);

template bool Contains<3>(const Polygon<3>&, const Segment<3>&, bool);

template bool Intersect<RotBox<2>, Polygon<2> >(const RotBox<2>&, const Polygon<2>&, bool);

template bool Intersect<RotBox<3>, Polygon<3> >(const RotBox<3>&, const Polygon<3>&, bool);

template bool Contains<3>(const RotBox<3>&, const Polygon<3>&, bool);

template bool Intersect<3>(const Polygon<3>&, const RotBox<3>&, bool);

template bool Contains<3>(const Polygon<3>&, const RotBox<3>&, bool);

template bool Intersect<3>(const Polygon<3>&, const Polygon<3>&, bool);

template bool Contains<3>(const Polygon<3>&, const Polygon<3>&, bool);

template<>
bool Poly2Orient<3>::checkIntersectPlane(const AxisBox<3>& b, Point<2>& p2,
										 bool proper) const {
	assert("This function should only be called if the orientation represents a plane" &&
		   m_origin.isValid() && m_axes[0].isValid() && m_axes[1].isValid());

	Vector<3> normal = Cross(m_axes[0], m_axes[1]); // normal to the plane

//  enum {
//    AXIS_UP,
//    AXIS_DOWN,
//    AXIS_FLAT
//  } axis_direction[3];

	CoordType normal_mag = normal.sloppyMag();
	int high_corner_num = 0;

	for (int i = 0; i < 3; ++i) {
		if (std::fabs(normal[i]) < normal_mag * numeric_constants<CoordType>::epsilon()) {
//      axis_direction[i] = AXIS_FLAT;
		} else if (normal[i] > 0) {
//      axis_direction[i] = AXIS_UP;
			high_corner_num |= (1 << i);
		}
//    else
//      axis_direction[i] = AXIS_DOWN;
	}

	int low_corner_num = high_corner_num ^ 7;

	Point<3> high_corner = b.getCorner(high_corner_num);
	Point<3> low_corner = b.getCorner(low_corner_num);

	// If these are on opposite sides of the plane, we have an intersection

	CoordType perp_size = Dot(normal, high_corner - low_corner) / normal_mag;
	assert(perp_size >= 0);

	if (perp_size < normal_mag * numeric_constants<CoordType>::epsilon()) {
		// We have a very flat box, lying parallel to the plane
		return !proper && checkContained(Midpoint(high_corner, low_corner), p2);
	}

	if (_Less(Dot(high_corner - m_origin, normal), 0, proper)
		|| _Less(Dot(low_corner - m_origin, normal), 0, proper))
		return false; // box lies above or below the plane

	// Find the intersection of the line through the corners with the plane

	Point<2> p2_high, p2_low;

	CoordType high_dist = offset(high_corner, p2_high).mag();
	CoordType low_dist = offset(low_corner, p2_low).mag();

	p2 = Midpoint(p2_high, p2_low, high_dist / (high_dist + low_dist));

	return true;
}

// This assumes the y coordinates of the points are all zero
static void LinePolyGetBounds(const Polygon<2>& poly, CoordType& low, CoordType& high) {
	low = high = poly[0][0];

	for (size_t i = 0; i < poly.numCorners(); ++i) {
		CoordType val = poly[i][0];
		if (val < low)
			low = val;
		if (val > high)
			high = val;
	}
}

// For use in GetCrossings()
struct LinePointData {
	CoordType low, high;
	bool cross;
};

// This finds the intervals where the polygon intersects the line
// through p parallel to v, and puts the endpoints of those
// intervals in the vector "cross"
static bool GetCrossings(const Polygon<2>& poly, const Point<2>& p,
						 const Vector<2>& v, std::vector<CoordType>& cross,
						 bool proper) {
	assert(poly.numCorners() == cross.size()); // Already allocated
	assert(Equal(v.sqrMag(), 1));

	// The sign of the cross product changes when you cross the line
	Point<2> old_p = poly.getCorner(poly.numCorners() - 1);
	bool old_below = (Cross(v, old_p - p) < 0);
	int next_cross = 0;

	// Stuff for when multiple sequential corners lie on the line
	std::list<LinePointData> line_point_data;

	for (size_t i = 0; i < poly.numCorners(); ++i) {
		Point<2> p_i = poly.getCorner(i);
		Vector<2> v_i = p_i - p;

		CoordType v_i_sqr_mag = v_i.sqrMag(), proj = Dot(v_i, v);

		if (Equal(v_i_sqr_mag, proj * proj)) { // corner lies on line
			Point<2> p_j;
			Vector<2> v_j;
			CoordType proj_j, low_proj = proj, high_proj = proj;
			size_t j;
			for (j = i + 1; j != i; j == poly.numCorners() - 1 ? j = 0 : ++j) {
				p_j = poly.getCorner(j);
				v_j = p_j - p;
				proj_j = Dot(v_j, v);

				if (!Equal(v_j.sqrMag(), proj_j * proj_j))
					break;

				if (proj_j < low_proj)
					low_proj = proj_j;
				if (proj_j > high_proj)
					high_proj = proj_j;
			}

			assert(j != i); // We know that the polygon spans a 2d space

			bool below = (Cross(v, v_j) < 0);

			if (below == old_below && proper) {
				old_p = p_j;
				continue;
			}

			if (j == i + 1) { // just one point on the line

				if (below != old_below) {
					old_below = below;
					cross[next_cross++] = proj;
				} else {
					assert(!proper);
					// Just touches, adding it twice will give a zero length "hit" region
					cross[next_cross++] = proj;
					cross[next_cross++] = proj;
				}

				old_p = p_j;
				continue;
			}

			LinePointData data = {low_proj, high_proj, below != old_below};

			std::list<LinePointData>::iterator I;

			for (I = line_point_data.begin(); I != line_point_data.end(); ++I) {
				if (data.low > I->high)
					continue;

				if (data.high < I->low) {
					line_point_data.insert(I, data);
					break;
				}

				// overlap

				I->low = (I->low < data.low) ? I->low : data.low;
				I->high = (I->high > data.high) ? I->high : data.high;
				I->cross = (I->cross != data.cross);

				auto J = I;

				++J;

				if (J->low < I->high) {
					I->high = J->high;
					I->cross = (I->cross != J->cross);
					line_point_data.erase(J);
				}
			}

			if (I == line_point_data.end())
				line_point_data.push_back(data);

			old_below = below;
			old_p = p_j;
			continue;
		}

		// the corner doesn't lie on the line, compute the intersection point

		bool below = (Cross(v, v_i) < 0);

		if (below != old_below) {
			old_below = below;
			Vector<2> dist = p - old_p;
			CoordType dist_sqr_mag = dist.sqrMag();
			CoordType dist_proj = Dot(dist, v);

			CoordType denom = dist_proj * dist_proj - dist_sqr_mag;

			assert(denom != 0); // We got a crossing, the vectors can't be parallel

			CoordType line_pos = (dist_proj * Dot(v_i, dist) + dist_sqr_mag * proj) / denom;

			cross[next_cross++] = line_pos;
		}

		old_p = p;
	}

	cross.resize(next_cross);
	std::sort(cross.begin(), cross.end());

	if (!line_point_data.empty()) {
		auto I = line_point_data.begin();
		auto cross_num = cross.begin();
		bool hit = false;

		while (cross_num != cross.end() && I != line_point_data.end()) {
			if (*cross_num < I->low) {
				++cross_num;
				hit = !hit;
				continue;
			}

			bool hit_between;
			if (*cross_num > I->high) {
				hit_between = I->cross;
			} else {
				auto high_cross_num = cross_num;

				do {
					++high_cross_num;
				} while (*high_cross_num < I->high);

				hit_between = (((high_cross_num - cross_num) % 2) != 0) != I->cross;

				cross_num = cross.erase(cross_num, high_cross_num);
			}

			if (hit_between) {
				cross_num = cross.insert(cross_num, proper == hit ? I->low : I->high);
				++cross_num;
				hit = !hit;
			} else if (!proper) {
				cross_num = cross.insert(cross_num, I->low);
				++cross_num;
				cross_num = cross.insert(cross_num, I->high);
				++cross_num;
			}
			++I;
		}

		while (I != line_point_data.end()) {
			if (I->cross) {
				cross.push_back(proper == hit ? I->low : I->high);
				hit = !hit;
			} else if (!proper) {
				cross.push_back(I->low);
				cross.push_back(I->high);
			}
			++I;
		}

		assert(!hit); // end outside the polygon
	}

	return !cross.empty();
}

bool PolyPolyIntersect(const Polygon<2>& poly1, const Polygon<2>& poly2,
					   const int intersect_dim,
					   const Poly2OrientIntersectData& data, bool proper) {
	switch (intersect_dim) {
		case -1:
			return false;
		case 0:
			return Intersect(poly1, data.p1, proper)
				   && Intersect(poly2, data.p2, proper);
		case 1:
			if (proper && (data.o1_is_line || data.o2_is_line))
				return false;

			if (data.o1_is_line && data.o2_is_line) {
				assert(!proper);
				CoordType low1, low2, high1, high2;

				LinePolyGetBounds(poly1, low1, high1);

				low1 -= data.p1[0];
				high1 -= data.p1[0];

				if (data.v1[0] < 0) { // v1 = (-1, 0)
					CoordType tmp = low1;
					low1 = -high1;
					high1 = -tmp;
				}

				LinePolyGetBounds(poly2, low2, high2);

				low2 -= data.p2[0];
				high2 -= data.p2[0];

				if (data.v2[0] < 0) { // v2 = (-1, 0)
					CoordType tmp = low2;
					low2 = -high2;
					high2 = -tmp;
				}

				return high1 >= low2 && high2 >= low1;
			}

			if (data.o1_is_line) {
				assert(!proper);
				CoordType min, max;
				LinePolyGetBounds(poly1, min, max);

				min -= data.p1[0];
				max -= data.p1[0];

				if (data.v1[0] < 0) { // v1 = (-1, 0)
					CoordType tmp = min;
					min = -max;
					max = -tmp;
				}

				Segment<2> s(data.p2 + min * data.v2, data.p1 + max * data.v2);

				return Intersect(poly2, s, false);
			}

			if (data.o2_is_line) {
				assert(!proper);
				CoordType min, max;
				LinePolyGetBounds(poly2, min, max);

				min -= data.p2[0];
				max -= data.p2[0];

				if (data.v2[0] < 0) { // v2 = (-1, 0)
					CoordType tmp = min;
					min = -max;
					max = -tmp;
				}

				Segment<2> s(data.p1 + min * data.v1, data.p1 + max * data.v1);

				return Intersect(poly1, s, false);
			}

			{
				std::vector<CoordType> cross1(poly1.numCorners());
				if (!GetCrossings(poly1, data.p1, data.v1, cross1, proper))
					return false; // line misses polygon

				std::vector<CoordType> cross2(poly2.numCorners());
				if (!GetCrossings(poly2, data.p2, data.v2, cross2, proper))
					return false; // line misses polygon

				auto i1 = cross1.begin(), i2 = cross2.begin();
				bool hit1 = false, hit2 = false;

				while (i1 != cross1.end() && i2 != cross2.end()) {
					if (Equal(*i1, *i2)) {
						if (!proper)
							return true;

						hit1 = !hit1;
						++i1;
						hit2 = !hit2;
						++i2;
					}

					if (*i1 < *i2) {
						hit1 = !hit1;
						++i1;
					} else {
						hit2 = !hit2;
						++i2;
					}

					if (hit1 && hit2)
						return true;
				}

				return false;
			}
		case 2:
			// Shift one polygon into the other's coordinates.
			// Perhaps not the most efficient, but this is a
			// rare special case.
		{
			Polygon<2> tmp_poly(poly2);

			for (size_t i = 0; i < tmp_poly.numCorners(); ++i) {
				Point<2>& p = tmp_poly[i];
				Point<2> shift_p = p + data.off;

				p[0] = shift_p[0] * data.v1[0] + shift_p[1] * data.v2[0];
				p[1] = shift_p[0] * data.v1[1] + shift_p[1] * data.v2[1];
			}

			return Intersect(poly1, tmp_poly, proper);
		}
		default:
			assert(false);
			return false;
	}
}

bool PolyPolyContains(const Polygon<2>& outer, const Polygon<2>& inner,
					  const int intersect_dim,
					  const Poly2OrientIntersectData& data, bool proper) {
	switch (intersect_dim) {
		case -1:
			return false;
		case 0:
			return Contains(data.p2, inner, false)
				   && Contains(outer, data.p1, proper);
		case 1:
			if (!data.o2_is_line) // The inner poly isn't contained by the intersect line
				return false;

			// The inner poly lies on a line, so it reduces to a line segment
			{
				CoordType min, max;
				LinePolyGetBounds(inner, min, max);

				min -= data.p2[0];
				max -= data.p2[0];

				if (data.v2[0] < 0) { // v2 = (-1, 0)
					CoordType tmp = min;
					min = -max;
					max = -tmp;
				}

				Segment<2> s(data.p1 + min * data.v1, data.p1 + max * data.v1);

				return Contains(outer, s, proper);
			}
		case 2:
			// Shift one polygon into the other's coordinates.
			// Perhaps not the most efficient, but this is a
			// rare special case.
		{
			Polygon<2> tmp_poly(inner);

			for (size_t i = 0; i < tmp_poly.numCorners(); ++i) {
				Point<2>& p = tmp_poly[i];
				Point<2> shift_p = p + data.off;

				p[0] = shift_p[0] * data.v1[0] + shift_p[1] * data.v2[0];
				p[1] = shift_p[0] * data.v1[1] + shift_p[1] * data.v2[1];
			}

			return Contains(outer, tmp_poly, proper);
		}
		default:
			assert(false);
			return false;
	}
}

// Polygon<2> intersection functions

// FIXME deal with round off error in _all_ these intersection functions

// The Polygon<2>/Point<2> intersection function was stolen directly
// from shape.cpp in libCoal

template<>
bool Intersect<2>(const Polygon<2>& r, const Point<2>& p, bool proper) {
	const Polygon<2>::theConstIter begin = r.m_points.begin(), end = r.m_points.end();
	bool hit = false;

        for (Polygon<2>::theConstIter i = begin, j = end - 1; i != end; j = i++) {
                bool vertically_between =
                                ((_LessEq((*i)[1], p[1], false) && _Less(p[1], (*j)[1], false)) ||
                                 (_LessEq((*j)[1], p[1], false) && _Less(p[1], (*i)[1], false)));

                if (!vertically_between)
                        continue;

                CoordType ydiff = (*j)[1] - (*i)[1];
                if (std::fabs(ydiff) <= numeric_constants<CoordType>::epsilon())
                        continue;
                CoordType x_intersect = (*i)[0] + ((*j)[0] - (*i)[0])
                                                                                  * (p[1] - (*i)[1]) / ydiff;

                if (Equal(p[0], x_intersect))
                        return !proper;

                if (_Less(p[0], x_intersect, false))
                        hit = !hit;
        }

	return hit;
}

template<>
bool Contains<2>(const Point<2>& p, const Polygon<2>& r, bool proper) {
	if (proper) // Weird degenerate case
		return r.numCorners() == 0;

	for (const auto& point: r.m_points)
		if (p != point)
			return false;

	return true;
}

template<>
bool Intersect<2>(const Polygon<2>& p, const AxisBox<2>& b, bool proper) {
	const Polygon<2>::theConstIter begin = p.m_points.begin(), end = p.m_points.end();
	bool hit = false;

        for (Polygon<2>::theConstIter i = begin, j = end - 1; i != end; j = i++) {
                bool low_vertically_between =
                                ((_LessEq((*i)[1], b.m_low[1], false) && _Less(b.m_low[1], (*j)[1], false)) ||
                                 (_LessEq((*j)[1], b.m_low[1], false) && _Less(b.m_low[1], (*i)[1], false)));
                bool low_horizontally_between =
                                ((_LessEq((*i)[0], b.m_low[0], false) && _Less(b.m_low[0], (*j)[0], false)) ||
                                 (_LessEq((*j)[0], b.m_low[0], false) && _Less(b.m_low[0], (*i)[0], false)));
                bool high_vertically_between =
                                ((_LessEq((*i)[1], b.m_high[1], false) && _Less(b.m_high[1], (*j)[1], false)) ||
                                 (_LessEq((*j)[1], b.m_high[1], false) && _Less(b.m_high[1], (*i)[1], false)));
                bool high_horizontally_between =
                                ((_LessEq((*i)[0], b.m_high[0], false) && _Less(b.m_high[0], (*j)[0], false)) ||
                                 (_LessEq((*j)[0], b.m_high[0], false) && _Less(b.m_high[0], (*i)[0], false)));

		CoordType xdiff = ((*j)[0] - (*i)[0]);
		CoordType ydiff = ((*j)[1] - (*i)[1]);

                if (low_vertically_between) { // Check for edge intersect
                        if (std::fabs(ydiff) <= numeric_constants<CoordType>::epsilon())
                                continue;
                        CoordType x_intersect = (*i)[0] + (b.m_low[1] - (*i)[1])
                                                                                          * xdiff / ydiff;

                        if (Equal(b.m_low[0], x_intersect) || Equal(b.m_high[0], x_intersect))
                                return !proper;
                        if (_Less(b.m_low[0], x_intersect, false) && _Greater(b.m_high[0], x_intersect, false))
                                return true;

                        // Also check for point inclusion here, only need to do this for one point
                        if (_Less(b.m_low[0], x_intersect, false))
                                hit = !hit;
		}

                if (low_horizontally_between) { // Check for edge intersect
                        if (std::fabs(xdiff) <= numeric_constants<CoordType>::epsilon())
                                continue;
                        CoordType y_intersect = (*i)[1] + (b.m_low[0] - (*i)[0])
                                                                                          * ydiff / xdiff;

                        if (Equal(b.m_low[1], y_intersect) || Equal(b.m_high[1], y_intersect))
                                return !proper;
                        if (_Less(b.m_low[1], y_intersect, false) && _Greater(b.m_high[1], y_intersect, false))
                                return true;
		}

                if (high_vertically_between) { // Check for edge intersect
                        if (std::fabs(ydiff) <= numeric_constants<CoordType>::epsilon())
                                continue;
                        CoordType x_intersect = (*i)[0] + (b.m_high[1] - (*i)[1])
                                                                                          * xdiff / ydiff;

                        if (Equal(b.m_low[0], x_intersect) || Equal(b.m_high[0], x_intersect))
                                return !proper;
                        if (_Less(b.m_low[0], x_intersect, false) && _Greater(b.m_high[0], x_intersect, false))
                                return true;
		}

                if (high_horizontally_between) { // Check for edge intersect
                        if (std::fabs(xdiff) <= numeric_constants<CoordType>::epsilon())
                                continue;
                        CoordType y_intersect = (*i)[1] + (b.m_high[0] - (*i)[0])
                                                                                          * ydiff / xdiff;

                        if (Equal(b.m_low[1], y_intersect) || Equal(b.m_high[1], y_intersect))
                                return !proper;
                        if (_Less(b.m_low[1], y_intersect, false) && _Greater(b.m_high[1], y_intersect, false))
                                return true;
		}
	}

	return hit;
}

template<>
bool Contains<2>(const Polygon<2>& p, const AxisBox<2>& b, bool proper) {
	const Polygon<2>::theConstIter begin = p.m_points.begin(), end = p.m_points.end();
	bool hit = false;

        for (Polygon<2>::theConstIter i = begin, j = end - 1; i != end; j = i++) {
                bool low_vertically_between =
                                ((_LessEq((*i)[1], b.m_low[1], false) && _Less(b.m_low[1], (*j)[1], false)) ||
                                 (_LessEq((*j)[1], b.m_low[1], false) && _Less(b.m_low[1], (*i)[1], false)));
                bool low_horizontally_between =
                                ((_LessEq((*i)[0], b.m_low[0], false) && _Less(b.m_low[0], (*j)[0], false)) ||
                                 (_LessEq((*j)[0], b.m_low[0], false) && _Less(b.m_low[0], (*i)[0], false)));
                bool high_vertically_between =
                                ((_LessEq((*i)[1], b.m_high[1], false) && _Less(b.m_high[1], (*j)[1], false)) ||
                                 (_LessEq((*j)[1], b.m_high[1], false) && _Less(b.m_high[1], (*i)[1], false)));
                bool high_horizontally_between =
                                ((_LessEq((*i)[0], b.m_high[0], false) && _Less(b.m_high[0], (*j)[0], false)) ||
                                 (_LessEq((*j)[0], b.m_high[0], false) && _Less(b.m_high[0], (*i)[0], false)));

		CoordType xdiff = ((*j)[0] - (*i)[0]);
		CoordType ydiff = ((*j)[1] - (*i)[1]);

                if (low_vertically_between) { // Check for edge intersect
                        if (std::fabs(ydiff) <= numeric_constants<CoordType>::epsilon())
                                continue;
                        CoordType x_intersect = (*i)[0] + (b.m_low[1] - (*i)[1])
                                                                                          * xdiff / ydiff;

                        bool on_corner = Equal(b.m_low[0], x_intersect)
                                                         || Equal(b.m_high[0], x_intersect);

                        if (on_corner && proper)
                                return false;

                        if (!on_corner && _Less(b.m_low[0], x_intersect, false)
                                        && _Greater(b.m_high[0], x_intersect, false))
                                return false;

                        // Also check for point inclusion here, only need to do this for one point
                        if (!on_corner && _Less(b.m_low[0], x_intersect, false))
                                hit = !hit;
		}

                if (low_horizontally_between) { // Check for edge intersect
                        if (std::fabs(xdiff) <= numeric_constants<CoordType>::epsilon())
                                continue;
                        CoordType y_intersect = (*i)[1] + (b.m_low[0] - (*i)[0])
                                                                                          * ydiff / xdiff;

                        bool on_corner = Equal(b.m_low[1], y_intersect)
                                                         || Equal(b.m_high[1], y_intersect);

                        if (on_corner && proper)
                                return false;

                        if (!on_corner && _Less(b.m_low[1], y_intersect, false)
                                        && _Greater(b.m_high[1], y_intersect, false))
                                return false;
		}

                if (high_vertically_between) { // Check for edge intersect
                        if (std::fabs(ydiff) <= numeric_constants<CoordType>::epsilon())
                                continue;
                        CoordType x_intersect = (*i)[0] + (b.m_high[1] - (*i)[1])
                                                                                          * xdiff / ydiff;

                        bool on_corner = Equal(b.m_low[0], x_intersect)
                                                         || Equal(b.m_high[0], x_intersect);

                        if (on_corner && proper)
                                return false;

                        if (!on_corner && _Less(b.m_low[0], x_intersect, false)
                                        && _Greater(b.m_high[0], x_intersect, false))
                                return false;
		}

                if (high_horizontally_between) { // Check for edge intersect
                        if (std::fabs(xdiff) <= numeric_constants<CoordType>::epsilon())
                                continue;
                        CoordType y_intersect = (*i)[1] + (b.m_high[0] - (*i)[0])
                                                                                          * ydiff / xdiff;

                        bool on_corner = Equal(b.m_low[1], y_intersect)
                                                         || Equal(b.m_high[1], y_intersect);

                        if (on_corner && proper)
                                return false;

                        if (!on_corner && _Less(b.m_low[1], y_intersect, false)
                                        && _Greater(b.m_high[1], y_intersect, false))
                                return false;
		}
	}

	return hit;
}

template<>
bool Contains<2>(const AxisBox<2>& b, const Polygon<2>& p, bool proper) {
	for (const auto& point: p.m_points)
		if (!Contains(b, point, proper))
			return false;

	return true;
}

template<>
bool Intersect<2>(const Polygon<2>& p, const Ball<2>& b, bool proper) {
	if (Contains(p, b.m_center, proper))
		return true;

	Segment<2> s2;
	s2.endpoint(0) = p.m_points.back();
	int next_end = 1;

	for (const auto& point: p.m_points) {
		s2.endpoint(next_end) = point;
		if (Intersect(s2, b, proper))
			return true;
		next_end = next_end ? 0 : 1;
	}

	return false;
}

template<>
bool Contains<2>(const Polygon<2>& p, const Ball<2>& b, bool proper) {
	if (!Contains(p, b.m_center, proper))
		return false;

	Segment<2> s2;
	s2.endpoint(0) = p.m_points.back();
	int next_end = 1;

	for (const auto& point: p.m_points) {
		s2.endpoint(next_end) = point;
		if (Intersect(s2, b, !proper))
			return false;
		next_end = next_end ? 0 : 1;
	}

	return true;
}

template<>
bool Contains<2>(const Ball<2>& b, const Polygon<2>& p, bool proper) {
	CoordType sqr_dist = b.m_radius * b.m_radius;

	for (const auto& point: p.m_points)
		if (_Greater(SquaredDistance(b.m_center, point), sqr_dist, proper))
			return false;

	return true;
}

template<>
bool Intersect<2>(const Polygon<2>& p, const Segment<2>& s, bool proper) {
	if (Contains(p, s.endpoint(0), proper))
		return true;

	const Polygon<2>::theConstIter begin = p.m_points.begin(), end = p.m_points.end();

	Segment<2> s2;

	s2.endpoint(0) = p.m_points.back();
	int next_point = 1;

	for (Polygon<2>::theConstIter i = begin; i != end; ++i) {
		s2.endpoint(next_point) = *i;
		if (Intersect(s, s2, proper))
			return true;
		next_point = next_point ? 0 : 1;
	}

	return false;
}

template<>
bool Contains<2>(const Polygon<2>& p, const Segment<2>& s, bool proper) {
	if (proper && !Contains(p, s.endpoint(0), true))
		return false;

	const Polygon<2>::theConstIter begin = p.m_points.begin(), end = p.m_points.end();

	Segment<2> s2;

	s2.endpoint(0) = p.m_points.back();
	int next_point = 1;
	bool hit = false;

	for (Polygon<2>::theConstIter i = begin; i != end; ++i) {
		s2.endpoint(next_point) = *i;
		if (Intersect(s2, s, !proper))
			return false;
		bool this_point = next_point;
		next_point = next_point ? 0 : 1;
		if (proper)
			continue;

		// Check for crossing at an endpoint
		if (Contains(s, *i, false) && (*i != s.m_p2)) {
			Vector<2> segment = s.m_p2 - s.m_p1;
			Vector<2> edge1 = *i - s2.endpoint(next_point); // Gives prev point in this case
			Vector<2> edge2 = *i - *(i + 1);

			CoordType c1 = Cross(segment, edge1), c2 = Cross(segment, edge2);

			if (c1 * c2 < 0) { // opposite sides
				if (*i == s.m_p1) { // really a containment issue
					if (edge1[1] * edge2[1] > 0 // Edges either both up or both down
						|| ((edge1[1] > 0) ? c1 : c2) < 0) // segment lies to the left
						hit = !hit;
					continue; // Already checked containment for this point
				} else
					return false;
			}
		}

		// Check containment of one endpoint

		// next_point also gives prev_point
		bool vertically_between =
				((s2.endpoint(this_point)[1] <= s.m_p1[1]
				  && s.m_p1[1] < s2.endpoint(next_point)[1]) ||
				 (s2.endpoint(next_point)[1] <= s.m_p1[1]
				  && s.m_p1[1] < s2.endpoint(this_point)[1]));

		if (!vertically_between)
			continue;

		CoordType x_intersect = s2.m_p1[0] + (s2.m_p2[0] - s2.m_p1[0])
											 * (s.m_p1[1] - s2.m_p1[1])
											 / (s2.m_p2[1] - s2.m_p1[1]);

		if (Equal(s.m_p1[0], x_intersect)) { // Figure out which side the segment's on

			// Equal points are handled in the crossing routine above
			if (s2.endpoint(next_point) == s.m_p1)
				continue;
			assert(s2.endpoint(this_point) != s.m_p1);

			Vector<2> poly_edge = (s2.m_p1[1] < s2.m_p2[1]) ? (s2.m_p2 - s2.m_p1)
															: (s2.m_p1 - s2.m_p2);
			Vector<2> segment = s.m_p2 - s.m_p1;

                        if (Cross(segment, poly_edge) < 0)
                                hit = !hit;
                } else if (_Less(s.m_p1[0], x_intersect, false))
                        hit = !hit;
        }

	return proper || hit;
}

template<>
bool Contains<2>(const Segment<2>& s, const Polygon<2>& p, bool proper) {
	for (const auto& point: p.m_points)
		if (!Contains(s, point, proper))
			return false;

	return true;
}

template<>
bool Intersect<2>(const Polygon<2>& p, const RotBox<2>& r, bool proper) {
	CoordType m_low[2], m_high[2];

	for (int j = 0; j < 2; ++j) {
		if (r.m_size[j] > 0) {
			m_low[j] = r.m_corner0[j];
			m_high[j] = r.m_corner0[j] + r.m_size[j];
		} else {
			m_high[j] = r.m_corner0[j];
			m_low[j] = r.m_corner0[j] + r.m_size[j];
		}
	}

	Point<2> ends[2];
        ends[0] = p.m_points.back();
        ends[0].rotateInverse(r.m_orient, r.m_corner0);
	int next_end = 1;

	const Polygon<2>::theConstIter begin = p.m_points.begin(), end = p.m_points.end();
	bool hit = false;

	for (Polygon<2>::theConstIter i = begin; i != end; ++i) {
                ends[next_end] = *i;
                ends[next_end].rotateInverse(r.m_orient, r.m_corner0);
		next_end = next_end ? 0 : 1;

                bool low_vertically_between =
                                ((_LessEq((ends[0])[1], m_low[1], false) && _Less(m_low[1], (ends[1])[1], false)) ||
                                 (_LessEq((ends[1])[1], m_low[1], false) && _Less(m_low[1], (ends[0])[1], false)));
                bool low_horizontally_between =
                                ((_LessEq((ends[0])[0], m_low[0], false) && _Less(m_low[0], (ends[1])[0], false)) ||
                                 (_LessEq((ends[1])[0], m_low[0], false) && _Less(m_low[0], (ends[0])[0], false)));
                bool high_vertically_between =
                                ((_LessEq((ends[0])[1], m_high[1], false) && _Less(m_high[1], (ends[1])[1], false)) ||
                                 (_LessEq((ends[1])[1], m_high[1], false) && _Less(m_high[1], (ends[0])[1], false)));
                bool high_horizontally_between =
                                ((_LessEq((ends[0])[0], m_high[0], false) && _Less(m_high[0], (ends[1])[0], false)) ||
                                 (_LessEq((ends[1])[0], m_high[0], false) && _Less(m_high[0], (ends[0])[0], false)));

		CoordType xdiff = (ends[1])[0] - (ends[0])[0];
		CoordType ydiff = (ends[1])[1] - (ends[0])[1];

		if (low_vertically_between) { // Check for edge intersect
			CoordType x_intersect = (ends[0])[0] + (m_low[1] - (ends[0])[1])
												   * xdiff / ydiff;

                        if (Equal(m_low[0], x_intersect) || Equal(m_high[0], x_intersect))
                                return !proper;
                        if (_Less(m_low[0], x_intersect, false) && _Greater(m_high[0], x_intersect, false))
                                return true;

                        // Also check for point inclusion here, only need to do this for one point
                        if (_Less(m_low[0], x_intersect, false))
                                hit = !hit;
		}

		if (low_horizontally_between) { // Check for edge intersect
			CoordType y_intersect = (ends[0])[1] + (m_low[0] - (ends[0])[0])
												   * ydiff / xdiff;

                        if (Equal(m_low[1], y_intersect) || Equal(m_high[1], y_intersect))
                                return !proper;
                        if (_Less(m_low[1], y_intersect, false) && _Greater(m_high[1], y_intersect, false))
                                return true;
		}

		if (high_vertically_between) { // Check for edge intersect
			CoordType x_intersect = (ends[0])[0] + (m_high[1] - (ends[0])[1])
												   * xdiff / ydiff;

                        if (Equal(m_low[0], x_intersect) || Equal(m_high[0], x_intersect))
                                return !proper;
                        if (_Less(m_low[0], x_intersect, false) && _Greater(m_high[0], x_intersect, false))
                                return true;
		}

		if (high_horizontally_between) { // Check for edge intersect
			CoordType y_intersect = (ends[0])[1] + (m_high[0] - (ends[0])[0])
												   * ydiff / xdiff;

                        if (Equal(m_low[1], y_intersect) || Equal(m_high[1], y_intersect))
                                return !proper;
                        if (_Less(m_low[1], y_intersect, false) && _Greater(m_high[1], y_intersect, false))
                                return true;
		}
	}

	return hit;
}

template<>
bool Contains<2>(const Polygon<2>& p, const RotBox<2>& r, bool proper) {
	CoordType m_low[2], m_high[2];

	for (int j = 0; j < 2; ++j) {
		if (r.m_size[j] > 0) {
			m_low[j] = r.m_corner0[j];
			m_high[j] = r.m_corner0[j] + r.m_size[j];
		} else {
			m_high[j] = r.m_corner0[j];
			m_low[j] = r.m_corner0[j] + r.m_size[j];
		}
	}

        Point<2> ends[2];
        ends[0] = p.m_points.back();
        ends[0].rotateInverse(r.m_orient, r.m_corner0);
        int next_end = 1;

	const Polygon<2>::theConstIter begin = p.m_points.begin(), end = p.m_points.end();
	bool hit = false;

	for (Polygon<2>::theConstIter i = begin; i != end; ++i) {
                ends[next_end] = *i;
                ends[next_end].rotateInverse(r.m_orient, r.m_corner0);
		next_end = next_end ? 0 : 1;

                bool low_vertically_between =
                                ((_LessEq((ends[0])[1], m_low[1], false) && _Less(m_low[1], (ends[1])[1], false)) ||
                                 (_LessEq((ends[1])[1], m_low[1], false) && _Less(m_low[1], (ends[0])[1], false)));
                bool low_horizontally_between =
                                ((_LessEq((ends[0])[0], m_low[0], false) && _Less(m_low[0], (ends[1])[0], false)) ||
                                 (_LessEq((ends[1])[0], m_low[0], false) && _Less(m_low[0], (ends[0])[0], false)));
                bool high_vertically_between =
                                ((_LessEq((ends[0])[1], m_high[1], false) && _Less(m_high[1], (ends[1])[1], false)) ||
                                 (_LessEq((ends[1])[1], m_high[1], false) && _Less(m_high[1], (ends[0])[1], false)));
                bool high_horizontally_between =
                                ((_LessEq((ends[0])[0], m_high[0], false) && _Less(m_high[0], (ends[1])[0], false)) ||
                                 (_LessEq((ends[1])[0], m_high[0], false) && _Less(m_high[0], (ends[0])[0], false)));

		CoordType xdiff = (ends[1])[0] - (ends[0])[0];
		CoordType ydiff = (ends[1])[1] - (ends[0])[1];

		if (low_vertically_between) { // Check for edge intersect
			CoordType x_intersect = (ends[0])[0] + (m_low[1] - (ends[0])[1])
												   * xdiff / ydiff;

                        bool on_corner = Equal(m_low[0], x_intersect)
                                                         || Equal(m_high[0], x_intersect);

                        if (on_corner && proper)
                                return false;

                        if (!on_corner && _Less(m_low[0], x_intersect, false)
                                        && _Greater(m_high[0], x_intersect, false))
                                return false;

                        // Also check for point inclusion here, only need to do this for one point
                        if (!on_corner && _Less(m_low[0], x_intersect, false))
                                hit = !hit;
		}

		if (low_horizontally_between) { // Check for edge intersect
			CoordType y_intersect = (ends[0])[1] + (m_low[0] - (ends[0])[0])
												   * ydiff / xdiff;

                        bool on_corner = Equal(m_low[1], y_intersect)
                                                         || Equal(m_high[1], y_intersect);

                        if (on_corner && proper)
                                return false;

                        if (!on_corner && _Less(m_low[1], y_intersect, false)
                                        && _Greater(m_high[1], y_intersect, false))
                                return false;
		}

		if (high_vertically_between) { // Check for edge intersect
			CoordType x_intersect = (ends[0])[0] + (m_high[1] - (ends[0])[1])
												   * xdiff / ydiff;

                        bool on_corner = Equal(m_low[0], x_intersect)
                                                         || Equal(m_high[0], x_intersect);

                        if (on_corner && proper)
                                return false;

                        if (!on_corner && _Less(m_low[0], x_intersect, false)
                                        && _Greater(m_high[0], x_intersect, false))
                                return false;
		}

		if (high_horizontally_between) { // Check for edge intersect
			CoordType y_intersect = (ends[0])[1] + (m_high[0] - (ends[0])[0])
												   * ydiff / xdiff;

                        bool on_corner = Equal(m_low[1], y_intersect)
                                                         || Equal(m_high[1], y_intersect);

                        if (on_corner && proper)
                                return false;

                        if (!on_corner && _Less(m_low[1], y_intersect, false)
                                        && _Greater(m_high[1], y_intersect, false))
                                return false;
		}
	}

	return hit;
}

template<>
bool Contains<2>(const RotBox<2>& r, const Polygon<2>& p, bool proper) {
	for (const auto& point: p.m_points)
		if (!Contains(r, point, proper))
			return false;

	return true;
}

template<>
bool Intersect<2>(const Polygon<2>& p1, const Polygon<2>& p2, bool proper) {
	auto begin1 = p1.m_points.begin(), end1 = p1.m_points.end();
	auto begin2 = p2.m_points.begin(), end2 = p2.m_points.end();
	Segment<2> s1, s2;
	int next_end1, next_end2;

	s1.endpoint(0) = p1.m_points.back();
	s2.endpoint(0) = p2.m_points.back();
	next_end1 = next_end2 = 1;
	for (auto i1 = begin1; i1 != end1; ++i1) {
		s1.endpoint(next_end1) = *i1;
		next_end1 = next_end1 ? 0 : 1;

		for (auto i2 = begin2; i2 != end2; ++i2) {
			s2.endpoint(next_end2) = *i2;
			next_end2 = next_end2 ? 0 : 1;

			if (Intersect(s1, s2, proper))
				return true;
		}
	}

	return Contains(p1, p2.m_points.front(), proper)
		   || Contains(p2, p1.m_points.front(), proper);
}

template<>
bool Contains<2>(const Polygon<2>& outer, const Polygon<2>& inner, bool proper) {
	if (proper && !Contains(outer, inner.m_points.front(), true))
		return false;

	auto begin = inner.m_points.begin(), end = inner.m_points.end();
	Segment<2> s;
	s.endpoint(0) = inner.m_points.back();
	int next_end = 1;

	for (auto i = begin; i != end; ++i) {
		s.endpoint(next_end) = *i;
		next_end = next_end ? 0 : 1;
		if (!proper) {
			if (!Contains(outer, s, false))
				return false;
		} else {
			auto begin2 = outer.m_points.begin(),
					end2 = outer.m_points.end();
			Segment<2> s2;
			s2.endpoint(0) = outer.m_points.back();
			int next_end2 = 1;
			for (auto i2 = begin2; i2 != end2; ++i2) {
				s2.endpoint(next_end2) = *i2;
				next_end2 = next_end2 ? 0 : 1;

				if (Intersect(s, s2, false))
					return false;
			}
		}
	}

	return true;
}

}
