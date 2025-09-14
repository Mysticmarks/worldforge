#include <cassert>
#include <cstring>
#include "DetourNavMeshBuilder.h"
#include "DetourNavMesh.h"

int main()
{
        unsigned char* data = 0;
        int dataSize = 0;
        dtNavMeshCreateParams params;
        memset(&params, 0, sizeof(params));

        // Invalid nvp (too large)
        params.nvp = DT_VERTS_PER_POLYGON + 1;
        dtStatus status = dtCreateNavMeshData(&params, &data, &dataSize);
        assert(dtStatusFailed(status));
        assert(dtStatusDetail(status, DT_INVALID_PARAM));

        // vertCount too large
        memset(&params, 0, sizeof(params));
        params.nvp = DT_VERTS_PER_POLYGON;
        params.vertCount = 0xffff;
        status = dtCreateNavMeshData(&params, &data, &dataSize);
        assert(dtStatusFailed(status));
        assert(dtStatusDetail(status, DT_INVALID_PARAM));

        // Missing vertices pointer
        memset(&params, 0, sizeof(params));
        params.nvp = DT_VERTS_PER_POLYGON;
        params.vertCount = 1;
        params.verts = 0;
        status = dtCreateNavMeshData(&params, &data, &dataSize);
        assert(dtStatusFailed(status));
        assert(dtStatusDetail(status, DT_INVALID_PARAM));

        // Missing polygon data
        unsigned short dummyVerts[3] = {0,0,0};
        memset(&params, 0, sizeof(params));
        params.nvp = DT_VERTS_PER_POLYGON;
        params.vertCount = 1;
        params.verts = dummyVerts;
        params.polyCount = 1;
        params.polys = 0;
        status = dtCreateNavMeshData(&params, &data, &dataSize);
        assert(dtStatusFailed(status));
        assert(dtStatusDetail(status, DT_INVALID_PARAM));

        return 0;
}

