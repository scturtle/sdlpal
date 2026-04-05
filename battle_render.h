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

#ifndef BATTLE_RENDER_H
#define BATTLE_RENDER_H

#include "common.h"
#include "global.h"

PAL_C_LINKAGE_BEGIN

// ============ Loading ============

VOID PAL_LoadBattleRenderResources(VOID);
VOID PAL_FreeBattleRenderResources(VOID);

// Load/free battle sprites for players and enemies
VOID PAL_LoadBattleSprites(VOID);
VOID PAL_FreeBattleSprites(VOID);

// ============ Sprite Management ============

// Update fighters' gesture frames and positions
VOID PAL_BattleUpdateFighters(VOID);

// Update all enemy sprite frames (idle animation)
VOID PAL_BattleUpdateAllEnemyFrame(VOID);

// Unlock sprite add lock
VOID PAL_BattleSpriteAddUnlock(VOID);

// Add a sprite object to draw sequence
VOID PAL_BattleAddSpriteObject(WORD wType, WORD wObjectIndex, PAL_POS pos, SHORT sLayerOffset, BOOL fHaveColorShift);

// ============ Scene Composition ============

// Compose full battle scene (background + sprites)
VOID PAL_BattleMakeScene(VOID);

// ============ Scene Effects ============

// Fade scene from backup buffer to current scene
VOID PAL_BattleFadeScene(VOID);

VOID PAL_DrawLevelUpMessage(int w, PLAYERROLES *ORIG_PLAYER);

PAL_C_LINKAGE_END

#endif
