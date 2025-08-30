// This file may be redistributed and modified only under the terms of
// the GNU General Public License (See COPYING for details).
// Copyright (C) 2024

#include <Mercator/Terrain.h>
#include <Mercator/Area.h>
#include <Mercator/AreaShader.h>
#include <Mercator/Segment.h>

#include <cassert>

typedef WFMath::Point<2> Point2;

void testAreaShaderHole() {
        auto outer = std::make_unique<Mercator::Area>(1, false);
        WFMath::Polygon<2> p;
        p.addCorner(p.numCorners(), Point2(0, 0));
        p.addCorner(p.numCorners(), Point2(16, 0));
        p.addCorner(p.numCorners(), Point2(16, 16));
        p.addCorner(p.numCorners(), Point2(0, 16));
        outer->setShape(p);

        auto hole = std::make_unique<Mercator::Area>(1, true);
        WFMath::Polygon<2> h;
        h.addCorner(h.numCorners(), Point2(4, 4));
        h.addCorner(h.numCorners(), Point2(12, 4));
        h.addCorner(h.numCorners(), Point2(12, 12));
        h.addCorner(h.numCorners(), Point2(4, 12));
        hole->setShape(h);

        Mercator::Terrain terrain(Mercator::Terrain::SHADED, 16);

        Mercator::AreaShader ashade(1);
        terrain.addShader(&ashade, 0);

        terrain.setBasePoint(0, 0, 0);
        terrain.setBasePoint(0, 1, 0);
        terrain.setBasePoint(1, 0, 0);
        terrain.setBasePoint(1, 1, 0);

        terrain.updateArea(1, std::move(outer));
        terrain.updateArea(2, std::move(hole));

        Mercator::Segment* seg = terrain.getSegmentAtIndex(0, 0);
        seg->populateSurfaces();

        Mercator::Surface* surf = seg->getSurfaces()[0].get();
        assert(surf);

        assert((*surf)(2, 2, 0) > 0);
        assert((*surf)(6, 6, 0) == 0);
}

int main() {
        testAreaShaderHole();
        return 0;
}
