/* -*- mode: c; tab-width: 4; c-basic-offset: 4; c-file-style: "linux" -*- */
//
// Copyright (c) 2009-2011, Wei Mingzhi <whistler_wmz@users.sf.net>.
// Copyright (c) 2011-2024, SDLPAL development team.
// All rights reserved.
//
// This file is part of SDLPAL.
//
// SDLPAL is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License, version 3
// as published by the Free Software Foundation.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef _SCENE_H
#define _SCENE_H

#include "common.h"
#include "map.h"

typedef enum tagLOADRESFLAG {
  kLoadGlobalData = (1 << 0),   // load global data
  kLoadScene = (1 << 1),        // load a scene
  kLoadPlayerSprite = (1 << 2), // load player sprites
} LOADRESFLAG;

PAL_C_LINKAGE_BEGIN

VOID PAL_SetLoadFlags(BYTE bFlags);

VOID PAL_LoadResources(VOID);

VOID PAL_FreeResources(VOID);

LPPALMAP PAL_GetCurrentMap(VOID);

LPSPRITE PAL_GetPlayerSprite(BYTE bPlayerIndex);

LPSPRITE PAL_GetBattleSprite(BYTE bPlayerIndex);

LPSPRITE PAL_GetEventObjectSprite(WORD wEventObjectID);

VOID PAL_ApplyWave(SDL_Surface *lpSurface);

VOID PAL_MakeScene(VOID);

BOOL PAL_CheckObstacleWithRange(PAL_POS pos, BOOL fCheckEventObjects, WORD wSelfObject, BOOL fCheckRange);

BOOL PAL_CheckObstacle(PAL_POS pos, BOOL fCheckEventObjects, WORD wSelfObject);

VOID PAL_UpdatePartyGestures(BOOL fWalking);

VOID PAL_UpdateParty(VOID);

VOID PAL_NPCWalkOneStep(WORD wEventObjectID, INT iSpeed);

BOOL PAL_NPCWalkTo(WORD wEventObjectID, INT x, INT y, INT h, INT iSpeed);

VOID PAL_PartyWalkTo(INT x, INT y, INT h, INT iSpeed);

VOID PAL_PartyRideEventObject(WORD wEventObjectID, INT x, INT y, INT h, INT iSpeed);

VOID PAL_MonsterChasePlayer(WORD wEventObjectID, WORD wSpeed, WORD wChaseRange, BOOL fFloating);

VOID PAL_MoveViewport(WORD op0, WORD op1, WORD op2);

PAL_C_LINKAGE_END

#endif
