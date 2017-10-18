#ifndef GUARD_OF_GAMESTATES_GAME_SPAWN_H
#define GUARD_OF_GAMESTATES_GAME_SPAWN_H

#include <ace/types.h>

#define SPAWN_BUSY_NOT 0
#define SPAWN_BUSY_SURFACING 1
#define SPAWN_BUSY_BUNKERING 2

#define SPAWN_INVALID 0xFF

typedef struct _tSpawn {
	UBYTE ubTileY;
	UBYTE ubTileX;
	UBYTE ubTeam;
	UBYTE ubBusy;
	UBYTE ubFrame;
	UBYTE ubVehicleType;
} tSpawn;

void spawnManagerCreate(
	IN FUBYTE fubMaxCount
);

void spawnManagerDestroy(void);

UBYTE spawnAdd(
	IN UBYTE ubTileX,
	IN UBYTE ubTileY,
	IN UBYTE ubTeam
);

void spawnCapture(
	IN UBYTE ubSpawnIdx,
	IN UBYTE ubTeam
);

UBYTE spawnGetNearest(
	IN UBYTE ubTileX,
	IN UBYTE ubTileY,
	IN UBYTE ubTeam
);

UBYTE spawnGetAt(
	IN UBYTE ubTileX,
	IN UBYTE ubTileY
);

void spawnSetBusy(
	IN FUBYTE fubSpawnIdx,
	IN FUBYTE fubBusyType,
	IN FUBYTE fubVehicleType
);

void spawnAnimate(
	IN UBYTE ubSpawnIdx
);

void spawnSim(void);

UBYTE spawnIsCoveredByAnyPlayer(
	IN UBYTE ubSpawnIdx
);

extern tSpawn *g_pSpawns;
extern UBYTE g_ubSpawnCount;

#endif // GUARD_OF_GAMESTATES_GAME_SPAWN_H
