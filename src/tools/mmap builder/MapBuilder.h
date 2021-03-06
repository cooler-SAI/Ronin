/*
 * Copyright (C) 2005-2011 MaNGOS <http://getmangos.com/>
 * Copyright (C) 2008-2013 TrinityCore <http://www.trinitycore.org/>
 * Copyright (C) 2013-2017 Sandshroud <https://github.com/Sandshroud>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _MAP_BUILDER_H
#define _MAP_BUILDER_H

#include <vector>
#include <set>
#include <map>

#include "TerrainBuilder.h"
#include "IntermediateValues.h"

#include "Recast.h"
#include "DetourNavMesh.h"

using namespace VMAP;

// G3D namespace typedefs conflicts with ACE typedefs

namespace MMAP
{
    typedef std::map<G3D::uint32, std::set<G3D::uint32>*> TileList;
    struct Tile
    {
        Tile() : chf(NULL), solid(NULL), cset(NULL), pmesh(NULL), dmesh(NULL) {}
        ~Tile()
        {
            rcFreeCompactHeightfield(chf);
            rcFreeContourSet(cset);
            rcFreeHeightField(solid);
            rcFreePolyMesh(pmesh);
            rcFreePolyMeshDetail(dmesh);
        }
        rcCompactHeightfield* chf;
        rcHeightfield* solid;
        rcContourSet* cset;
        rcPolyMesh* pmesh;
        rcPolyMeshDetail* dmesh;
    };

    class MapBuilder
    {
        public:
            MapBuilder(float maxWalkableAngle   = 55.f,
                bool skipLiquid          = false,
                bool skipContinents      = false,
                bool skipJunkMaps        = true,
                bool skipBattlegrounds   = false,
                bool debugOutput         = false,
                bool bigBaseUnit         = false,
                const char* offMeshFilePath = NULL);

            ~MapBuilder();

            // builds all mmap tiles for the specified map id (ignores skip settings)
            void buildMap(G3D::uint32 mapID);
            void buildMeshFromFile(char* name);

            // builds an mmap tile for the specified map and its mesh
            void buildSingleTile(G3D::uint32 mapID, G3D::uint32 tileX, G3D::uint32 tileY);

            // builds list of maps, then builds all of mmap tiles (based on the skip settings)
            void buildAllMaps(int threads);

        private:
            // detect maps and tiles
            void discoverTiles();
            std::set<G3D::uint32>* getTileList(G3D::uint32 mapID);

            void buildNavMesh(G3D::uint32 mapID, dtNavMesh* &navMesh);

            void buildTile(G3D::uint32 mapID, G3D::uint32 tileX, G3D::uint32 tileY, dtNavMesh* navMesh);

            // move map building
            void buildMoveMapTile(G3D::uint32 mapID,
                G3D::uint32 tileX,
                G3D::uint32 tileY,
                MeshData &meshData,
                float bmin[3],
                float bmax[3],
                dtNavMesh* navMesh);

            void getTileBounds(G3D::uint32 tileX, G3D::uint32 tileY,
                float* verts, int vertCount,
                float* bmin, float* bmax);
            void getGridBounds(G3D::uint32 mapID, G3D::uint32 &minX, G3D::uint32 &minY, G3D::uint32 &maxX, G3D::uint32 &maxY);

            bool shouldSkipMap(G3D::uint32 mapID);
            bool isTransportMap(G3D::uint32 mapID);
            bool shouldSkipTile(G3D::uint32 mapID, G3D::uint32 tileX, G3D::uint32 tileY);

            TerrainBuilder* m_terrainBuilder;
            TileList m_tiles;

            bool m_debugOutput;

            const char* m_offMeshFilePath;
            bool m_skipContinents;
            bool m_skipJunkMaps;
            bool m_skipBattlegrounds;

            float m_maxWalkableAngle;
            bool m_bigBaseUnit;

            // build performance - not really used for now
            rcContext* m_rcContext;
    };

    class MapBuildRequest
    {
        public:
            MapBuildRequest(G3D::uint32 mapId) : _mapId(mapId) {}

            virtual int call()
            {
                /// @ Actually a creative way of unabstracting the class and returning a member variable
                return (int)_mapId;
            }

        private:
            G3D::uint32 _mapId;
    };
}

#endif
