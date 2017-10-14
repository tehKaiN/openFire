#include "gamestates/game/game.h"
#include <hardware/intbits.h> // INTB_COPER
#include <ace/macros.h>
#include <ace/managers/copper.h>
#include <ace/managers/blit.h>
#include <ace/managers/viewport/simplebuffer.h>
#include <ace/managers/key.h>
#include <ace/managers/game.h>
#include <ace/managers/rand.h>
#include <ace/utils/extview.h>
#include <ace/utils/palette.h>
#include "gamestates/game/map.h"
#include "gamestates/game/vehicle.h"
#include "gamestates/game/player.h"
#include "gamestates/game/team.h"
#include "gamestates/game/projectile.h"
#include "gamestates/game/data.h"
#include "gamestates/game/hud.h"
#include "cursor.h"
#include "gamestates/game/turret.h"
#include "gamestates/game/spawn.h"
#include "gamestates/game/control.h"
#include "gamestates/game/console.h"

// Viewport stuff
tView *g_pWorldView;
tVPort *s_pWorldMainVPort;
tSimpleBufferManager *g_pWorldMainBfr;
tCameraManager *g_pWorldCamera;
tBitMap *s_pTiles;

// Silo highlight
// TODO: struct?
tBob *s_pSiloHighlight;
UBYTE s_ubWasSiloHighlighted;
UBYTE g_ubDoSiloHighlight;
UWORD g_uwSiloHighlightTileY;
UWORD g_uwSiloHighlightTileX;

// Speed logging
#define SPEED_LOG
tAvg *s_pDrawAvgExplosions;
tAvg *s_pDrawAvgProjectiles;
tAvg *s_pDrawAvgVehicles;
tAvg *s_pUndrawAvgExplosions;
tAvg *s_pUndrawAvgProjectiles;
tAvg *s_pUndrawAvgVehicles;
tAvg *s_pProcessAvgCursor;
tAvg *s_pProcessAvgDataRecv;
tAvg *s_pProcessAvgInput;
tAvg *s_pProcessAvgSpawn;
tAvg *s_pProcessAvgPlayer;
tAvg *s_pProcessAvgControl;
tAvg *s_pProcessAvgTurret;
tAvg *s_pProcessAvgProjectile;
tAvg *s_pProcessAvgDataSend;
tAvg *s_pAvgRedrawControl;
tAvg *s_pAvgUpdateSprites;
tAvg *s_pProcessAvgHud;

UWORD uwLimboX;
UWORD uwLimboY;

ULONG g_ulGameFrame;

void displayPrepareLimbo(FUBYTE fubSpawnIdx) {
	cursorSetConstraints(0,0, 320, 255);
	hudChangeState(HUD_STATE_SELECTING);

	if(fubSpawnIdx == SPAWN_INVALID)
		g_pLocalPlayer->ubSpawnIdx = spawnGetNearest(
			g_pLocalPlayer->sVehicle.uwX >> MAP_TILE_SIZE,
			g_pLocalPlayer->sVehicle.uwY >> MAP_TILE_SIZE,
			g_pLocalPlayer->ubTeam
		);
	else
		g_pLocalPlayer->ubSpawnIdx = fubSpawnIdx;

	uwLimboX = MAX(0, (g_pSpawns[g_pLocalPlayer->ubSpawnIdx].ubTileX << MAP_TILE_SIZE) + MAP_HALF_TILE - (WORLD_VPORT_WIDTH/2));
	uwLimboY = MAX(0, (g_pSpawns[g_pLocalPlayer->ubSpawnIdx].ubTileY << MAP_TILE_SIZE) + MAP_HALF_TILE - (WORLD_VPORT_HEIGHT/2));
}

void displayPrepareDriving(void) {
	cursorSetConstraints(0, 0, 320, 191);
	hudChangeState(HUD_STATE_DRIVING);
}

void worldUndraw(void) {
	UBYTE ubPlayer;

	logAvgBegin(s_pUndrawAvgExplosions);
	explosionsUndraw(g_pWorldMainBfr);
	logAvgEnd(s_pUndrawAvgExplosions);

	logAvgBegin(s_pUndrawAvgProjectiles);
	projectileUndraw();
	logAvgEnd(s_pUndrawAvgProjectiles);

	// Vehicles
	logAvgBegin(s_pUndrawAvgVehicles);
	for(ubPlayer = g_ubPlayerLimit; ubPlayer--;)
		vehicleUndraw(&g_pPlayers[ubPlayer].sVehicle);
	logAvgEnd(s_pUndrawAvgVehicles);

	// Silo highlight
	if(s_ubWasSiloHighlighted) {
		bobUndraw(
			s_pSiloHighlight, g_pWorldMainBfr
		);
	}
}

void gsGameCreate(void) {
	logBlockBegin("gsGameCreate()");
	randInit(2184);

	// Prepare view
	// Must be before mapCreate 'cuz turretListCreate() needs copperlist
	g_pWorldView = viewCreate(0,
		TAG_VIEW_GLOBAL_CLUT, 1,
		TAG_VIEW_COPLIST_MODE, VIEW_COPLIST_MODE_RAW,
		TAG_VIEW_COPLIST_RAW_COUNT, WORLD_COP_SIZE,
		TAG_DONE
	);

	// Load gfx
	s_pTiles = bitmapCreateFromFile("data/tiles.bm");
	s_pSiloHighlight = bobUniqueCreate("data/silohighlight.bm", "data/silohighlight.msk", 0, 0);

	teamsInit();

	mapCreate("data/maps/fubar.json");
	// Create viewports
	s_pWorldMainVPort = vPortCreate(0,
		TAG_VPORT_VIEW, g_pWorldView,
		TAG_VPORT_HEIGHT, WORLD_VPORT_HEIGHT,
		TAG_VPORT_BPP, WORLD_BPP,
		TAG_DONE
	);
	g_pWorldMainBfr = simpleBufferCreate(0,
		TAG_SIMPLEBUFFER_VPORT, s_pWorldMainVPort,
		TAG_SIMPLEBUFFER_BOUND_WIDTH, g_fubMapTileWidth << MAP_TILE_SIZE,
		TAG_SIMPLEBUFFER_BOUND_HEIGHT, g_fubMapTileHeight << MAP_TILE_SIZE,
		TAG_SIMPLEBUFFER_COPLIST_OFFSET, WORLD_COP_VPMAIN_POS,
		TAG_SIMPLEBUFFER_BITMAP_FLAGS, BMF_INTERLEAVED,
		TAG_DONE
	);
	if(!g_pWorldMainBfr) {
		logWrite("Buffer creation failed");
		gamePopState();
		return;
	}
	g_pWorldCamera = g_pWorldMainBfr->pCameraManager;
	mapSetSrcDst(s_pTiles, g_pWorldMainBfr->pBuffer);
	paletteLoad("data/game.plt", s_pWorldMainVPort->pPalette, 16);
	paletteLoad("data/openfire_sprites.plt", &s_pWorldMainVPort->pPalette[16], 16);
	hudCreate();

	// Enabling sprite DMA
	tCopCmd *pSpriteEnList = &g_pWorldView->pCopList->pBackBfr->pList[WORLD_COP_SPRITEEN_POS];
	copSetMove(&pSpriteEnList[0].sMove, &custom.dmacon, BITSET | DMAF_SPRITE);
	CopyMemQuick(
		&g_pWorldView->pCopList->pBackBfr->pList[WORLD_COP_SPRITEEN_POS],
		&g_pWorldView->pCopList->pFrontBfr->pList[WORLD_COP_SPRITEEN_POS],
		sizeof(tCopCmd)
	);
	copRawDisableSprites(g_pWorldView->pCopList, 251, WORLD_COP_SPRITEEN_POS+1);

	// Crosshair stuff
	cursorCreate(g_pWorldView, 2, "data/crosshair.bm", WORLD_COP_CROSS_POS);

	// Explosions
	explosionsCreate();

	#ifdef SPEED_LOG
	s_pDrawAvgExplosions = logAvgCreate("draw explosions", 50);
	s_pDrawAvgProjectiles = logAvgCreate("draw projectiles", 50);
	s_pDrawAvgVehicles = logAvgCreate("draw vehicles", 50);

	s_pUndrawAvgExplosions = logAvgCreate("undraw explosions", 50);
	s_pUndrawAvgProjectiles = logAvgCreate("undraw projectiles", 50);
	s_pUndrawAvgVehicles = logAvgCreate("undraw vehicles", 50);

	s_pProcessAvgCursor = logAvgCreate("cursor", 50);
	s_pProcessAvgDataRecv = logAvgCreate("data recv", 50);
	s_pProcessAvgInput = logAvgCreate("input", 50);
	s_pProcessAvgSpawn = logAvgCreate("spawn", 50);
	s_pProcessAvgPlayer = logAvgCreate("player", 50);
	s_pProcessAvgControl = logAvgCreate("control", 50);
	s_pProcessAvgTurret = logAvgCreate("turret", 50);
	s_pProcessAvgProjectile = logAvgCreate("projectile", 50);
	s_pProcessAvgDataSend = logAvgCreate("data send", 50);
	s_pAvgRedrawControl = logAvgCreate("redraw control points", 50);
	s_pAvgUpdateSprites = logAvgCreate("update sprites", 50);
	s_pProcessAvgHud = logAvgCreate("hud update", 50);
	#endif

	// Initial values
	s_ubWasSiloHighlighted = 0;
	g_ubDoSiloHighlight = 0;
	g_ulGameFrame = 0;

	// Add players
	playerListCreate(8);
	g_pLocalPlayer = playerAdd("player", TEAM_BLUE);
	for(FUBYTE i = 0; i != 7; ++i) {
		char szName[10];
		sprintf(szName, "player%hhu", i);
		playerAdd(szName, TEAM_BLUE);
	}

	// Now that world buffer is created, do the first draw
	mapRedraw();
	displayPrepareLimbo(SPAWN_INVALID);

	// Get some speed out of unnecessary DMA
	custom.dmacon = BITCLR | DMAF_DISK;

	viewLoad(g_pWorldView);
	logBlockEnd("gsGameCreate()");
}

void gsGameLoop(void) {
	// Quit?
	if(keyCheck(KEY_ESCAPE)) {
		gamePopState(); // Pop to precalc so it may free stuff and quit
		return;
	}
	// Steering-irrelevant player input
	if(keyUse(KEY_C))
		bitmapSaveBmp(g_pWorldMainBfr->pBuffer, s_pWorldMainVPort->pPalette, "debug/bufDump.bmp");
	if(keyUse(KEY_L))
		copDumpBfr(g_pWorldView->pCopList->pBackBfr);
	if(keyUse(KEY_T))
		consoleChatBegin();

	++g_ulGameFrame;
	logAvgBegin(s_pProcessAvgDataRecv);
	dataRecv(); // Receives positions of other players from server
	logAvgEnd(s_pProcessAvgDataRecv);

  logAvgBegin(s_pProcessAvgCursor);
  cursorUpdate();
  logAvgEnd(s_pProcessAvgCursor);

	logAvgBegin(s_pProcessAvgInput);
	playerLocalProcessInput(); // Steer requests, chat, limbo
	logAvgEnd(s_pProcessAvgInput);

	logAvgBegin(s_pProcessAvgDataSend);
	dataSend(); // Send input requests to server
	logAvgEnd(s_pProcessAvgDataSend);

	logAvgBegin(s_pProcessAvgSpawn);
	spawnSim();
	logAvgEnd(s_pProcessAvgSpawn);

	logAvgBegin(s_pProcessAvgControl);
	controlSim();
	logAvgEnd(s_pProcessAvgControl);

	if(g_pLocalPlayer->ubState != PLAYER_STATE_LIMBO) {
		UWORD uwLocalX, uwLocalY;
		cameraCenterAt(
			g_pWorldCamera,
			g_pLocalPlayer->sVehicle.uwX & 0xFFFE, g_pLocalPlayer->sVehicle.uwY
		);
	}
	else {
		WORD wDx = CLAMP(uwLimboX - g_pWorldCamera->uPos.sUwCoord.uwX, -2, 2);
		WORD wDy = CLAMP(uwLimboY - g_pWorldCamera->uPos.sUwCoord.uwY, -2, 2);
		cameraMoveBy(g_pWorldCamera, wDx, wDy);
	}

	// Start refreshing gfx at hud
	vPortWaitForEnd(s_pWorldMainVPort);
	worldUndraw(); // This will take almost whole HUD time

	mapUpdateTiles();

	logAvgBegin(s_pAvgRedrawControl);
	controlRedrawPoints();
	logAvgEnd(s_pAvgRedrawControl);

	// Silo highlight
	if(g_ubDoSiloHighlight) {
		bobDraw(
			s_pSiloHighlight, g_pWorldMainBfr,
			g_uwSiloHighlightTileX << MAP_TILE_SIZE,
			g_uwSiloHighlightTileY << MAP_TILE_SIZE
		);
	}

	logAvgBegin(s_pProcessAvgPlayer);
	playerSim(); // Players & vehicles states
	logAvgEnd(s_pProcessAvgPlayer);

	logAvgBegin(s_pDrawAvgProjectiles);
	projectileDraw();
	logAvgEnd(s_pDrawAvgProjectiles);

	logAvgBegin(s_pDrawAvgExplosions);
	explosionsDraw(g_pWorldMainBfr);
	logAvgEnd(s_pDrawAvgExplosions);

	logAvgBegin(s_pAvgUpdateSprites);
	turretUpdateSprites();
	logAvgEnd(s_pAvgUpdateSprites);

	s_ubWasSiloHighlighted = g_ubDoSiloHighlight;

	// This should be done on vblank interrupt
	viewProcessManagers(g_pWorldView);
	copProcessBlocks();

	logAvgBegin(s_pProcessAvgTurret);
	turretSim();               // Turrets: targeting, rotation & projectile spawn
	logAvgEnd(s_pProcessAvgTurret);

	logAvgBegin(s_pProcessAvgProjectile);
	projectileSim();           // Projectiles: new positions, damage
	logAvgEnd(s_pProcessAvgProjectile);

	logAvgBegin(s_pProcessAvgHud);
	hudUpdate();
	logAvgEnd(s_pProcessAvgHud);
}

void gsGameDestroy(void) {
	// Return DMA to correct state
	custom.dmacon = BITSET | DMAF_DISK;
	logBlockBegin("gsGameDestroy()");

	cursorDestroy();
	hudDestroy();
	explosionsDestroy();
	viewDestroy(g_pWorldView);
	bobUniqueDestroy(s_pSiloHighlight);
	bitmapDestroy(s_pTiles);

	#ifdef SPEED_LOG
	logAvgDestroy(s_pUndrawAvgExplosions);
	logAvgDestroy(s_pUndrawAvgProjectiles);
	logAvgDestroy(s_pUndrawAvgVehicles);
	logAvgDestroy(s_pDrawAvgVehicles);
	logAvgDestroy(s_pDrawAvgProjectiles);
	logAvgDestroy(s_pDrawAvgExplosions);
	logAvgDestroy(s_pProcessAvgCursor);
	logAvgDestroy(s_pProcessAvgDataRecv);
	logAvgDestroy(s_pProcessAvgInput);
	logAvgDestroy(s_pProcessAvgSpawn);
	logAvgDestroy(s_pProcessAvgPlayer);
	logAvgDestroy(s_pProcessAvgControl);
	logAvgDestroy(s_pProcessAvgTurret);
	logAvgDestroy(s_pProcessAvgProjectile);
	logAvgDestroy(s_pProcessAvgDataSend);
	logAvgDestroy(s_pAvgRedrawControl);
	logAvgDestroy(s_pAvgUpdateSprites);
	logAvgDestroy(s_pProcessAvgHud);
	#endif

	mapDestroy();
	playerListDestroy();

	logBlockEnd("gsGameDestroy()");
}
