// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2003 Damien McGinnes

#include "Intersect.h"
#include "Segment.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Mercator {
//floor and ceil functions that return d-1 and d+1
//respectively if d is integral
static inline float gridceil(float d) {
	float c = std::ceil(d);
	return (c == d) ? c + 1.0f : c;
}

static inline double gridceil(double d) {
	auto c = std::ceil(d);
	return (c == d) ? c + 1.0 : c;
}

static inline float gridfloor(float d) {
	float c = std::floor(d);
	return (c == d) ? c - 1.0f : c;
}

static inline double gridfloor(double d) {
	auto c = std::floor(d);
	return (c == d) ? c - 1.0 : c;
}


//check intersection of an axis-aligned box with the terrain
bool Intersect(const Terrain& t, const WFMath::AxisBox<3>& bbox) {
	WFMath::CoordType max;
	WFMath::CoordType min = bbox.lowCorner()[1];
	const int res = t.getResolution();
	const float spacing = t.getSpacing();

	//determine which segments are involved
	//usually will just be one
	int xlow = (int) floor(bbox.lowCorner()[0] / spacing);
	int xhigh = (int) gridceil(bbox.highCorner()[0] / spacing);
	int zlow = (int) floor(bbox.lowCorner()[2] / spacing);
	int zhigh = (int) gridceil(bbox.highCorner()[2] / spacing);

	//loop across all tiles covered by this bbox
	for (int x = xlow; x < xhigh; x++) {
		for (int z = zlow; z < zhigh; z++) {
			//check the bbox against the extent of each tile
			//as an early rejection
			Segment* thisSeg = t.getSegmentAtIndex(x, z);

			if (thisSeg)
				max = thisSeg->getMax();
			else
				max = Terrain::defaultLevel;

			if (max > min) {
				//entity bbox overlaps with the extents of this tile
				//now check each tile point covered by the entity bbox

				//clip the points to be tested against the bbox
				int min_x = (int) floor(bbox.lowCorner()[0] - ((float) x * spacing));
				if (min_x < 0) min_x = 0;

				int max_x = (int) gridceil(bbox.highCorner()[0] - ((float) x * spacing));
				if (max_x > res) min_x = res;

				int min_z = (int) floor(bbox.lowCorner()[2] - ((float) z * spacing));
				if (min_z < 0) min_z = 0;

				int max_z = (int) gridceil(bbox.highCorner()[2] - ((float) z * spacing));
				if (max_z > res) min_z = res;

				//loop over each point and see if it is greater than the minimum
				//of the bbox. If all points are below, the the bbox does NOT
				//intersect. If a single point is above, then the bbox MIGHT
				//intersect.
				for (int xpt = min_x; xpt <= max_x; xpt++) {
					for (int zpt = min_z; zpt <= max_z; zpt++) {
						if (thisSeg) {
							if (thisSeg->get(xpt, zpt) > min) {
								return true;
							}
						} else if (Terrain::defaultLevel > min) {
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}

static bool HOT(const Terrain& t, const WFMath::Point<3>& pt, double& h) {
	WFMath::Vector<3> normal;
	float terrHeight;
	if (!t.getHeightAndNormal(pt[0], pt[2], terrHeight, normal)) {
		return false;
	}

	h = (pt[1] - terrHeight);
	return true;
}

bool Intersect(const Terrain& t, const WFMath::Point<3>& pt) {
	double h;
	return HOT(t, pt, h) && h <= 0.0;
}

//helper function for ray terrain intersection
static bool cellIntersect(double h1, double h2, double h3, double h4, double X, double Z,
						  const WFMath::Vector<3>& nDir, float dirLen,
						  const WFMath::Point<3>& sPt, WFMath::Point<3>& intersection,
						  WFMath::Vector<3>& normal, double& par) {
	//ray plane intersection roughly using the following:
	//parametric ray eqn:  p=po + par V
	//plane eqn: p dot N + d = 0
	//
	//intersection:
	// -par = (po dot N + d ) / (V dot N)
	//
	//
	// effectively we calculate the ray parametric variable for the
	// intersection of the plane corresponding to each triangle
	// then clip them by endpints of the ray, and by the sides of the square
	// and by the diagonal
	//
	// if they both still intersect, then we choose the earlier intersection


	//intersection points for top and bottom triangles
	WFMath::Point<3> topInt, botInt;

	//point to use in plane equation for both triangles
	WFMath::Vector<3> p0 = WFMath::Vector<3>(X, h1, Z);

	// square is broken into two triangles
	// top triangle |/
	bool topIntersected = false;
	WFMath::Vector<3> topNormal(h2 - h3, 1.0, h1 - h2);
	topNormal.normalize();
	double t = Dot(nDir, topNormal);

	double topP = 0.0;

	if ((t > 1e-7) || (t < -1e-7)) {
		topP = -(Dot((sPt - WFMath::Point<3>(0, 0, 0)), topNormal)
				 - Dot(topNormal, p0)) / t;
		topInt = sPt + nDir * topP;
		//check the intersection is inside the triangle, and within the ray extents
		if ((topP <= dirLen) && (topP > 0.0) &&
			(topInt[0] >= X) && (topInt[2] <= Z + 1) &&
			((topInt[0] - topInt[2]) <= (X - Z))) {
			topIntersected = true;
		}
	}

	// bottom triangle /|
	bool botIntersected = false;
	WFMath::Vector<3> botNormal(h1 - h4, 1.0, h4 - h3);
	botNormal.normalize();
	double b = Dot(nDir, botNormal);
	double botP = 0.0;

	if ((b > 1e-7) || (b < -1e-7)) {
		botP = -(Dot((sPt - WFMath::Point<3>(0, 0, 0)), botNormal)
				 - Dot(botNormal, p0)) / b;
		botInt = sPt + nDir * botP;
		//check the intersection is inside the triangle, and within the ray extents
		if ((botP <= dirLen) && (botP > 0.0) &&
			(botInt[0] <= X + 1) && (botInt[2] >= Z) &&
			((botInt[0] - botInt[2]) >= (X - Z))) {
			botIntersected = true;
		}
	}

	if (topIntersected && botIntersected) { //intersection with both
		if (botP <= topP) {
			intersection = botInt;
			normal = botNormal;
			par = botP / dirLen;
			if (botP == topP) {
				normal += topNormal;
				normal.normalize();
			}
			return true;
		} else {
			intersection = topInt;
			normal = topNormal;
			par = topP / dirLen;
			return true;
		}
	} else if (topIntersected) { //intersection with top
		intersection = topInt;
		normal = topNormal;
		par = topP / dirLen;
		return true;
	} else if (botIntersected) { //intersection with bot
		intersection = botInt;
		normal = botNormal;
		par = botP / dirLen;
		return true;
	}

	return false;
}

//Intersection of ray with terrain
//
//returns true on successful intersection
//ray is represented by a start point (sPt)
//and a direction vector (dir). The length of dir is
//used as the length of the ray. (ie not an infinite ray)
//intersection and normal are filled in on a successful result.
//par is the distance along the ray where intersection occurs
//0.0 == start, 1.0 == end.

bool Intersect(const Terrain& t, const WFMath::Point<3>& sPt, const WFMath::Vector<3>& dir,
                           WFMath::Point<3>& intersection, WFMath::Vector<3>& normal, double& par) {
        // check if startpoint is below ground
        double hot;
        if (HOT(t, sPt, hot) && hot < 0.0) return true;

        // Fast path for vertical rays
        if ((dir[0] == 0.0f) && (dir[2] == 0.0f)) {
                if (dir[1] == 0.0f) {
                        return false;
                }

                float terrHeight;
                WFMath::Vector<3> terrNormal;
                if (!t.getHeightAndNormal(sPt[0], sPt[2], terrHeight, terrNormal)) {
                        return false;
                }

                if (dir[1] > 0.0f) {
                        // vertical up: already checked start point, so no intersection
                        return false;
                }

                double endY = sPt[1] + dir[1];
                if (endY > terrHeight) {
                        return false;
                }

                intersection = WFMath::Point<3>(sPt[0], terrHeight, sPt[2]);
                normal = terrNormal;
                par = (sPt[1] - terrHeight) / std::abs(dir[1]);
                return true;
        }

        const int res = t.getResolution();
        const int size = res + 1;

        double paraX = 0.0, paraZ = 0.0; //used to store the parametric gap between grid crossings
        double pX, pZ; //the accumulators for the parametrics as we traverse the ray
        float h1, h2, h3, h4;

        WFMath::Point<3> last(sPt), next(sPt);
        WFMath::Vector<3> nDir(dir);
        nDir.normalize();
        float dirLen = dir.mag();

        //work out where the ray first crosses an X grid line
        if (dir[0] != 0.0f) {
                paraX = 1.0f / dir[0];
                double crossX = (dir[0] > 0.0f) ? gridceil(last[0]) : gridfloor(last[0]);
                pX = (crossX - last[0]) * paraX;
                pX = std::min(pX, 1.0);
        } else { //parallel: never crosses
                pX = 1.0f;
        }

        //work out where the ray first crosses a Y grid line
        if (dir[2] != 0.0f) {
                paraZ = 1.0f / dir[2];
                double crossZ = (dir[2] > 0.0f) ? gridceil(last[2]) : gridfloor(last[2]);
                pZ = (crossZ - sPt[2]) * paraZ;
                pZ = std::min(pZ, 1.0);
        } else { //parallel: never crosses
                pZ = 1.0f;
        }

        //ensure we traverse the ray forwards
        paraX = std::abs(paraX);
        paraZ = std::abs(paraZ);

        bool endpoint = false;

        Segment* seg = nullptr;
        const float* segData = nullptr;
        float segMax = Terrain::defaultLevel;
        int segX = std::numeric_limits<int>::min();
        int segZ = std::numeric_limits<int>::min();

        //check each candidate tile for an intersection
        while (true) {
                last = next;
                if (pX < pZ) { // cross x grid line first
                        next = sPt + (pX * dir);
                        pX += paraX; // set x accumulator to current p
                } else { //cross z grid line first
                        next = sPt + (pZ * dir);
                        if (pX == pZ) {
                                pX += paraX; //unusual case where ray crosses corner
                        }
                        pZ += paraZ; // set z accumulator to current p
                }

                float x = (dir[0] > 0) ? std::floor(last[0]) : std::floor(next[0]);
                float z = (dir[2] > 0) ? std::floor(last[2]) : std::floor(next[2]);

                int csx = static_cast<int>(x) / res;
                int csz = static_cast<int>(z) / res;
                if (csx != segX || csz != segZ) {
                        seg = t.getSegmentAtIndex(csx, csz);
                        if (seg) {
                                segData = seg->getPoints();
                                segMax = seg->getMax();
                        } else {
                                segData = nullptr;
                                segMax = Terrain::defaultLevel;
                        }
                        segX = csx;
                        segZ = csz;
                }

                if ((last[1] > segMax) && (next[1] > segMax)) {
                        // Ray section entirely above segment
                } else {
                        if (segData) {
                                int localX = static_cast<int>(x) - segX * res;
                                int localZ = static_cast<int>(z) - segZ * res;
                                int base = localZ * size + localX;
                                h1 = segData[base];
                                h2 = segData[base + size];
                                h3 = segData[base + size + 1];
                                h4 = segData[base + 1];
                        } else {
                                h1 = h2 = h3 = h4 = Terrain::defaultLevel;
                        }
                        auto height = std::max(std::max(h1, h2), std::max(h3, h4));

                        if ((last[1] < height) || (next[1] < height)) {
                                // possible intersect with this tile
                                if (cellIntersect(h1, h2, h3, h4, x, z, nDir, dirLen, sPt,
                                                                  intersection, normal, par)) {
                                        return true;
                                }
                        }
                }

                if ((pX >= 1.0f) && (pZ >= 1.0f)) {
                        if (endpoint) {
                                break;
                        } else endpoint = true;
                }
        }

        return false;
}

} // namespace Mercator
