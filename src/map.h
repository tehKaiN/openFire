#ifndef GUARD_OF_MAP_H
#define GUARD_OF_MAP_H

#include <ace/types.h>

#define MAP_LOGIC_WATER   '.'
#define MAP_LOGIC_DIRT    ' '
#define MAP_LOGIC_ROAD    '#'
#define MAP_LOGIC_WALL    '-'
#define MAP_LOGIC_WALL_VERTICAL '|' /* Convenience */
#define MAP_LOGIC_SPAWN0  '0'
#define MAP_LOGIC_SPAWN1  '1'
#define MAP_LOGIC_SPAWN2  '2'
#define MAP_LOGIC_SENTRY0 '$'
#define MAP_LOGIC_SENTRY1 's'
#define MAP_LOGIC_SENTRY2 'S'
#define MAP_LOGIC_FLAG1   'f'
#define MAP_LOGIC_FLAG2   'F'
#define MAP_LOGIC_GATE1   'g'
#define MAP_LOGIC_GATE2   'G'
#define MAP_LOGIC_CAPTURE0 'o'
#define MAP_LOGIC_CAPTURE1 'c'
#define MAP_LOGIC_CAPTURE2 'C'

#define MAP_MAX_SIZE 128

typedef struct _tTile {
	UBYTE ubIdx;  ///< Tileset idx
	UBYTE ubData; ///< Data field. For buildings/gates/spawns used as array idx.
} tMapTile;

typedef struct _tMap {
	char szPath[200];
	FUBYTE fubWidth;
	FUBYTE fubHeight;
	FUBYTE fubControlPointCount;
	FUBYTE fubSpawnCount;
	tMapTile pData[MAP_MAX_SIZE][MAP_MAX_SIZE];
} tMap;

void mapInit(
	IN char *szPath
);

extern tMap g_sMap;

#endif // GUARD_OF_MAP_H