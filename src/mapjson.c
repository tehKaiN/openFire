#include "mapjson.h"
#include <ace/macros.h>
#include <ace/managers/log.h>
#include "map.h"
#include "gamestates/game/control.h"
#include "gamestates/game/building.h"

UBYTE mapJsonGetMeta(const tJson *pJson, tMap *pMap) {
	logBlockBegin(
		"mapJsonGetMeta(pJson: %p, pMap: %p",
		pJson, pMap
	);

	UWORD uwTokWidth = jsonGetDom(pJson, "width");
	UWORD uwTokHeight = jsonGetDom(pJson, "height");
	UWORD uwTokPts = jsonGetDom(pJson, "controlPoints");

	if(!uwTokWidth || !uwTokHeight || !uwTokPts) {
		logWrite("ERR: Malformed JSON!");
		logBlockEnd("mapJsonGetMeta()");
		return 0;
	}

	pMap->fubWidth = jsonTokToUlong(pJson, uwTokWidth, 10);
	pMap->fubHeight = jsonTokToUlong(pJson, uwTokHeight, 10);
	pMap->fubControlPointCount = pJson->pTokens[uwTokPts].size;

	logBlockEnd("mapJsonGetMeta()");
	return 1;
}

void mapJsonReadTiles(const tJson *pJson, tMap *pMap) {
	UWORD uwTokTiles = jsonGetDom(pJson, "tiles");
	if(!uwTokTiles) {
		logWrite("ERR: JSON 'tiles' array not found!\n");
	}

	// Tiles found - check row count
	if(pJson->pTokens[uwTokTiles].size != pMap->fubHeight) {
		logWrite(
			"ERR: tile rows provided: %d, expected %"PRI_FUBYTE"\n",
			pJson->pTokens[uwTokTiles].size, pMap->fubHeight
		);
		return;
	}

	// Do some reading
	pMap->fubSpawnCount = 0;
	UWORD uwTokRow = jsonGetElementInArray(pJson, uwTokTiles, 0);
	for(FUBYTE y = 0; y < pMap->fubHeight; ++y) {
		jsmntok_t *pTokRow = &pJson->pTokens[uwTokRow+y];
		FUWORD fuwWidth = pTokRow->end - pTokRow->start;
		if(pTokRow->type != JSMN_STRING || fuwWidth != pMap->fubWidth) {
			logWrite(
				"ERR: Malformed row @y %"PRI_FUBYTE": %d(%"PRI_FUBYTE")\n",
				y, pTokRow->type, fuwWidth
			);
			return;
		}

		// Read row to logic tiles
		for(FUBYTE x = 0; x < fuwWidth; ++x) {
			pMap->pData[x][y].ubIdx = (UBYTE)pJson->szData[pTokRow->start + x];
			pMap->pData[x][y].ubData = BUILDING_IDX_INVALID;
			if(
				pMap->pData[x][y].ubIdx == MAP_LOGIC_SPAWN0 ||
				pMap->pData[x][y].ubIdx == MAP_LOGIC_SPAWN1 ||
				pMap->pData[x][y].ubIdx == MAP_LOGIC_SPAWN2
			)
				++pMap->fubSpawnCount;
			else if(pMap->pData[x][y].ubIdx == MAP_LOGIC_WALL_VERTICAL)
				pMap->pData[x][y].ubIdx = MAP_LOGIC_WALL;
		}
	}
}

void mapJsonReadControlPoints(const tJson *pJson) {
	logBlockBegin("mapJsonReadControlPoints(pJson: %p)", pJson);
	UWORD uwTokPts = jsonGetDom(pJson, "controlPoints");
	if(!uwTokPts || pJson->pTokens[uwTokPts].type != JSMN_ARRAY) {
		logWrite("ERR: JSON controlPoints array not found!\n");
		logBlockEnd("mapJsonReadControlPoints()");
		return;
	}

	FUBYTE fubControlPointCount = pJson->pTokens[uwTokPts].size;
	for(FUBYTE fubPt = 0; fubPt != fubControlPointCount; ++fubPt) {
		UWORD uwTokPoint = jsonGetElementInArray(pJson, uwTokPts, fubPt);
		if(!uwTokPoint || pJson->pTokens[uwTokPoint].type != JSMN_OBJECT) {
			logWrite("ERR: Malformed control point: %"PRI_FUBYTE"\n", fubPt);
			logBlockEnd("mapJsonReadControlPoints()");
			return;
		}

		UWORD uwTokPtName = jsonGetElementInStruct(pJson, uwTokPoint, "name");
		UWORD uwTokPtCapture = jsonGetElementInStruct(pJson, uwTokPoint, "capture");
		UWORD uwTokPtPoly = jsonGetElementInStruct(pJson, uwTokPoint, "polygon");
		if(!uwTokPtName || !uwTokPtCapture || !uwTokPtPoly) {
			logWrite("ERR: Missing properties in control point: %"PRI_FUBYTE"\n", fubPt);
			logBlockEnd("mapJsonReadControlPoints()");
			return;
		}

		// Name
		char szControlName[CONTROL_NAME_MAX];
		jsonTokStrCpy(pJson, uwTokPtName, szControlName, CONTROL_NAME_MAX);

		// Control point
		if(
			pJson->pTokens[uwTokPtCapture].type != JSMN_ARRAY ||
			pJson->pTokens[uwTokPtCapture].size != 2
		) {
			logWrite(
				"ERR: capture point not a point: '%.*s'\n",
				pJson->pTokens[uwTokPtCapture].end - pJson->pTokens[uwTokPtCapture].start,
				pJson->szData + pJson->pTokens[uwTokPtCapture].start
			);
			logBlockEnd("mapJsonReadControlPoints()");
			return;
		}
		FUBYTE fubCaptureX = jsonTokToUlong(pJson, uwTokPtCapture+1, 10);
		FUBYTE fubCaptureY = jsonTokToUlong(pJson, uwTokPtCapture+2, 10);

		// Polygon
		FUBYTE fubPolyPointCnt = pJson->pTokens[uwTokPtPoly].size;
		tUbCoordYX *pPolyPoints = memAllocFast(fubPolyPointCnt * sizeof(tUbCoordYX));
		for(FUBYTE pp = 0; pp != fubPolyPointCnt; ++pp) {
			UWORD uwTokPolyPoint = jsonGetElementInArray(pJson, uwTokPtPoly, pp);
			if(
				!uwTokPolyPoint ||
				pJson->pTokens[uwTokPolyPoint].type != JSMN_ARRAY ||
				pJson->pTokens[uwTokPolyPoint].size != 2
			) {
				logWrite(
					"ERR: polygon point not a point %"PRI_FUBYTE" (%d): '%.*s'",
					pp, pJson->pTokens[uwTokPolyPoint].type,
					pJson->pTokens[uwTokPolyPoint].end - pJson->pTokens[uwTokPolyPoint].start,
					pJson->szData + pJson->pTokens[uwTokPolyPoint].start
				);
				memFree(pPolyPoints, fubPolyPointCnt * sizeof(tUbCoordYX));
				logBlockEnd("mapJsonReadControlPoints()");
				return;
			}
			pPolyPoints[pp].sUbCoord.ubX = jsonTokToUlong(pJson, uwTokPolyPoint+1, 10);
			pPolyPoints[pp].sUbCoord.ubY = jsonTokToUlong(pJson, uwTokPolyPoint+2, 10);
		}

		if(!fubPolyPointCnt) {
			logWrite("ERR: No polygon points supplied @point %"PRI_FUBYTE"!\n", fubPt);
			memFree(pPolyPoints, fubPolyPointCnt * sizeof(tUbCoordYX));
			logBlockEnd("mapJsonReadControlPoints()");
			return;
		}
		if(!fubCaptureX && !fubCaptureY) {
			logWrite("ERR: No capture point supplied @point %"PRI_FUBYTE"!\n", fubPt);
			memFree(pPolyPoints, fubPolyPointCnt * sizeof(tUbCoordYX));
			logBlockEnd("mapJsonReadControlPoints()");
			return;
		}
		if(!strlen(szControlName)) {
			logWrite("ERR: No control point name! @point %"PRI_FUBYTE"\n", fubPt);
			memFree(pPolyPoints, fubPolyPointCnt * sizeof(tUbCoordYX));
			logBlockEnd("mapJsonReadControlPoints()");
			return;
		}
		// Close polygon
		++fubPolyPointCnt;
		pPolyPoints[fubPolyPointCnt-1].uwYX = pPolyPoints[0].uwYX;
		controlAddPoint(
			szControlName, fubCaptureX, fubCaptureY, fubPolyPointCnt, pPolyPoints
		);
		memFree(pPolyPoints, (fubPolyPointCnt-1) * sizeof(tUbCoordYX));
	}
}
