// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2004 Alistair Riddoch

#include "iround.h"

#include "Forest.h"
#include "Plant.h"
#include "Area.h"

#include <wfmath/MersenneTwister.h>
#include <wfmath/intersect.h>

#include <iostream>
#include <cmath>

namespace Mercator {

/// \brief Construct a new forest with the given seed.
Forest::Forest(unsigned long seed) :
                m_area(),
                m_seed(seed),
                m_randCache(seed, std::make_unique<ZeroSpiralOrdering>()) {
}

/// \brief Destruct a forest.
///
/// All contained vegetation is lost, so references to contained
/// vegetation must not be maintained if this is likely to occur.
Forest::~Forest() = default;

/// \brief Assign an area to this forest.
void Forest::setArea(std::unique_ptr<Area> area) {
        m_area = std::move(area);
}

static const float plant_chance = 0.04f;
static const float plant_min_height = 5.f;
static const float plant_height_range = 20.f;


/// \brief This function uses a pseudo-random technique to populate the
/// forest with trees. This algorithm as the following essental properties:
///
///   - It is repeatable. It can be repeated on the client and the server,
///     and give identical results.
///
///   - It is location independent. It gives the same results even if the
///     forest is in a different place.
///
///   - It is shape and size independent. A given area of the forest is
///     the same even if the borders of the forest change.
///
///   - It is localizable. It is possible to only partially populate the
///     the forest, and still get the same results in that area.
///
/// This function will have no effect if the area defining the forest remains
/// uninitialised. Any previously generated contents are erased.
/// For each instance a new seed is used to ensure it is repeatable, and
/// height, displacement and orientation are calculated.
void Forest::populate() {
	if (!m_area) return;
	WFMath::AxisBox<2> bbox(m_area->bbox());

	// Fill the plant store with plants.
	m_plants.clear();
	WFMath::MTRand rng;

	int lx = I_ROUND(bbox.lowCorner().x()),
			ly = I_ROUND(bbox.lowCorner().y()),
			hx = I_ROUND(bbox.highCorner().x()),
			hy = I_ROUND(bbox.highCorner().y());

	PlantSpecies::const_iterator I;
	auto Iend = m_species.end();

	for (int j = ly; j < hy; ++j) {
		for (int i = lx; i < hx; ++i) {
			if (!m_area->contains(i, j)) {
				continue;
			}
			double prob = m_randCache(i, j);
			I = m_species.begin();
			for (; I != Iend; ++I) {
				const Species& species = *I;
				if (prob > species.m_probability) {
					prob -= species.m_probability;
					// Next species
					continue;
				}

//                std::cout << "Plant at [" << i << ", " << j << "]"
//                          << std::endl;
				//this is a bit of a hack
				rng.seed((int) (prob / I->m_probability * 123456));

				Plant& plant = m_plants[i][j];
				// plant.setHeight(rng() * plant_height_range + plant_min_height);
				plant.m_displacement = WFMath::Point<2>(
						(rng.rand<WFMath::CoordType>() - 0.5f) * species.m_deviation,
						(rng.rand<WFMath::CoordType>() - 0.5f) * species.m_deviation);
				plant.m_orientation = WFMath::Quaternion(2, rng.rand<WFMath::CoordType>() * 2 * WFMath::numeric_constants<WFMath::CoordType>::pi());
//                auto J = species.m_parameters.begin();
//                auto Jend = species.m_parameters.end();
//                for (; J != Jend; ++J) {
//                    plant.setParameter(J->first, rng.rand<WFMath::CoordType>() * J->second.range + J->second.min);
//                }
				break;
			}
		}
	}
}

}
