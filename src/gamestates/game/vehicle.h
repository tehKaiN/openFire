#ifndef GUARD_OF_GAMESTATES_GAME_VEHICLE_H
#define GUARD_OF_GAMESTATES_GAME_VEHICLE_H

#include <ace/utils/bitmap.h>
#include "vehicletypes.h"
#include "gamestates/game/worldmap.h"
#include "gamestates/game/gamemath.h"
#include "gamestates/game/projectile.h"
#include "gamestates/game/bob_new.h"
#include "gamestates/game/steer_request.h"

/// Vehicle-specific constants
#define VEHICLE_TANK_COOLDOWN PROJECTILE_FRAME_LIFE

typedef struct _tVehicle {
	tVehicleType *pType; ///< Ptr to vehicle type definition
	tBobNew sBob;        ///< Main body bob
	tBobNew sAuxBob;     ///< Tank - turret, chopper - takeoff anim
	fix16_t fX;          ///< Vehicle X-position relative to center of gfx.
	fix16_t fY;          ///< Ditto, vehicle Y.
	UWORD uwX;           ///< Same as fX, but converted to UWORD. Read-only.
	UWORD uwY;           ///< Ditto.
	UBYTE ubBodyAngle;   ///< Measured clockwise, +90deg is to bottom.
	UBYTE ubAuxAngle;    ///< NOT relative to body angle, measured as above.
	UBYTE ubBaseAmmo;
	UBYTE ubSuperAmmo;
	BYTE  bRotDiv;
	UBYTE ubFuel;
	UBYTE ubLife;
	UBYTE ubCooldown; ///< Cooldown timer after fire
	fix16_t fSpeed;
	fix16_t fSpeed2;
} tVehicle;

void vehicleInit(tVehicle *pVehicle, UBYTE ubVehicleType, UBYTE ubSpawnIdx);

void vehicleSteerTank(tVehicle *pVehicle, const tSteerRequest *pSteerRequest);
void vehicleSteerJeep(tVehicle *pVehicle, const tSteerRequest *pSteerRequest);
void vehicleSteerChopper(tVehicle *pVehicle, const tSteerRequest *pSteerRequest);

#endif
