#include "gamestates/game/hud.h"
#include <ace/managers/blit.h>
#include <ace/managers/viewport/simplebuffer.h>
#include <ace/utils/bitmap.h>
#include <ace/utils/custom.h>
#include "gamestates/game/game.h"
#include "gamestates/game/player.h"
#include "gamestates/game/console.h"
#include "vehicletypes.h"

static tVPort *s_pHudVPort;
tSimpleBufferManager *g_pHudBfr;
static tBitMap *s_pHudPanels[2];
static UBYTE s_ubHudState, s_ubHudPrevState;
static tFont *s_pHudFont;
static tTextBitMap *s_pSpawnTextBfr;

static UWORD s_uwPrevTickets[2];
static UBYTE s_ubFrame;
static UBYTE s_isFpsEnabled;

void hudCreate(tFont *pFont) {
	s_pHudVPort = vPortCreate(0,
		TAG_VPORT_VIEW,	g_pWorldView,
		TAG_VPORT_BPP, WORLD_BPP,
		TAG_VPORT_OFFSET_TOP, 1,
	TAG_DONE);
	g_pHudBfr = simpleBufferCreate(0,
		TAG_SIMPLEBUFFER_VPORT, s_pHudVPort,
		TAG_SIMPLEBUFFER_COPLIST_OFFSET, WORLD_COP_VPHUD_POS,
		TAG_SIMPLEBUFFER_BITMAP_FLAGS, BMF_INTERLEAVED,
	TAG_DONE);
	s_pHudFont = pFont;
	s_pSpawnTextBfr = fontCreateTextBitMap(64, pFont->uwHeight);

	tCopCmd *pCopList = g_pWorldView->pCopList->pBackBfr->pList;
	copSetWait(
		&pCopList[WORLD_COP_VPHUD_DMAOFF_POS+0].sWait,
		0x04,
		pCopList[WORLD_COP_VPHUD_POS].sWait.bfWaitY
	);
	copSetMove(
		&pCopList[WORLD_COP_VPHUD_DMAOFF_POS+1].sMove,
		&g_pCustom->dmacon, BITCLR | DMAF_RASTER | DMAF_SPRITE
	);
	copSetMove(
		&pCopList[WORLD_COP_VPHUD_DMAON_POS].sMove,
		&g_pCustom->dmacon, BITSET|DMAF_RASTER
	);
	// Same for front bfr
	CopyMemQuick(
		&g_pWorldView->pCopList->pBackBfr->pList[WORLD_COP_VPHUD_DMAOFF_POS],
		&g_pWorldView->pCopList->pFrontBfr->pList[WORLD_COP_VPHUD_DMAOFF_POS],
		2*sizeof(tCopCmd)
	);
	CopyMemQuick(
		&g_pWorldView->pCopList->pBackBfr->pList[WORLD_COP_VPHUD_DMAON_POS],
		&g_pWorldView->pCopList->pFrontBfr->pList[WORLD_COP_VPHUD_DMAON_POS],
		1*sizeof(tCopCmd)
	);

	// Initial draw on buffer
	bitmapLoadFromFile(g_pHudBfr->pBack, "data/hud/blank.bm", 0, 0);
	s_pHudPanels[HUD_STATE_DRIVING] = bitmapCreateFromFile("data/hud/driving.bm");
	s_pHudPanels[HUD_STATE_SELECTING] = bitmapCreateFromFile("data/hud/selecting.bm");

	s_ubHudPrevState = 0xFF;
	s_ubFrame = 0;
	s_isFpsEnabled = 0;
	consoleCreate(s_pHudFont);
}

void hudChangeState(UBYTE ubState) {
	s_ubHudState = ubState;

	UWORD uwDmaCon;
	volatile UWORD * pCrossCtl;
	tCopCmd *pFrontList = g_pWorldView->pCopList->pFrontBfr->pList;
	tCopCmd *pBackList = g_pWorldView->pCopList->pBackBfr->pList;
	if(ubState == HUD_STATE_DRIVING) {
		pCrossCtl = &g_pCustom->spr[2].ctl;
		uwDmaCon = BITCLR | DMAF_RASTER | DMAF_SPRITE;
	}
	else {
		pCrossCtl = &g_pCustom->spr[0].ctl;
		uwDmaCon = BITCLR | DMAF_RASTER;
	}
	// Disable/enable sprite DMA during hud
	copSetMove(&pBackList[WORLD_COP_VPHUD_DMAOFF_POS+1].sMove, &g_pCustom->dmacon, uwDmaCon);
	copSetMove(&pFrontList[WORLD_COP_VPHUD_DMAOFF_POS+1].sMove, &g_pCustom->dmacon, uwDmaCon);
	// Disable/enable crosshair sprite during hud
	copSetMove(&pBackList[WORLD_COP_VPHUD_DMAOFF_POS+2].sMove, pCrossCtl, 0);
	copSetMove(&pFrontList[WORLD_COP_VPHUD_DMAOFF_POS+2].sMove, pCrossCtl, 0);
}

static void hudDrawBar(
	UWORD uwBarY, UWORD uwValue, UWORD uwMaxValue, UBYTE ubColor
) {
	// TODO draw rects on only one bitplane
	const UWORD uwBarHeight = 4;
	const UWORD uwBarX = 71;
	const UWORD uwMaxBarWidth = 32;

	// Calculate color length
	UWORD uwCurrBarWidth = (uwMaxBarWidth*uwValue)/ uwMaxValue;

	// Black part of bar
	if(uwCurrBarWidth != uwMaxBarWidth) {
		blitRect(
			g_pHudBfr->pBack,	(WORD)(uwBarX + uwCurrBarWidth), (WORD)uwBarY,
			(WORD)(uwMaxBarWidth - uwCurrBarWidth), uwBarHeight, 0
		);
	}

	// Colored part of bar
	if(uwCurrBarWidth) {
		blitRect(
			g_pHudBfr->pBack, (WORD)uwBarX, (WORD)uwBarY,
			(WORD)uwCurrBarWidth, (WORD)uwBarHeight, ubColor
		);
	}
}

static void hudDrawTeamScore(UBYTE ubTeam) {
	const UWORD uwTicketX = 2+72+2;
	const UWORD uwTicketY[2] = {2+35+3, 2+35+3+5+4};
	const UBYTE pTeamColors[2] = {12, 10};
	char szSpawnBfr[6];
	blitRect(g_pHudBfr->pBack, (WORD)uwTicketX, (WORD)uwTicketY[ubTeam], 26, 5, 0);
	sprintf(szSpawnBfr, "%5hu", g_pTeams[ubTeam].uwTicketsLeft);
	fontFillTextBitMap(s_pHudFont, s_pSpawnTextBfr, szSpawnBfr);
	fontDrawTextBitMap(
		g_pHudBfr->pBack, s_pSpawnTextBfr, uwTicketX, uwTicketY[ubTeam],
		pTeamColors[ubTeam], FONT_COOKIE | FONT_LAZY
	);
	s_uwPrevTickets[ubTeam] = g_pTeams[ubTeam].uwTicketsLeft;
}

static void hudForceRedraw(void) {
		blitCopy(
			s_pHudPanels[s_ubHudState], 0, 0, g_pHudBfr->pBack, 2, 2,
			104, (WORD)s_pHudPanels[0]->Rows, MINTERM_COOKIE, 0xFF
		);

	hudDrawTeamScore(TEAM_BLUE);
	hudDrawTeamScore(TEAM_RED);
}

void hudToggleFps(void) {
	if(s_isFpsEnabled) {
		// Redraw HUD to hide FPS counter
		hudForceRedraw();
	}
	s_isFpsEnabled = !s_isFpsEnabled;
}

static void hudDrawFps(void) {
	const UWORD uwFpsX = 3;
	const UWORD uwFpsY = 63-3-5;
	blitRect(g_pHudBfr->pBack, (WORD)uwFpsX, (WORD)uwFpsY-1, 26, 5+2, 0);
	char szFpsBfr[9];
	if(g_ubFps > 99) {
		strcpy(szFpsBfr, "FPS: 99+");
	}
	else {
		sprintf(szFpsBfr, "FPS: %3hu", g_ubFps);
	}
	fontFillTextBitMap(s_pHudFont, s_pSpawnTextBfr, szFpsBfr);
	blitRect(g_pHudBfr->pBack, uwFpsX-1, uwFpsY-1, 40+2, 5+2, 0);
	fontDrawTextBitMap(
		g_pHudBfr->pBack, s_pSpawnTextBfr, uwFpsX, uwFpsY,
		12, FONT_LAZY | FONT_COOKIE
	);
}

void hudUpdate(void) {
	if(s_ubHudState != s_ubHudPrevState) {
		hudForceRedraw();
	}
	if(s_ubHudState == HUD_STATE_DRIVING) {
		tVehicle *pVehicle = &g_pLocalPlayer->sVehicle;
		tVehicleType *pType = &g_pVehicleTypes[g_pLocalPlayer->ubCurrentVehicleType];

		// Draw bars
		const UWORD uwBarLifeY = 5;
		const UWORD uwBarAmmoY = 13;
		const UWORD uwBarFuelY = 21;
		hudDrawBar(uwBarLifeY, pVehicle->ubLife, pType->ubMaxLife, 4);
		if(s_ubFrame == 0 || s_ubFrame == 25) {
			hudDrawBar(uwBarAmmoY, pVehicle->ubBaseAmmo, pType->ubMaxBaseAmmo, 8);
		}
		else if(s_ubFrame == 1 || s_ubFrame == 26) {
			hudDrawBar(uwBarFuelY, pVehicle->ubFuel, pType->ubMaxFuel, 11);
		}
	}

	// Update ticket count
		if(s_ubFrame == 30 && s_uwPrevTickets[TEAM_BLUE] != g_pTeams[TEAM_BLUE].uwTicketsLeft) {
			hudDrawTeamScore(TEAM_BLUE);
		}
		else if(s_ubFrame == 40 && s_uwPrevTickets[TEAM_RED] != g_pTeams[TEAM_RED].uwTicketsLeft) {
			hudDrawTeamScore(TEAM_RED);
		}
	else if(s_ubFrame == 2 || s_ubFrame == 27) {
		consoleUpdate();
	}
	else if(s_ubFrame == 3 || s_ubFrame == 28) {
		if(s_isFpsEnabled) {
			hudDrawFps();
		}
	}

	s_ubHudPrevState = s_ubHudState;
	++s_ubFrame;
	if(s_ubFrame >= 50) {
		s_ubFrame = 0;
	}
}

void hudDestroy(void) {
	consoleDestroy();
	fontDestroyTextBitMap(s_pSpawnTextBfr);

	bitmapDestroy(s_pHudPanels[HUD_STATE_DRIVING]);
	bitmapDestroy(s_pHudPanels[HUD_STATE_SELECTING]);
	// VPort & simpleBuffer are destroyed by g_pGameView
}
