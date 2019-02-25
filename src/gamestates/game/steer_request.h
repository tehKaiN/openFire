#ifndef GUARD_OF_GAMESTATES_GAME_STEER_REQUEST_H
#define GUARD_OF_GAMESTATES_GAME_STEER_REQUEST_H

#include <ace/types.h>

typedef struct _tSteerRequest {
	UBYTE ubForward;
	UBYTE ubBackward;
	UBYTE ubLeft;
	UBYTE ubRight;
	UBYTE ubAction1;
	UBYTE ubAction2;
	UBYTE ubAction3;
	UBYTE ubDestAngle;
} tSteerRequest;

void steerRequestInit(void);
void steerRequestSwap(void);
tSteerRequest *steerRequestGetCurr(void);
UBYTE steerRequestIsLast(void);
UBYTE steerRequestIsFirst(void);
void steerRequestCapture(void);
UBYTE steerRequestReadCount(void);
UBYTE steerRequestProcessedCount(void);
void steerRequestMoveToNext(void);

#endif // GUARD_OF_GAMESTATES_GAME_STEER_REQUEST_H
