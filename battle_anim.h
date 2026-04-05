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

#ifndef BATTLE_ANIM_H
#define BATTLE_ANIM_H

#include "common.h"

PAL_C_LINKAGE_BEGIN

// Show player physical attack animation
VOID PAL_BattleShowPlayerAttackAnim(WORD wPlayerIndex, BOOL fCritical, BOOL fSecondAttack);

// Show player throwing item animation
VOID PAL_BattleShowPlayerThrowItemAnim(WORD wPlayerIndex, WORD wObject, SHORT sTarget);

// Show player using item animation
VOID PAL_BattleShowPlayerUseItemAnim(WORD wPlayerIndex, WORD wObjectID, SHORT sTarget);

// Show player's pre-magic preparation animation
VOID PAL_BattleShowPlayerPreMagicAnim(WORD wPlayerIndex, BOOL fSummon);

// Show player's defensive magic animation (healing, buffs)
VOID PAL_BattleShowPlayerDefMagicAnim(WORD wPlayerIndex, WORD wObjectID, SHORT sTarget);

// Show player's offensive magic animation (damage spells)
VOID PAL_BattleShowPlayerOffMagicAnim(WORD wPlayerIndex, WORD wObjectID, SHORT sTarget, BOOL fSummon);

// Show player's summon magic animation (召唤神兽)
VOID PAL_BattleShowPlayerSummonMagicAnim(WORD wObjectID);

// Show post-magic effect (enemy damage animation)
VOID PAL_BattleShowPostMagicAnim(VOID);

// Show enemy magic animation
VOID PAL_BattleShowEnemyMagicAnim(WORD wEnemyIndex, WORD wObjectID, SHORT sTarget);

// Play enemy escape animation
VOID PAL_BattleEnemyEscape(VOID);

// Play player escape animation
VOID PAL_BattlePlayerEscape(VOID);

PAL_C_LINKAGE_END

#endif
