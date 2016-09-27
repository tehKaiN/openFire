#include "gamestates/game/map.h"
#include <ace/config.h>
#include <ace/managers/blit.h>
#include <ace/managers/viewport/simplebuffer.h>
#include <ace/utils/extview.h>
#include "gamestates/game/team.h"

#define MAP_TILE_WATER  0
#define MAP_TILE_SPAWN1 1
#define MAP_TILE_SPAWN2 2
// Flag buildings - 'live' & destroyed
#define MAP_TILE_FLAG1L 3
#define MAP_TILE_FLAG2L 4
#define MAP_TILE_FLAGD  5
// Destroyed wall tile
#define MAP_WALLD       6
// Gates - horizontal & vertical, 'live' & destroyed
#define MAP_GATEVL      7
#define MAP_GATEVD      8
#define MAP_GATEHL      9
#define MAP_GATEHD      10
// 16-variant tiles - base codes, ends 16 positions later.
#define MAP_TILE_DIRT   16
#define MAP_TILE_ROAD   32
#define MAP_TILE_WALL   48
#define MAP_TILE_TURRET 64

UBYTE **g_pMap;

UBYTE isWater(UBYTE ubMapTile) {
	return ubMapTile == MAP_LOGIC_WATER;
}

UBYTE isWall(UBYTE ubMapTile) {
	return (
		ubMapTile == MAP_LOGIC_WALL    ||
		ubMapTile == MAP_LOGIC_SENTRY1 ||
		ubMapTile == MAP_LOGIC_SENTRY2 ||
		0
	);
}

UBYTE isRoadFriend(UBYTE ubMapTile) {
	return (
		ubMapTile == MAP_LOGIC_ROAD   ||
		ubMapTile == MAP_LOGIC_SPAWN1 ||
		ubMapTile == MAP_LOGIC_SPAWN2 ||
		ubMapTile == MAP_LOGIC_GATE1  ||
		ubMapTile == MAP_LOGIC_GATE2  ||
		ubMapTile == MAP_LOGIC_FLAG1 ||
		ubMapTile == MAP_LOGIC_FLAG2 ||
		0
	);
}

UBYTE mapCheckNeighbours(UBYTE ubX, UBYTE ubY, UBYTE (*checkFn)(UBYTE)) {
	UBYTE ubTileType;
	UBYTE ubOut;
	const UBYTE ubE = 8;
	const UBYTE ubW = 4;
	const UBYTE ubS = 2;
	const UBYTE ubN = 1;
	
	ubOut = 0;
	ubTileType = g_pMap[ubX][ubY];
	if(ubX && checkFn(g_pMap[ubX+1][ubY]))
		ubOut |= ubE;
	if(ubX-1 < MAP_TILE_WIDTH && checkFn(g_pMap[ubX-1][ubY]))
		ubOut |= ubW;
	if(ubY && checkFn(g_pMap[ubX][ubY-1]))
		ubOut |= ubN;
	if(ubY-1 < MAP_TILE_HEIGHT && checkFn(g_pMap[ubX][ubY+1]))
		ubOut |= ubS;
	return ubOut;
}

void mapCreate(tVPort *pVPort, tBitMap *pTileset, char *szPath) {
	UBYTE x, y, ubTileIdx, ubOutTile;
	FILE *pMapFile;
	tSimpleBufferManager *pManager;
	tBitMap *pBuffer;
	
	logBlockBegin("mapCreate(pVPort: %p, pTileset: %p, szPath: %s)", pVPort, pTileset, szPath);

	pManager = (tSimpleBufferManager*)vPortGetManager(pVPort, VPM_SCROLL);
	pBuffer = pManager->pBuffer;
	
	// First pass - mem alloc
	if(!pMapFile)
		logWrite("ERR: File doesn't exist: %s\n", szPath);
	g_pMap = memAllocFast(sizeof(UBYTE*) * MAP_TILE_WIDTH);
	for(x = 0; x != MAP_TILE_WIDTH; ++x)
		g_pMap[x] = memAllocFast(sizeof(UBYTE) * MAP_TILE_HEIGHT);
	
	// Second pass - read from file
	pMapFile = fopen(szPath, "rb");
	for(y = 0; y != MAP_TILE_HEIGHT; ++y) {
		for(x = 0; x != MAP_TILE_WIDTH; ++x) {
			do
				fread(&ubTileIdx, 1, 1, pMapFile);
				while(ubTileIdx == '\n' || ubTileIdx == '\r');
			g_pMap[x][y] = ubTileIdx;
		}
	}
	fclose(pMapFile);
	
	// Third pass - generate graphics based on logic tiles
	for(x = 0; x != MAP_TILE_WIDTH; ++x) {
		for(y = 0; y != MAP_TILE_HEIGHT; ++y) {
			ubTileIdx = g_pMap[x][y];
			switch(ubTileIdx) {
				case MAP_LOGIC_WATER:
					ubOutTile = MAP_TILE_WATER;
					break;
				case MAP_LOGIC_SPAWN1:
					ubOutTile = MAP_TILE_SPAWN1;
					teamAddSpawn(TEAM_GREEN, x, y);
					break;
				case MAP_LOGIC_SPAWN2:
					ubOutTile = MAP_TILE_SPAWN2;
					teamAddSpawn(TEAM_BROWN, x, y);
					break;
				case MAP_LOGIC_ROAD:
					ubOutTile = MAP_TILE_ROAD + mapCheckNeighbours(x, y, isRoadFriend);
					break;
				case MAP_LOGIC_WALL:
					ubOutTile = MAP_TILE_WALL + mapCheckNeighbours(x, y, isWall);
					break;
				case MAP_LOGIC_FLAG1:
					// TODO: register flag
					ubOutTile = MAP_TILE_FLAG1L;
					break;
				case MAP_LOGIC_FLAG2:
					// TODO: register flag
					ubOutTile = MAP_TILE_FLAG2L;
					break;
				case MAP_LOGIC_SENTRY1:
				case MAP_LOGIC_SENTRY2:
					ubOutTile = MAP_TILE_TURRET;
					// TODO: register turret
					break;
				case MAP_LOGIC_DIRT:
				default:
					ubOutTile = MAP_TILE_DIRT + mapCheckNeighbours(x, y, isWater);
					break;
			}
			blitCopyAligned(
				pTileset, 0, ubOutTile << MAP_TILE_SIZE,
				pBuffer, x << MAP_TILE_SIZE, y << MAP_TILE_SIZE,
				1 << MAP_TILE_SIZE, 1 << MAP_TILE_SIZE
			);
		}
	}
	
	logBlockEnd("mapCreate()");
}

void mapDestroy(void) {
	UBYTE x;
	
	logBlockBegin("mapDestroy()");
	for(x = 0; x != MAP_TILE_WIDTH; ++x) {
		memFree(g_pMap[x], sizeof(UBYTE) * MAP_TILE_HEIGHT);	
	}
	memFree(g_pMap, sizeof(UBYTE*) * MAP_TILE_WIDTH);
	logBlockEnd("mapDestroy()");
}