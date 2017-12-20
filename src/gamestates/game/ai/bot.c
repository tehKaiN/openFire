#include "gamestates/game/ai/bot.h"
#include <fixmath/fix16.h>
#include "gamestates/game/spawn.h"
#include "gamestates/game/ai/heap.h"

#define AI_BOT_STATE_IDLE           0
#define AI_BOT_STATE_MOVING_TO_NODE 1
#define AI_BOT_STATE_NODE_REACHED   2
#define AI_BOT_STATE_HOLDING_POS    3
#define AI_BOT_STATE_BLOCKED        4

static tBot *s_pBots;
static FUBYTE s_fubBotCount;
static FUBYTE s_fubBotLimit;

static inline void astar(tRoute *pRoute, tAiNode *pSrcNode, tAiNode *pDstNode);

static void botSay(tBot *pBot, char *szFmt, ...) {
#ifdef AI_BOT_DEBUG
	char szBfr[100];
	va_list vArgs;
	va_start(vArgs, szFmt);
	vsprintf(szBfr, szFmt, vArgs);
	playerSay(pBot->pPlayer, szBfr, 0);
	va_end(vArgs);
#endif // #ifdef AI_BOT_DEBUG
}

void botManagerCreate(FUBYTE fubBotLimit) {
	logBlockBegin("botManagerCreate(fubBotLimit: %"PRI_FUBYTE")", fubBotLimit);
	s_fubBotCount = 0;
	s_pBots = memAllocFastClear(sizeof(tBot) * fubBotLimit);
	s_fubBotLimit = fubBotLimit;
	logBlockEnd("botManagerCreate()");
}

void botManagerDestroy(void) {
	logBlockBegin("botManagerDestroy()");
	memFree(s_pBots, sizeof(tBot) * s_fubBotLimit);
	logBlockEnd("botManagerDestroy()");
}

void botAdd(char *szName, UBYTE ubTeam) {
	tBot *pBot = &s_pBots[s_fubBotCount];
	pBot->pPlayer = playerAdd(szName, ubTeam);
	if(!pBot->pPlayer) {
		logWrite("ERR: No more room for bots\n");
		return;
	}
	pBot->pNextNode = 0;
	pBot->ubState = AI_BOT_STATE_IDLE;
	pBot->ubTick = 0;
	pBot->uwNextX = 0;
	pBot->uwNextY = 0;
	pBot->ubNextAngle = 0;
	++s_fubBotCount;

	botSay(pBot, "Ich bin ein computer");
}

void botRemoveByName(char *szName) {

}

void botRemoveByPtr(tBot *pBot) {

}

typedef struct _tBotRouteSubcandidate {
	tRoute *pBaseRoute;
	tAiNode *pNextNode;
	UWORD uwCost;
} tBotRouteSubcandidate;

void botSetupRoute(tBot *pBot, tAiNode *pNodeStart, tAiNode *pNodeEnd) {
	logBlockBegin("botSetupRoute()");
	logWrite(
		"From: %hu,%hu to %hu,%hu\n",
		pNodeStart->fubX, pNodeStart->fubY, pNodeEnd->fubX, pNodeEnd->fubY
	);
	astar(&pBot->sRoute, pNodeStart, pNodeEnd);
	logBlockEnd("botSetupRoute()");
}

void botFindNewTarget(tBot *pBot, tAiNode *pNodeToEvade) {

	// Find capture point which is neutral or needs defending or being
	// attacked or nearest to attack
	// TODO remaining variants, prioritize
	tAiNode *pRouteEnd = 0;
	for(FUBYTE i = 0; i != g_fubCaptureNodeCount; ++i) {
		if(
			g_pCaptureNodes[i]->pControlPoint->fubTeam != pBot->pPlayer->ubTeam &&
			g_pCaptureNodes[i] != pNodeToEvade
		) {
			pRouteEnd = g_pCaptureNodes[i];
			break;
		}
	}
	if(!pRouteEnd)
		return;

	botSay(pBot, "New target at %"PRI_FUBYTE",%"PRI_FUBYTE,	pRouteEnd->fubX, pRouteEnd->fubY);
	logWrite(
		"Bot pos: %hu, %hu (%d,%d)\n",
		pBot->pPlayer->sVehicle.uwX, pBot->pPlayer->sVehicle.uwY,
		pBot->pPlayer->sVehicle.uwX >> MAP_TILE_SIZE, pBot->pPlayer->sVehicle.uwY >> MAP_TILE_SIZE
	);
	tAiNode *pRouteStart = aiFindClosestNode(
		pBot->pPlayer->sVehicle.uwX >> MAP_TILE_SIZE,
		pBot->pPlayer->sVehicle.uwY >> MAP_TILE_SIZE
	);
	if(pRouteStart) {
		// Find best route
		botSetupRoute(pBot, pRouteStart, pRouteEnd);

		// Set first node of route as next to reach
		pBot->pNextNode = pBot->sRoute.pNodes[pBot->sRoute.ubCurrNode];
		pBot->uwNextX = (pBot->pNextNode->fubX << MAP_TILE_SIZE) + MAP_HALF_TILE;
		pBot->uwNextY = (pBot->pNextNode->fubY << MAP_TILE_SIZE) + MAP_HALF_TILE;
	}
}

void botProcessDriving(tBot *pBot) {
	switch(pBot->ubState) {
		case AI_BOT_STATE_IDLE:
			botFindNewTarget(pBot, pBot->pNextNode);
			botSay(pBot, "Going to %hu,%hu\n", pBot->pNextNode->fubX, pBot->pNextNode->fubY);
			pBot->ubTick = 10;
			pBot->ubState = AI_BOT_STATE_MOVING_TO_NODE;
			break;
		case AI_BOT_STATE_MOVING_TO_NODE:
			// Update target angle
			if(pBot->ubTick == 10) {
				pBot->ubTick = 0;
				pBot->ubNextAngle = getAngleBetweenPoints(
					pBot->pPlayer->sVehicle.uwX,
					pBot->pPlayer->sVehicle.uwY,
					(pBot->pNextNode->fubX << MAP_TILE_SIZE) + MAP_HALF_TILE,
					(pBot->pNextNode->fubY << MAP_TILE_SIZE) + MAP_HALF_TILE
				);
			}
			else
				++pBot->ubTick;
			// Check if in destination
			if(
				ABS(pBot->uwNextX - pBot->pPlayer->sVehicle.uwX) < (MAP_HALF_TILE/2) &&
				ABS(pBot->uwNextY - pBot->pPlayer->sVehicle.uwY) < (MAP_HALF_TILE/2)
			) {
				pBot->pPlayer->sSteerRequest.ubForward = 0;
				pBot->pPlayer->sSteerRequest.ubBackward = 0;
				pBot->pPlayer->sSteerRequest.ubLeft = 0;
				pBot->pPlayer->sSteerRequest.ubRight = 0;
				pBot->ubState = AI_BOT_STATE_NODE_REACHED;
				break;
			}

			// Steer to proper angle
			BYTE bDir = getDeltaAngleDirection(pBot->pPlayer->sVehicle.ubBodyAngle, pBot->ubNextAngle, 1);
			if(bDir) {
				if(bDir > 0) {
					pBot->pPlayer->sSteerRequest.ubLeft = 0;
					pBot->pPlayer->sSteerRequest.ubRight = 1;
				}
				else if(bDir < 0) {
					pBot->pPlayer->sSteerRequest.ubLeft = 1;
					pBot->pPlayer->sSteerRequest.ubRight = 0;
				}
				break;
			}

			// Check field in front for collisions
			pBot->pPlayer->sSteerRequest.ubLeft = 0;
			pBot->pPlayer->sSteerRequest.ubRight = 0;
			UWORD uwChkX = pBot->pPlayer->sVehicle.uwX + fix16_to_int(
				(3*MAP_FULL_TILE)/2 * ccos(pBot->pPlayer->sVehicle.ubBodyAngle)
			);
			UWORD uwChkY = pBot->pPlayer->sVehicle.uwY + fix16_to_int(
				(3*MAP_FULL_TILE)/2 * csin(pBot->pPlayer->sVehicle.ubBodyAngle)
			);
			if(playerAnyNearPoint(uwChkX, uwChkY, MAP_FULL_TILE)) {
				pBot->ubState = AI_BOT_STATE_BLOCKED;
				pBot->ubTick = 0;
				pBot->pPlayer->sSteerRequest.ubForward = 0;
				break;
			}

			// Move until close to node
			pBot->pPlayer->sSteerRequest.ubForward = 1;
			break;
		case AI_BOT_STATE_NODE_REACHED:
			if(!pBot->sRoute.ubCurrNode) {
				// Last node from route - hold pos
				pBot->ubTick = 0;
				pBot->ubState = AI_BOT_STATE_HOLDING_POS;
				botSay(pBot, "Destination reached - holding pos");
			}
			else {
				// Get next node from route
				--pBot->sRoute.ubCurrNode;
				pBot->pNextNode = pBot->sRoute.pNodes[pBot->sRoute.ubCurrNode];
				pBot->uwNextX = (pBot->pNextNode->fubX << MAP_TILE_SIZE) + MAP_HALF_TILE;
				pBot->uwNextY = (pBot->pNextNode->fubY << MAP_TILE_SIZE) + MAP_HALF_TILE;
				pBot->ubTick = 10;
				pBot->ubState = AI_BOT_STATE_MOVING_TO_NODE;
				botSay(pBot, "Moving to next pos: %hu, %hu\n", pBot->pNextNode->fubX, pBot->pNextNode->fubY);
			}
			break;
		case AI_BOT_STATE_HOLDING_POS:
			// Move to tile next to it to prevent blocking other players/bots
			// If point has been captured start measuring ticks
			if(pBot->ubTick >= 200) {
				tAiNode *pLastNode = pBot->sRoute.pNodes[0];
				// After some ticks check if work is done on this point
				if(pLastNode->pControlPoint->fubTeam == pBot->pPlayer->ubTeam) {
					// If so, go to next point
					botFindNewTarget(pBot, 0);
					if(pBot->pNextNode)
						pBot->ubState = AI_BOT_STATE_MOVING_TO_NODE;
				}
				else
					botSay(pBot, "capturing...");
				pBot->ubTick = 0;
			}
			else
				++pBot->ubTick;
			break;
		case AI_BOT_STATE_BLOCKED: {
			UWORD uwChkX = pBot->pPlayer->sVehicle.uwX + fix16_to_int(
				(3*MAP_FULL_TILE)/2 * ccos(pBot->pPlayer->sVehicle.ubBodyAngle)
			);
			UWORD uwChkY = pBot->pPlayer->sVehicle.uwY + fix16_to_int(
				(3*MAP_FULL_TILE)/2 * csin(pBot->pPlayer->sVehicle.ubBodyAngle)
			);
			if(playerAnyNearPoint(uwChkX, uwChkY, MAP_FULL_TILE)) {
				if(pBot->ubTick == 50) {
					// Change target & route
					botFindNewTarget(pBot, pBot->pNextNode);
					if(!pBot->pNextNode)
						pBot->ubState = AI_BOT_STATE_IDLE;
					else
						pBot->ubState = AI_BOT_STATE_MOVING_TO_NODE;
					pBot->ubTick = 0;
				}
				else
					++pBot->ubTick;
			}
			else {
				// Continue your route
				pBot->ubState = AI_BOT_STATE_MOVING_TO_NODE;
				pBot->ubTick = 0;
			}
		} break;
	}
}

void botProcessLimbo(tBot *pBot) {
	if(pBot->pPlayer->uwCooldown)
		return;
	// Find some place to go - e.g. capture point
	botFindNewTarget(pBot, 0);
	// Find nearest spawn point
	pBot->pPlayer->ubSpawnIdx = spawnGetNearest(
		pBot->pNextNode->fubX, pBot->pNextNode->fubY,
		pBot->pPlayer->ubTeam
	);
	// After arriving at surface, recalculate where bot is going
	pBot->ubState = AI_BOT_STATE_IDLE;
	// Make sure noone from own team stands at spawn
	// TODO: spawn kill ppl from other team
	if(!spawnIsCoveredByAnyPlayer(pBot->pPlayer->ubSpawnIdx)) {
		playerSelectVehicle(pBot->pPlayer, VEHICLE_TYPE_TANK);
		botSay(pBot, "Surfacing...");
	}
}

void botProcess(void) {
	for(FUBYTE i = 0; i != s_fubBotCount; ++i) {
		tBot *pBot = &s_pBots[i];
		switch(pBot->pPlayer->ubState) {
			case PLAYER_STATE_DRIVING:
				botProcessDriving(pBot);
				break;
			case PLAYER_STATE_LIMBO:
				botProcessLimbo(pBot);
				break;
		}
	}
}

static inline void astar(tRoute *pRoute, tAiNode *pSrcNode, tAiNode *pDstNode) {
	tHeap *pFrontier = heapCreate(100);
	tAiNode *pCameFrom[AI_MAX_NODES];
	UWORD pCostSoFar[AI_MAX_NODES];
	memset(pCostSoFar, 0xFFFF, AI_MAX_NODES);

	pCameFrom[pSrcNode - g_pNodes] = 0;
	pCostSoFar[pSrcNode - g_pNodes] = 0;

	heapPush(pFrontier, pSrcNode, 0);

	while(pFrontier->uwCount) {
		tAiNode *pCurrNode = heapPop(pFrontier);
		if(pCurrNode == pDstNode) {
			break;
		}

		for(UWORD i = 0; i <= AI_MAX_NODES; ++i) {
			tAiNode *pNextNode = &g_pNodes[i];
			if(pNextNode == pCurrNode)
				continue;
			// new_cost = cost_so_far[current] + graph.cost(current, next)
			UWORD uwCost = pCostSoFar[pCurrNode - g_pNodes] + aiGetCostBetweenNodes(pCurrNode, pNextNode);
			// if next not in cost_so_far or new_cost < cost_so_far[next]:
			if(uwCost < pCostSoFar[pNextNode - g_pNodes]) {
				// cost_so_far[next] = new_cost
				pCostSoFar[pNextNode - g_pNodes] = uwCost;
				// priority = new_cost + heuristic(goal, next)
				UWORD uwPriority = uwCost + aiGetCostBetweenNodes(pNextNode, pDstNode);
				// frontier.put(next, priority)
				heapPush(pFrontier, pNextNode, uwPriority);
				// came_from[next] = current
				pCameFrom[pNextNode - g_pNodes] = pCurrNode;
			}
		}
	}

	pRoute->uwCost = pCostSoFar[pDstNode - g_pNodes];
	pRoute->pNodes[0] = pDstNode;
	pRoute->ubNodeCount = 1;
	tAiNode *pPrev = pCameFrom[pDstNode - g_pNodes];
	logWrite("Astar: %hu,%hu", pDstNode->fubX, pDstNode->fubY);
	while(pPrev) {
		logWrite(" <- %hu,%hu", pPrev->fubX, pPrev->fubY);
		pRoute->pNodes[pRoute->ubNodeCount] = pPrev;
		++pRoute->ubNodeCount;
		pPrev = pCameFrom[pPrev - g_pNodes];
	}
	logWrite(" (count: %hu)\n", pRoute->ubNodeCount);
	pRoute->ubCurrNode = pRoute->ubNodeCount-1;

	heapDestroy(pFrontier);
}
