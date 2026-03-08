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

#ifndef UIBATTLE_H
#define UIBATTLE_H

#include "common.h"
#include "ui.h"

typedef enum tagBATTLEUISTATE {
  kBattleUIWait,                  // UI等待状态（无交互）
  kBattleUISelectMove,            // 待选择行动
  kBattleUISelectTargetEnemy,     // 待选择敌方目标
  kBattleUISelectTargetPlayer,    // 待选择友方目标
} BATTLEUISTATE;

typedef enum tagBATTLEMENUSTATE {
  kBattleMenuMain,            // 战斗主菜单
  kBattleMenuMagicSelect,     // 仙术选择子菜单
  kBattleMenuUseItemSelect,   // 物品使用子菜单
  kBattleMenuThrowItemSelect, // 物品投掷子菜单
  kBattleMenuMisc,            // 杂项菜单 (合击/防御/逃跑/状态)
  kBattleMenuMiscItemSubMenu, // 杂项中的物品子菜单
} BATTLEMENUSTATE;

typedef enum tagBATTLEUIACTION {
  kBattleUIActionAttack,    // 界面动作：普攻
  kBattleUIActionMagic,     // 界面动作：仙术
  kBattleUIActionCoopMagic, // 界面动作：合击技
  kBattleUIActionMisc,      // 界面动作：其他 (物品/防御/逃跑)
} BATTLEUIACTION;

typedef struct tagSHOWNUM {
  WORD wNum;       // 显示的数值 (伤害或回复量)
  PAL_POS pos;     // 数值显示的屏幕坐标
  BYTE pastFrames; // 已显示的帧数 (最多 10 帧)
  NUMCOLOR color;  // 数值的颜色 (如红色代表伤害，绿色代表回复)
} SHOWNUM;

#define BATTLEUI_MAX_SHOWNUM 16

typedef struct tagBATTLEUI {
  BATTLEUISTATE state;       // 当前UI状态（选择动作/选择目标等）
  BATTLEMENUSTATE MenuState; // 当前菜单层级状态

  WORD wCurPlayerIndex; // 当前正在操作的玩家角色索引
  WORD wSelectedAction; // 当前菜单中选中的选项索引
  INT iSelectedIndex;   // 当前光标选中的目标索引（敌人或玩家）
  INT iPrevEnemyTarget; // 上一次选中的敌人目标（用于光标记忆）

  WORD wActionType; // 最终决定执行的动作类型
  WORD wObjectID;   // 要使用的物品或仙术的对象ID

  BOOL fAutoAttack; // 自动攻击模式

  SHOWNUM rgShowNum[BATTLEUI_MAX_SHOWNUM]; // 飘出的数值对象数组（伤害/治疗数字）
} BATTLEUI;

PAL_C_LINKAGE_BEGIN

VOID PAL_PlayerInfoBox(PAL_POS pos, int iPartyIndex, BOOL fUpdate);

VOID PAL_BattleUIPlayerReady(WORD wPlayerIndex);

VOID PAL_BattleUIUpdate(VOID);

VOID PAL_BattleUIShowNum(WORD wNum, PAL_POS pos, NUMCOLOR color);

PAL_C_LINKAGE_END

#endif
