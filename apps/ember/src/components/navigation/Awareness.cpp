/*
 Copyright (C) 2014 Erik Ogenvik

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software Foundation,
 Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

 Some portions of this file include code taken from the OgreCrowd project, which has the copyrights and license as described below.
 These portions are the findPath() and processTiles() methods.

    OgreCrowd
    ---------

    Copyright (c) 2012 Jonas Hauquier

    Additional contributions by:

    - mkultra333
    - Paul Wilson

    Sincere thanks and to:

    - Mikko Mononen (developer of Recast navigation libraries)

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
    THE SOFTWARE.


 */

#include "Awareness.h"
#include "AwarenessUtils.h"

#include "DetourNavMeshQuery.h"
#include "DetourObstacleAvoidance.h"

#include "domain/IHeightProvider.h"
#include "domain/EmberEntity.h"

#include "framework/Log.h"
#include "framework/Exception.h"

#include <Eris/View.h>
#include <Eris/Avatar.h>
#include <wfmath/wfmath.h>

#include <sigc++/bind.h>

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/hashed_index.hpp>
#include <boost/multi_index/sequenced_index.hpp>

#include <queue>

#define MAX_PATHPOLY      256 // max number of polygons in a path
#define MAX_PATHVERT      512 // most verts in a path
#define MAX_OBSTACLES_CIRCLES 4 // max number of circle obstacles to consider when doing avoidance

namespace Ember::Navigation {

// This value specifies how many layers (or "floors") each navmesh tile is expected to have.
static const int EXPECTED_LAYERS_PER_TILE = 1;

using namespace boost::multi_index;

/**
 * @brief A Most Recently Used list implemented using boost::multi_index.
 *
 */
template<typename TItem>
class MRUList {
public:

	void insert(const TItem& item) {
		auto p = mItems.push_front(item);

		if (!p.second) {
			mItems.relocate(mItems.begin(), p.first);
		}
	}

	TItem pop_back() {
		TItem back = mItems.back();
		mItems.pop_back();
		return back;
	}

	std::size_t size() const {
		return mItems.size();
	}


private:
	multi_index_container<TItem, indexed_by<sequenced<>, hashed_unique<identity<TItem> > > > mItems;

};

class AwarenessContext : public rcContext {
protected:
	void doLog(const rcLogCategory category, const char* msg, const int len) override {
		if (category == RC_LOG_PROGRESS) {
			logger->debug("Recast: {}", msg);
		} else if (category == RC_LOG_WARNING) {
			logger->warn("Recast: {}", msg);
		} else {
			logger->error("Recast: {}", msg);
		}
	}

};

Awareness::Awareness(EmberEntity& avatarEntity, IHeightProvider& heightProvider, int tileSize, int walkableClimb, int walkableSlopeAngle) :
		mAvatarEntity(avatarEntity),
		mDomainEntity(*mAvatarEntity.getEmberLocation()),
		mHeightProvider(heightProvider),
		mTalloc(std::make_unique<LinearAllocator>(128000)),
		mTcomp(std::make_unique<FastLZCompressor>()),
		mTmproc(std::make_unique<MeshProcess>()),
		mAvatarRadius(0.4f),
		mDesiredTilesAmount(128),
		mCtx(std::make_unique<AwarenessContext>()),
		mCfg{},
		mTileCache(nullptr),
		mNavMesh(nullptr),
		mNavQuery(dtAllocNavMeshQuery()),
		mFilter(std::make_unique<dtQueryFilter>()),
		mObstacleAvoidanceParams(std::make_unique<dtObstacleAvoidanceParams>()),
		mActiveTileList(std::make_unique<MRUList<std::pair<int, int>>>()) {
	try {

		// Setup the default query filter
		mFilter->setIncludeFlags(0xFFFF);    // Include all
		mFilter->setExcludeFlags(0);         // Exclude none
		// Area flags for polys to consider in search, and their cost
		mFilter->setAreaCost(POLYAREA_GROUND, 1.0f);

		//height of our agent, default to 2 meters unless there's a bbox
		float h = 2.0f;

		if (mAvatarEntity.hasBBox() && mAvatarEntity.getBBox().isValid()) {
			auto avatarBbox = mAvatarEntity.getBBox();
			//get the radius from the avatar entity
			WFMath::AxisBox<2> avatar2dBbox(WFMath::Point<2>(avatarBbox.lowCorner().x(), avatarBbox.lowCorner().z()), WFMath::Point<2>(avatarBbox.highCorner().x(), avatarBbox.highCorner().z()));
			mAvatarRadius = std::max(0.2, avatar2dBbox.boundingSphere().radius()); //Don't make the radius smaller than 0.2 meters, to avoid too many cells

			//height of our agent
			h = avatarBbox.highCorner().y() - avatarBbox.lowCorner().y();
		}

		auto& extent = mDomainEntity.getBBox();

		const WFMath::Point<3>& lower = extent.lowCorner();
		const WFMath::Point<3>& upper = extent.highCorner();

		//Recast uses y for the vertical axis
		mCfg.bmin[0] = lower.x();
		mCfg.bmin[1] = std::min(-500.0, lower.y());
		mCfg.bmin[2] = lower.z();
		mCfg.bmax[0] = upper.x();
		mCfg.bmax[1] = std::max(500.0, upper.y());
		mCfg.bmax[2] = upper.z();

		int gw = 0, gh = 0;
		float cellsize = mAvatarRadius / 2.0f; //Should be enough for outdoors; indoors we might want r / 3.0 instead
		rcCalcGridSize(mCfg.bmin, mCfg.bmax, cellsize, &gw, &gh);
		const int tilewidth = (gw + tileSize - 1) / tileSize;
		const int tileheight = (gh + tileSize - 1) / tileSize;

		// Max tiles and max polys affect how the tile IDs are calculated.
		// There are 22 bits available for identifying a tile and a polygon.
		int tileBits = rcMin((int) dtIlog2(dtNextPow2(tilewidth * tileheight * EXPECTED_LAYERS_PER_TILE)), 14);
		if (tileBits > 14)
			tileBits = 14;
		int polyBits = 22 - tileBits;
		int maxTiles = 1u << tileBits;
		int maxPolysPerTile = 1u << polyBits;

		//For an explanation of these values see http://digestingduck.blogspot.se/2009/08/recast-settings-uncovered.html

		mCfg.cs = cellsize;
		mCfg.ch = mCfg.cs / 2.0f; //Height of one voxel; should really only come into play when doing 3d traversal
		//	m_cfg.ch = std::max(upper.y() - lower.y(), 100.0f); //For 2d traversal make the voxel size as large as possible
                mCfg.walkableHeight = (int) std::ceil(h / mCfg.ch); //This is in voxels
                mCfg.walkableClimb = walkableClimb;
                mCfg.walkableRadius = (int) std::ceil(mAvatarRadius / mCfg.cs);
                mCfg.walkableSlopeAngle = walkableSlopeAngle;

		mCfg.maxEdgeLen = mCfg.walkableRadius * 8;
		mCfg.maxSimplificationError = 1.3f;
		mCfg.minRegionArea = (int) rcSqr(8);
		mCfg.mergeRegionArea = (int) rcSqr(20);

		mCfg.tileSize = tileSize;
		mCfg.borderSize = mCfg.walkableRadius + 3; // Reserve enough padding.
		mCfg.width = mCfg.tileSize + mCfg.borderSize * 2;
		mCfg.height = mCfg.tileSize + mCfg.borderSize * 2;
		//	m_cfg.detailSampleDist = m_detailSampleDist < 0.9f ? 0 : m_cfg.cs * m_detailSampleDist;
		//	m_cfg.detailSampleMaxError = m_cfg.m_cellHeight * m_detailSampleMaxError;

		// Tile cache params.
		dtTileCacheParams tcparams{};
		memset(&tcparams, 0, sizeof(tcparams));
		rcVcopy(tcparams.orig, mCfg.bmin);
		tcparams.cs = mCfg.cs;
		tcparams.ch = mCfg.ch;
		tcparams.width = (int) mCfg.tileSize;
		tcparams.height = (int) mCfg.tileSize;
		tcparams.walkableHeight = h;
		tcparams.walkableRadius = mAvatarRadius;
		tcparams.walkableClimb = mCfg.walkableClimb;
		//	tcparams.maxSimplificationError = m_edgeMaxError;
		tcparams.maxTiles = tilewidth * tileheight * EXPECTED_LAYERS_PER_TILE;
		tcparams.maxObstacles = 128;

		dtStatus status;

		mTileCache = dtAllocTileCache();
		if (!mTileCache) {
			throw Exception("buildTiledNavigation: Could not allocate tile cache.");
		}
		status = mTileCache->init(&tcparams, mTalloc.get(), mTcomp.get(), mTmproc.get());
		if (dtStatusFailed(status)) {
			throw Exception("buildTiledNavigation: Could not init tile cache.");
		}

		dtFreeNavMesh(mNavMesh);

		mNavMesh = dtAllocNavMesh();
		if (!mNavMesh) {
			throw Exception("buildTiledNavigation: Could not allocate navmesh.");
		}

		dtNavMeshParams params{};
		memset(&params, 0, sizeof(params));
		rcVcopy(params.orig, mCfg.bmin);
		params.tileWidth = (float) tileSize * cellsize;
		params.tileHeight = (float) tileSize * cellsize;
		params.maxTiles = maxTiles;
		params.maxPolys = maxPolysPerTile;

		status = mNavMesh->init(&params);
		if (dtStatusFailed(status)) {
			throw Exception("buildTiledNavigation: Could not init navmesh.");
		}

		status = mNavQuery->init(mNavMesh, 2048);
		if (dtStatusFailed(status)) {
			throw Exception("buildTiledNavigation: Could not init Detour navmesh query");
		}

		mObstacleAvoidanceQuery = dtAllocObstacleAvoidanceQuery();
		mObstacleAvoidanceQuery->init(MAX_OBSTACLES_CIRCLES, 0);

		mObstacleAvoidanceParams->velBias = 0.4f;
		mObstacleAvoidanceParams->weightDesVel = 2.0f;
		mObstacleAvoidanceParams->weightCurVel = 0.75f;
		mObstacleAvoidanceParams->weightSide = 0.75f;
		mObstacleAvoidanceParams->weightToi = 2.5f;
		mObstacleAvoidanceParams->horizTime = 2.5f;
		mObstacleAvoidanceParams->gridSize = 33;
		mObstacleAvoidanceParams->adaptiveDivs = 7;
		mObstacleAvoidanceParams->adaptiveRings = 2;
		mObstacleAvoidanceParams->adaptiveDepth = 5;

		for (auto entity: mDomainEntity.getContent()) {
			if (entity->getId() != mAvatarEntity.getId()) {
				//Only observe entities that have a bounding box.
				if (entity->hasBBox() && entity->getBBox().isValid()) {
					auto result = mObservedEntities.insert(std::make_pair(entity, EntityConnections()));
					EntityConnections& connections = result.first->second;

					connections.isIgnored = false;
					if (entity->hasProperty("velocity")) {
						mMovingEntities.push_back(entity);
						connections.isMoving = true;
					} else {
						connections.moved = entity->Moved.connect(sigc::bind(sigc::mem_fun(*this, &Awareness::Entity_Moved), entity));
						connections.isMoving = false;
					}
					buildEntityAreas(*entity, mEntityAreas);
				}
			}
		}
		mDomainEntity.ChildAdded.connect(sigc::mem_fun(*this, &Awareness::Domain_ChildAdded));
		mDomainEntity.ChildRemoved.connect(sigc::mem_fun(*this, &Awareness::Domain_ChildRemoved));
	} catch (const std::exception& e) {
		mObstacleAvoidanceParams.reset();
		dtFreeObstacleAvoidanceQuery(mObstacleAvoidanceQuery);

		dtFreeNavMesh(mNavMesh);
		dtFreeNavMeshQuery(mNavQuery);
		mFilter.reset();

		dtFreeTileCache(mTileCache);

		mTmproc.reset();
		mTcomp.reset();
		mTalloc.reset();

		mCtx.reset();
		mActiveTileList.reset();
		throw;

	}
}

Awareness::~Awareness() {
	//disconnect signals when shutting down
	mSignalConnections.clear();
	mObservedEntities.clear();

	dtFreeObstacleAvoidanceQuery(mObstacleAvoidanceQuery);

	dtFreeNavMesh(mNavMesh);
	dtFreeNavMeshQuery(mNavQuery);

	dtFreeTileCache(mTileCache);

}

void Awareness::Domain_ChildAdded(Eris::Entity* entity) {
	auto result = mObservedEntities.insert(std::make_pair(entity, EntityConnections()));
	EntityConnections& connections = result.first->second;
	connections.isMoving = false;
	connections.bboxChanged = entity->observe("bbox", [&, entity](const Atlas::Message::Element&) {
		this->Entity_Moved(entity);
	}, true);

}

void Awareness::Domain_ChildRemoved(Eris::Entity* entity) {
	auto I = mObservedEntities.find(entity);
	if (I != mObservedEntities.end()) {
		if (!I->second.isIgnored) {
			if (I->second.isMoving) {
				mMovingEntities.remove(entity);
			} else {
				std::map<Eris::Entity*, WFMath::RotBox<2>> areas;

				buildEntityAreas(*entity, areas);

				for (auto& entry: areas) {
					markTilesAsDirty(entry.second.boundingBox());
				}
				mEntityAreas.erase(entity);
			}
			mObservedEntities.erase(entity);
		}
	}
}

void Awareness::Entity_Moved(Eris::Entity* entity) {
	//If an entity which previously didn't move start moving we need to move it to the "movable entities" collection.
	if (entity->getVelocity().isValid() && entity->getVelocity() != WFMath::Vector<3>::ZERO()) {
		mMovingEntities.push_back(entity);
		auto I = mObservedEntities.find(entity);
		//No need to listen to more Moved events.
		I->second.moved.disconnect();
		I->second.isMoving = true;
		auto existingI = mEntityAreas.find(entity);
		if (existingI != mEntityAreas.end()) {
			//The entity already was registered; mark those tiles where the entity previously were as dirty.
			markTilesAsDirty(existingI->second.boundingBox());
			mEntityAreas.erase(entity);
		}
	} else {
		if (entity->hasBBox()) {
			std::map<Eris::Entity*, WFMath::RotBox<2>> areas;

			buildEntityAreas(*entity, areas);

			for (auto& entry: areas) {
				markTilesAsDirty(entry.second.boundingBox());
				auto existingI = mEntityAreas.find(entry.first);
				if (existingI != mEntityAreas.end()) {
					//The entity already was registered; mark both those tiles where the entity previously were as well as the new tiles as dirty.
					markTilesAsDirty(existingI->second.boundingBox());
					existingI->second = entry.second;
				} else {
					mEntityAreas.insert(entry);
				}
			}
		}
	}

}

bool Awareness::avoidObstacles(const WFMath::Point<2>& position, const WFMath::Vector<2>& desiredVelocity, WFMath::Vector<2>& newVelocity) const {
	struct EntityCollisionEntry {
		float distance;
		Eris::Entity* entity;
		WFMath::Point<2> viewPosition;
		WFMath::Ball<2> viewRadius;
	};

	auto comp = [](EntityCollisionEntry& a, EntityCollisionEntry& b) { return a.distance < b.distance; };
	std::priority_queue<EntityCollisionEntry, std::vector<EntityCollisionEntry>, decltype(comp)> nearestEntities(comp);

	WFMath::Ball<2> playerRadius(position, 5);

	for (auto entity: mMovingEntities) {


		//All of the entities have the same location as we have, so we don't need to resolve the position in the world.

		WFMath::Point<3> pos = entity->getPredictedPos();
		if (!pos.isValid()) {
			continue;
		}

		WFMath::Point<2> entityView2dPos(pos.x(), pos.z());
		WFMath::Ball<2> entityViewRadius(entityView2dPos, entity->getBBox().boundingSphereSloppy().radius());

		if (WFMath::Intersect(playerRadius, entityViewRadius, false) || WFMath::Contains(playerRadius, entityViewRadius, false)) {
			nearestEntities.push(EntityCollisionEntry({static_cast<float>(WFMath::Distance(position, entityView2dPos)), entity, entityView2dPos, entityViewRadius}));
		}


	}

	if (!nearestEntities.empty()) {
		mObstacleAvoidanceQuery->reset();
		int i = 0;
		while (!nearestEntities.empty() && i < MAX_OBSTACLES_CIRCLES) {
			const EntityCollisionEntry& entry = nearestEntities.top();
			auto entity = entry.entity;
			float pos[]{static_cast<float>(entry.viewPosition.x()), 0, static_cast<float>(entry.viewPosition.y())};
			float vel[]{static_cast<float>(entity->getVelocity().x()), 0, static_cast<float>(entity->getVelocity().z())};
			mObstacleAvoidanceQuery->addCircle(pos, entry.viewRadius.radius(), vel, vel);
			nearestEntities.pop();
			++i;
		}

		float pos[]{static_cast<float>(position.x()), 0, static_cast<float>(position.y())};
		float vel[]{static_cast<float>(desiredVelocity.x()), 0, static_cast<float>(desiredVelocity.y())};
		float dvel[]{static_cast<float>(desiredVelocity.x()), 0, static_cast<float>(desiredVelocity.y())};
		float nvel[]{0, 0, 0};
		float desiredSpeed = desiredVelocity.mag();

		auto samples = mObstacleAvoidanceQuery->sampleVelocityAdaptive(pos, mAvatarRadius, desiredSpeed, vel, dvel, nvel, mObstacleAvoidanceParams.get(), nullptr);
		if (samples > 0) {
			if (!WFMath::Equal(vel[0], nvel[0]) || !WFMath::Equal(vel[2], nvel[2])) {
				newVelocity.x() = nvel[0];
				newVelocity.y() = nvel[2];
				newVelocity.setValid(true);
				return true;
			}
		}

	}
	return false;

}

void Awareness::markTilesAsDirty(const WFMath::AxisBox<2>& area) {
	int tileMinXIndex, tileMaxXIndex, tileMinYIndex, tileMaxYIndex;
	findAffectedTiles(area, tileMinXIndex, tileMaxXIndex, tileMinYIndex, tileMaxYIndex);
	markTilesAsDirty(tileMinXIndex, tileMaxXIndex, tileMinYIndex, tileMaxYIndex);
}

void Awareness::markTilesAsDirty(int tileMinXIndex, int tileMaxXIndex, int tileMinYIndex, int tileMaxYIndex) {
	bool wereDirtyTiles = !mDirtyAwareTiles.empty();

	for (int tx = tileMinXIndex; tx <= tileMaxXIndex; ++tx) {
		for (int ty = tileMinYIndex; ty <= tileMaxYIndex; ++ty) {
			std::pair<int, int> index(tx, ty);
			if (mAwareTiles.find(index) != mAwareTiles.end()) {
				if (mDirtyAwareTiles.insert(index).second) {
					mDirtyAwareOrderedTiles.push_back(index);
				}
			} else {
				mDirtyUnwareTiles.insert(index);
			}
		}
	}
	if (!wereDirtyTiles && !mDirtyAwareTiles.empty()) {
		EventTileDirty();
	}
}

size_t Awareness::rebuildDirtyTile() {
	if (!mDirtyAwareTiles.empty()) {
		const auto tileIndexI = mDirtyAwareOrderedTiles.begin();
		const auto& tileIndex = *tileIndexI;

		float tilesize = (float) mCfg.tileSize * mCfg.cs;
		WFMath::AxisBox<2> adjustedArea(WFMath::Point<2>(mCfg.bmin[0] + (tileIndex.first * tilesize), mCfg.bmin[2] + (tileIndex.second * tilesize)),
										WFMath::Point<2>(mCfg.bmin[0] + ((tileIndex.first + 1) * tilesize), mCfg.bmin[2] + ((tileIndex.second + 1) * tilesize)));

		std::vector<WFMath::RotBox<2>> entityAreas;
		findEntityAreas(adjustedArea, entityAreas);

		rebuildTile(tileIndex.first, tileIndex.second, entityAreas);
		mDirtyAwareTiles.erase(tileIndex);
		mDirtyAwareOrderedTiles.erase(tileIndexI);
	}
	return mDirtyAwareTiles.size();
}

void Awareness::pruneTiles() {
	//remove any tiles that aren't used
	if (mActiveTileList->size() > mAwareTiles.size()) {
		if (mActiveTileList->size() > mDesiredTilesAmount) {
			auto entry = mActiveTileList->pop_back();

			dtCompressedTileRef tilesRefs[MAX_LAYERS];
			const int ntiles = mTileCache->getTilesAt(entry.first, entry.second, tilesRefs, MAX_LAYERS);
			for (int i = 0; i < ntiles; ++i) {
				const dtCompressedTile* tile = mTileCache->getTileByRef(tilesRefs[i]);
				float min[3];
				int tx = tile->header->tx;
				int ty = tile->header->ty;
				int tlayer = tile->header->tlayer;
				rcVcopy(min, tile->header->bmin);
				mTileCache->removeTile(tilesRefs[i], nullptr, nullptr);
				mNavMesh->removeTile(mNavMesh->getTileRefAt(tx, ty, tlayer), nullptr, nullptr);

				EventTileRemoved(tx, ty, tlayer);
			}

		}
	}
}

bool Awareness::needsPruning() const {
	return (mActiveTileList->size() > mDesiredTilesAmount) && (mActiveTileList->size() > mAwareTiles.size());
}

void Awareness::setDesiredTilesAmount(size_t amount) {
	mDesiredTilesAmount = amount;
}

void Awareness::findAffectedTiles(const WFMath::AxisBox<2>& area, int& tileMinXIndex, int& tileMaxXIndex, int& tileMinYIndex, int& tileMaxYIndex) const {
	float tilesize = mCfg.tileSize * mCfg.cs;
	WFMath::Point<2> lowCorner = area.lowCorner();
	WFMath::Point<2> highCorner = area.highCorner();

	if (lowCorner.x() < mCfg.bmin[0]) {
		lowCorner.x() = mCfg.bmin[0];
	}
	if (lowCorner.y() < mCfg.bmin[2]) {
		lowCorner.y() = mCfg.bmin[2];
	}
	if (lowCorner.x() > mCfg.bmax[0]) {
		lowCorner.x() = mCfg.bmax[0];
	}
	if (lowCorner.y() > mCfg.bmax[2]) {
		lowCorner.y() = mCfg.bmax[2];
	}

	if (highCorner.x() < mCfg.bmin[0]) {
		highCorner.x() = mCfg.bmin[0];
	}
	if (highCorner.y() < mCfg.bmin[2]) {
		highCorner.y() = mCfg.bmin[2];
	}
	if (highCorner.x() > mCfg.bmax[0]) {
		highCorner.x() = mCfg.bmax[0];
	}
	if (highCorner.y() > mCfg.bmax[2]) {
		highCorner.y() = mCfg.bmax[2];
	}

	tileMinXIndex = (lowCorner.x() - mCfg.bmin[0]) / tilesize;
	tileMaxXIndex = (highCorner.x() - mCfg.bmin[0]) / tilesize;
	tileMinYIndex = (lowCorner.y() - mCfg.bmin[2]) / tilesize;
	tileMaxYIndex = (highCorner.y() - mCfg.bmin[2]) / tilesize;
}

int Awareness::findPath(const WFMath::Point<3>& start, const WFMath::Point<3>& end, std::list<WFMath::Point<3>>& path) const {

	float pStartPos[]{static_cast<float>(start.x()), static_cast<float>(start.y()), static_cast<float>(start.z())};
	float pEndPos[]{static_cast<float>(end.x()), static_cast<float>(end.y()), static_cast<float>(end.z())};
	float extent[]{2, 100, 2}; //Look two meters in each direction

	dtStatus status;
	dtPolyRef StartPoly;
	float StartNearest[3];
	dtPolyRef EndPoly;
	float EndNearest[3];
	dtPolyRef PolyPath[MAX_PATHPOLY];
	int nPathCount = 0;
	float StraightPath[MAX_PATHVERT * 3];
	int nVertCount = 0;

// find the start polygon
	status = mNavQuery->findNearestPoly(pStartPos, extent, mFilter.get(), &StartPoly, StartNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
		return -1; // couldn't find a polygon

// find the end polygon
	status = mNavQuery->findNearestPoly(pEndPos, extent, mFilter.get(), &EndPoly, EndNearest);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
		return -2; // couldn't find a polygon

	status = mNavQuery->findPath(StartPoly, EndPoly, StartNearest, EndNearest, mFilter.get(), PolyPath, &nPathCount, MAX_PATHPOLY);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
		return -3; // couldn't create a path
	if (nPathCount == 0)
		return -4; // couldn't find a path

	status = mNavQuery->findStraightPath(StartNearest, EndNearest, PolyPath, nPathCount, StraightPath, nullptr, nullptr, &nVertCount, MAX_PATHVERT);
	if ((status & DT_FAILURE) || (status & DT_STATUS_DETAIL_MASK))
		return -5; // couldn't create a path
	if (nVertCount == 0)
		return -6; // couldn't find a path

// At this point we have our path.
	for (int nVert = 0; nVert < nVertCount; nVert++) {
		path.emplace_back(StraightPath[nVert * 3], StraightPath[(nVert * 3) + 1], StraightPath[(nVert * 3) + 2]);
	}

	return nVertCount;
}

void Awareness::setAwarenessArea(const WFMath::RotBox<2>& area, const WFMath::Segment<2>& focusLine) {

	WFMath::AxisBox<2> axisbox = area.boundingBox();

//adjust area to fit with tiles

	float tilesize = mCfg.tileSize * mCfg.cs;
	WFMath::Point<2> lowCorner = axisbox.lowCorner();
	WFMath::Point<2> highCorner = axisbox.highCorner();

	if (lowCorner.x() < mCfg.bmin[0]) {
		lowCorner.x() = mCfg.bmin[0];
	}
	if (lowCorner.y() < mCfg.bmin[2]) {
		lowCorner.y() = mCfg.bmin[2];
	}
	if (lowCorner.x() > mCfg.bmax[0]) {
		lowCorner.x() = mCfg.bmax[0];
	}
	if (lowCorner.y() > mCfg.bmax[2]) {
		lowCorner.y() = mCfg.bmax[2];
	}

	if (highCorner.x() < mCfg.bmin[0]) {
		highCorner.x() = mCfg.bmin[0];
	}
	if (highCorner.y() < mCfg.bmin[2]) {
		highCorner.y() = mCfg.bmin[2];
	}
	if (highCorner.x() > mCfg.bmax[0]) {
		highCorner.x() = mCfg.bmax[0];
	}
	if (highCorner.y() > mCfg.bmax[2]) {
		highCorner.y() = mCfg.bmax[2];
	}

	int tileMinXIndex = (lowCorner.x() - mCfg.bmin[0]) / tilesize;
	int tileMaxXIndex = (highCorner.x() - mCfg.bmin[0]) / tilesize;
	int tileMinYIndex = (lowCorner.y() - mCfg.bmin[2]) / tilesize;
	int tileMaxYIndex = (highCorner.y() - mCfg.bmin[2]) / tilesize;

//Now mark tiles
	const float tcs = mCfg.tileSize * mCfg.cs;
	const float tileBorderSize = mCfg.borderSize * mCfg.cs;

	auto oldDirtyAwareTiles = mDirtyAwareTiles;
	mDirtyAwareTiles.clear();
	mDirtyAwareOrderedTiles.clear();
	mAwareTiles.clear();
	bool wereDirtyTiles = !mDirtyAwareTiles.empty();
	for (int tx = tileMinXIndex; tx <= tileMaxXIndex; ++tx) {
		for (int ty = tileMinYIndex; ty <= tileMaxYIndex; ++ty) {
			// Tile bounds.
			WFMath::AxisBox<2> tileBounds(WFMath::Point<2>((mCfg.bmin[0] + tx * tcs) - tileBorderSize, (mCfg.bmin[2] + ty * tcs) - tileBorderSize),
										  WFMath::Point<2>((mCfg.bmin[0] + (tx + 1) * tcs) + tileBorderSize, (mCfg.bmin[2] + (ty + 1) * tcs) + tileBorderSize));
			if (WFMath::Intersect(area, tileBounds, false) || WFMath::Contains(area, tileBounds, false)) {

				std::pair<int, int> index(tx, ty);
				//If true we should insert in the front of the dirty tiles list.
				bool insertFront = false;
				//If true we should insert in the back of the dirty tiles list.
				bool insertBack = false;
				//If the tile was marked as dirty in the old aware tiles, retain it as such
				if (oldDirtyAwareTiles.find(index) != oldDirtyAwareTiles.end()) {
					if (focusLine.isValid() && WFMath::Intersect(focusLine, tileBounds, false)) {
						insertFront = true;
					} else {
						insertBack = true;
					}
				} else if (mDirtyUnwareTiles.find(index) != mDirtyUnwareTiles.end()) {
					//if the tile was marked as dirty in the unaware tiles we'll move it to the dirty aware collection.
					if (focusLine.isValid() && WFMath::Intersect(focusLine, tileBounds, false)) {
						insertFront = true;
					} else {
						insertBack = true;
					}
				} else {
					//The tile wasn't marked as dirty in any set, but it might be that it hasn't been processed before.
					auto tile = mTileCache->getTileAt(tx, ty, 0);
					if (!tile) {
						if (focusLine.isValid() && WFMath::Intersect(focusLine, tileBounds, false)) {
							insertFront = true;
						} else {
							insertBack = true;
						}
					}
				}

				if (insertFront) {
					if (mDirtyAwareTiles.insert(index).second) {
						mDirtyAwareOrderedTiles.push_front(index);
					}
				} else if (insertBack) {
					if (mDirtyAwareTiles.insert(index).second) {
						mDirtyAwareOrderedTiles.push_back(index);
					}
				}

				mDirtyUnwareTiles.erase(index);

				mAwareTiles.insert(index);

				mActiveTileList->insert(index);
			}
		}
	}

	if (!wereDirtyTiles && !mDirtyAwareTiles.empty()) {
		EventTileDirty();
	}
}

void Awareness::rebuildTile(int tx, int ty, const std::vector<WFMath::RotBox<2>>& entityAreas) {
	TileCacheData tiles[MAX_LAYERS];
	memset(tiles, 0, sizeof(tiles));

	int ntiles = rasterizeTileLayers(entityAreas, tx, ty, tiles, MAX_LAYERS);

	for (int j = 0; j < ntiles; ++j) {
		TileCacheData* tile = &tiles[j];

		auto* header = (dtTileCacheLayerHeader*) tile->data;
		dtTileRef tileRef = mTileCache->getTileRef(mTileCache->getTileAt(header->tx, header->ty, header->tlayer));
		if (tileRef) {
			mTileCache->removeTile(tileRef, nullptr, nullptr);
		}
		dtStatus status = mTileCache->addTile(tile->data, tile->dataSize, DT_COMPRESSEDTILE_FREE_DATA, nullptr);  // Add compressed tiles to tileCache
		if (dtStatusFailed(status)) {
			dtFree(tile->data);
			tile->data = nullptr;
			continue;
		}
	}

	mTileCache->buildNavMeshTilesAt(tx, ty, mNavMesh);

	EventTileUpdated(tx, ty);

}

void Awareness::buildEntityAreas(Eris::Entity& entity, std::map<Eris::Entity*, WFMath::RotBox<2>>& entityAreas) const {

	//The entity is solid (i.e. can be collided with) if it has a bbox and the "solid" property isn't set to false (or 0 as it's an int).
	bool isSolid = entity.hasBBox();
	if (isSolid && entity.hasProperty("solid")) {
		auto solidElement = entity.valueOfProperty("solid");
		if (solidElement.isInt() && solidElement.asInt() == 0) {
			isSolid = false;
		}
	}

	if (isSolid) {
		auto pos = entity.getPosition();
		auto orientation = entity.getOrientation();
		if (pos.isValid()) {
			WFMath::RotMatrix<2> rm;
			if (orientation.isValid()) {
				WFMath::Vector<3> xVec = WFMath::Vector<3>(1.0, 0.0, 0.0).rotate(orientation);
				auto theta = std::atan2(xVec.z(), xVec.x()); // rotation about Y

				rm.rotation(theta);
			}

			auto bbox = entity.getBBox();

			WFMath::Point<2> highCorner(bbox.highCorner().x(), bbox.highCorner().z());
			WFMath::Point<2> lowCorner(bbox.lowCorner().x(), bbox.lowCorner().z());

			//Expand the box a little so that we can navigate around it without being stuck on it.
			//We'll the radius of the avatar.
			highCorner += WFMath::Vector<2>(mAvatarRadius, mAvatarRadius);
			lowCorner -= WFMath::Vector<2>(mAvatarRadius, mAvatarRadius);

			WFMath::RotBox<2> rotbox(WFMath::Point<2>::ZERO(), highCorner - lowCorner, WFMath::RotMatrix<2>().identity());
			rotbox.shift(WFMath::Vector<2>(lowCorner.x(), lowCorner.y()));
			if (rm.isValid()) {
				rotbox.rotatePoint(rm, WFMath::Point<2>::ZERO());
			}

			rotbox.shift(WFMath::Vector<2>(pos.x(), pos.z()));

			entityAreas.insert(std::make_pair(&entity, rotbox));
		}
	}
}

void Awareness::findEntityAreas(const WFMath::AxisBox<2>& extent, std::vector<WFMath::RotBox<2> >& areas) {
	for (auto& entry: mEntityAreas) {
		auto& rotbox = entry.second;
		if (WFMath::Contains(extent, rotbox, false) || WFMath::Intersect(extent, rotbox, false)) {
			areas.push_back(rotbox);
		}
	}
}

int Awareness::rasterizeTileLayers(const std::vector<WFMath::RotBox<2>>& entityAreas, int tx, int ty, TileCacheData* tiles, int maxTiles) {
	std::vector<float> vertsVector;
	std::vector<int> trisVector;

	FastLZCompressor comp;
	RasterizationContext rc;

// Tile bounds.
	const float tcs = mCfg.tileSize * mCfg.cs;

	rcConfig tcfg{};
	memcpy(&tcfg, &mCfg, sizeof(tcfg));

	tcfg.bmin[0] = mCfg.bmin[0] + (float) tx * tcs;
	tcfg.bmin[1] = mCfg.bmin[1];
	tcfg.bmin[2] = mCfg.bmin[2] + (float) ty * tcs;
	tcfg.bmax[0] = mCfg.bmin[0] + (float) (tx + 1) * tcs;
	tcfg.bmax[1] = mCfg.bmax[1];
	tcfg.bmax[2] = mCfg.bmin[2] + (float) (ty + 1) * tcs;
	tcfg.bmin[0] -= (float) tcfg.borderSize * tcfg.cs;
	tcfg.bmin[2] -= (float) tcfg.borderSize * tcfg.cs;
	tcfg.bmax[0] += (float) tcfg.borderSize * tcfg.cs;
	tcfg.bmax[2] += (float) tcfg.borderSize * tcfg.cs;

//First define all vertices. Get one extra vertex in each direction so that there's no cutoff at the tile's edges.
	int heightsXMin = (int) std::floor(tcfg.bmin[0]) - 1;
	int heightsXMax = (int) std::ceil(tcfg.bmax[0]) + 1;
	int heightsYMin = (int) std::floor(tcfg.bmin[2]) - 1;
	int heightsYMax = (int) std::ceil(tcfg.bmax[2]) + 1;
	int sizeX = heightsXMax - heightsXMin;
	int sizeY = heightsYMax - heightsYMin;

//Blit height values with 1 meter interval
	std::vector<float> heights(sizeX * sizeY);
	mHeightProvider.blitHeights(heightsXMin, heightsXMax, heightsYMin, heightsYMax, heights);

	float* heightData = heights.data();
	for (int y = heightsYMin; y < heightsYMax; ++y) {
		for (int x = heightsXMin; x < heightsXMax; ++x) {
			vertsVector.push_back(x);
			vertsVector.push_back(*heightData);
			vertsVector.push_back(y);
			heightData++;
		}
	}

//Then define the triangles
	for (int y = 0; y < (sizeY - 1); y++) {
		for (int x = 0; x < (sizeX - 1); x++) {
			size_t vertPtr = (y * sizeX) + x;
			//make a square, including the vertices to the right and below
			trisVector.push_back(vertPtr);
			trisVector.push_back(vertPtr + sizeX);
			trisVector.push_back(vertPtr + 1);

			trisVector.push_back(vertPtr + 1);
			trisVector.push_back(vertPtr + sizeX);
			trisVector.push_back(vertPtr + 1 + sizeX);
		}
	}

	float* verts = vertsVector.data();
	int* tris = trisVector.data();
	auto nverts = vertsVector.size() / 3;
	auto ntris = trisVector.size() / 3;

// Allocate voxel heightfield where we rasterize our input data to.
	rc.solid = rcAllocHeightfield();
	if (!rc.solid) {
		mCtx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'solid'.");
		return 0;
	}
	if (!rcCreateHeightfield(mCtx.get(), *rc.solid, tcfg.width, tcfg.height, tcfg.bmin, tcfg.bmax, tcfg.cs, tcfg.ch)) {
		mCtx->log(RC_LOG_ERROR, "buildNavigation: Could not create solid heightfield.");
		return 0;
	}

// Allocate array that can hold triangle flags.
	rc.triareas = new unsigned char[ntris];
	if (!rc.triareas) {
		mCtx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'm_triareas' (%d).", ntris / 3);
		return 0;
	}

	memset(rc.triareas, 0, ntris * sizeof(unsigned char));
	rcMarkWalkableTriangles(mCtx.get(), tcfg.walkableSlopeAngle, verts, nverts, tris, ntris, rc.triareas);

	rcRasterizeTriangles(mCtx.get(), verts, nverts, tris, rc.triareas, ntris, *rc.solid, tcfg.walkableClimb);

// Once all geometry is rasterized, we do initial pass of filtering to
// remove unwanted overhangs caused by the conservative rasterization
// as well as filter spans where the character cannot possibly stand.

//NOTE: These are disabled for now since we currently only handle a simple 2d height map
//with bounding boxes snapped to the ground. If this changes these calls probably needs to be activated.
//	rcFilterLowHangingWalkableObstacles(m_ctx, tcfg.walkableClimb, *rc.solid);
//	rcFilterLedgeSpans(m_ctx, tcfg.walkableHeight, tcfg.walkableClimb, *rc.solid);
//	rcFilterWalkableLowHeightSpans(m_ctx, tcfg.walkableHeight, *rc.solid);

	rc.chf = rcAllocCompactHeightfield();
	if (!rc.chf) {
		mCtx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'chf'.");
		return 0;
	}
	if (!rcBuildCompactHeightfield(mCtx.get(), tcfg.walkableHeight, tcfg.walkableClimb, *rc.solid, *rc.chf)) {
		mCtx->log(RC_LOG_ERROR, "buildNavigation: Could not build compact data.");
		return 0;
	}

// Erode the walkable area by agent radius.
	if (!rcErodeWalkableArea(mCtx.get(), tcfg.walkableRadius, *rc.chf)) {
		mCtx->log(RC_LOG_ERROR, "buildNavigation: Could not erode.");
		return 0;
	}

// Mark areas.
	for (auto& rotbox: entityAreas) {
		float boxVerts[3 * 4];

		boxVerts[0] = rotbox.getCorner(1).x();
		boxVerts[1] = 0;
		boxVerts[2] = rotbox.getCorner(1).y();

		boxVerts[3] = rotbox.getCorner(3).x();
		boxVerts[4] = 0;
		boxVerts[5] = rotbox.getCorner(3).y();

		boxVerts[6] = rotbox.getCorner(2).x();
		boxVerts[7] = 0;
		boxVerts[8] = rotbox.getCorner(2).y();

		boxVerts[9] = rotbox.getCorner(0).x();
		boxVerts[10] = 0;
		boxVerts[11] = rotbox.getCorner(0).y();

		rcMarkConvexPolyArea(mCtx.get(), boxVerts, 4, tcfg.bmin[1], tcfg.bmax[1], DT_TILECACHE_NULL_AREA, *rc.chf);
	}

	rc.lset = rcAllocHeightfieldLayerSet();
	if (!rc.lset) {
		mCtx->log(RC_LOG_ERROR, "buildNavigation: Out of memory 'lset'.");
		return 0;
	}
	if (!rcBuildHeightfieldLayers(mCtx.get(), *rc.chf, tcfg.borderSize, tcfg.walkableHeight, *rc.lset)) {
		mCtx->log(RC_LOG_ERROR, "buildNavigation: Could not build heighfield layers.");
		return 0;
	}

	rc.ntiles = 0;
	for (int i = 0; i < rcMin(rc.lset->nlayers, MAX_LAYERS); ++i) {
		TileCacheData* tile = &rc.tiles[rc.ntiles++];
		const rcHeightfieldLayer* layer = &rc.lset->layers[i];

		// Store header
		dtTileCacheLayerHeader header{};
		header.magic = DT_TILECACHE_MAGIC;
		header.version = DT_TILECACHE_VERSION;

		// Tile layer location in the navmesh.
		header.tx = tx;
		header.ty = ty;
		header.tlayer = i;
		dtVcopy(header.bmin, layer->bmin);
		dtVcopy(header.bmax, layer->bmax);

		// Tile info.
		header.width = (unsigned char) layer->width;
		header.height = (unsigned char) layer->height;
		header.minx = (unsigned char) layer->minx;
		header.maxx = (unsigned char) layer->maxx;
		header.miny = (unsigned char) layer->miny;
		header.maxy = (unsigned char) layer->maxy;
		header.hmin = (unsigned short) layer->hmin;
		header.hmax = (unsigned short) layer->hmax;

		dtStatus status = dtBuildTileCacheLayer(&comp, &header, layer->heights, layer->areas, layer->cons, &tile->data, &tile->dataSize);
		if (dtStatusFailed(status)) {
			return 0;
		}
	}

// Transfer ownership of tile data from build context to the caller.
	int n = 0;
	for (int i = 0; i < rcMin(rc.ntiles, maxTiles); ++i) {
		tiles[n++] = rc.tiles[i];
		rc.tiles[i].data = nullptr;
		rc.tiles[i].dataSize = 0;
	}

	return n;
}

void Awareness::processTiles(const WFMath::AxisBox<2>& area,
							 const std::function<void(unsigned int, dtTileCachePolyMesh&, float* origin, float cellsize, float cellheight, dtTileCacheLayer& layer)>& processor) const {
	float bmin[]{static_cast<float>(area.lowCorner().x()), -100, static_cast<float>(area.lowCorner().y())};
	float bmax[]{static_cast<float>(area.highCorner().x()), 100, static_cast<float>(area.highCorner().y())};

	dtCompressedTileRef tilesRefs[256];
	int ntiles;
	dtStatus status = mTileCache->queryTiles(bmin, bmax, tilesRefs, &ntiles, 256);
	if (status == DT_SUCCESS) {
		std::vector<const dtCompressedTile*> tiles(ntiles);
		for (int i = 0; i < ntiles; ++i) {
			tiles[i] = mTileCache->getTileByRef(tilesRefs[i]);
		}
		processTiles(tiles, processor);
	}
}

void Awareness::processTile(int tx, int ty, const std::function<void(unsigned int, dtTileCachePolyMesh&, float* origin, float cellsize, float cellheight, dtTileCacheLayer& layer)>& processor) const {
	dtCompressedTileRef tilesRefs[MAX_LAYERS];
	auto ntiles = mTileCache->getTilesAt(tx, ty, tilesRefs, MAX_LAYERS);

	std::vector<const dtCompressedTile*> tiles(ntiles);
	for (int i = 0; i < ntiles; ++i) {
		tiles[i] = mTileCache->getTileByRef(tilesRefs[i]);
	}

	processTiles(tiles, processor);
}

void Awareness::processAllTiles(const std::function<void(unsigned int, dtTileCachePolyMesh&, float* origin, float cellsize, float cellheight, dtTileCacheLayer& layer)>& processor) const {
	auto ntiles = mTileCache->getTileCount();
	std::vector<const dtCompressedTile*> tiles(ntiles);
	for (int i = 0; i < ntiles; ++i) {
		tiles[i] = mTileCache->getTile(i);
	}

	processTiles(tiles, processor);

}

void Awareness::processTiles(std::vector<const dtCompressedTile*> tiles,
							 const std::function<void(unsigned int, dtTileCachePolyMesh&, float* origin, float cellsize, float cellheight, dtTileCacheLayer& layer)>& processor) const {
	struct TileCacheBuildContext {
		inline explicit TileCacheBuildContext(struct dtTileCacheAlloc* a) :
				layer(nullptr), lcset(nullptr), lmesh(nullptr), alloc(a) {
		}

		inline ~TileCacheBuildContext() {
			purge();
		}

		void purge() {
			dtFreeTileCacheLayer(alloc, layer);
			layer = nullptr;
			dtFreeTileCacheContourSet(alloc, lcset);
			lcset = nullptr;
			dtFreeTileCachePolyMesh(alloc, lmesh);
			lmesh = nullptr;
		}

		struct dtTileCacheLayer* layer;
		struct dtTileCacheContourSet* lcset;
		struct dtTileCachePolyMesh* lmesh;
		struct dtTileCacheAlloc* alloc;
	};

	dtTileCacheAlloc* talloc = mTileCache->getAlloc();
	dtTileCacheCompressor* tcomp = mTileCache->getCompressor();
	const dtTileCacheParams* params = mTileCache->getParams();

	for (auto* tile: tiles) {

		talloc->reset();

		TileCacheBuildContext bc(talloc);
		auto walkableClimbVx = (int) (params->walkableClimb / params->ch);
		dtStatus status;

		// Decompress tile layer data.
		status = dtDecompressTileCacheLayer(talloc, tcomp, tile->data, tile->dataSize, &bc.layer);
		if (dtStatusFailed(status))
			return;

		// Build navmesh
		status = dtBuildTileCacheRegions(talloc, *bc.layer, walkableClimbVx);
		if (dtStatusFailed(status))
			return;

		bc.lcset = dtAllocTileCacheContourSet(talloc);
		if (!bc.lcset)
			return;
		status = dtBuildTileCacheContours(talloc, *bc.layer, walkableClimbVx, params->maxSimplificationError, *bc.lcset);
		if (dtStatusFailed(status))
			return;

		bc.lmesh = dtAllocTileCachePolyMesh(talloc);
		if (!bc.lmesh)
			return;
		status = dtBuildTileCachePolyMesh(talloc, *bc.lcset, *bc.lmesh);
		if (dtStatusFailed(status))
			return;

		processor(mTileCache->getTileRef(tile), *bc.lmesh, tile->header->bmin, params->cs, params->ch, *bc.layer);

	}
}

}

