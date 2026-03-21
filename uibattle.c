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

#include "main.h"

#define SPRITENUM_BATTLEICON_ATTACK 40
#define SPRITENUM_BATTLEICON_MAGIC 41
#define SPRITENUM_BATTLEICON_COOPMAGIC 42
#define SPRITENUM_BATTLEICON_MISCMENU 43

#define SPRITENUM_BATTLE_ARROW_CURRENTPLAYER 69
#define SPRITENUM_BATTLE_ARROW_CURRENTPLAYER_RED 68
#define SPRITENUM_BATTLE_ARROW_SELECTEDPLAYER 67
#define SPRITENUM_BATTLE_ARROW_SELECTEDPLAYER_RED 66

#define BATTLEUI_LABEL_ITEM 5
#define BATTLEUI_LABEL_DEFEND 58
#define BATTLEUI_LABEL_AUTO 56
#define BATTLEUI_LABEL_INVENTORY 57
#define BATTLEUI_LABEL_FLEE 59
#define BATTLEUI_LABEL_STATUS 60
#define BATTLEUI_LABEL_USEITEM 23
#define BATTLEUI_LABEL_THROWITEM 24

#define TIMEMETER_COLOR_DEFAULT 0x1B
#define TIMEMETER_COLOR_SLOW 0x5B
#define TIMEMETER_COLOR_HASTE 0x2A

#define BATTLEUI_MAX_SHOWNUM 16

extern WORD g_rgPlayerPos[3][3][2];

static int g_iCurMiscMenuItem = 0;
static int g_iCurSubMenuItem = 0;

VOID PAL_PlayerInfoBox(PAL_POS pos, int iPartyIndex, BOOL fUpdate) {
  const BYTE rgStatusPos[kStatusAll][2] = {
      {35, 19}, {44, 12}, {54, 1}, {55, 20}, {0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0},
  };
  const WORD rgwStatusWord[kStatusAll] = {
      0x1D, 0x1B, 0x1C, 0x1A, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  const BYTE rgbStatusColor[kStatusAll] = {
      0x5F, 0xBF, 0x0E, 0x3C, 0x00, 0x00, 0x00, 0x00, 0x00,
  };

  // Draw the box
  PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_PLAYERINFOBOX), gpScreen, pos);

  WORD wMaxLevel = 0;
  BYTE bPoisonColor = 0xFF;

  for (int i = 0; i < MAX_POISONS; i++) {
    WORD w = g.rgPoisonStatus[i][iPartyIndex].wPoisonID;
    if (w != 0 && OBJECT[w].poison.wPoisonLevel <= 3) {
      if (OBJECT[w].poison.wPoisonLevel >= wMaxLevel) {
        wMaxLevel = OBJECT[w].poison.wPoisonLevel;
        bPoisonColor = (BYTE)(OBJECT[w].poison.wColor);
      }
    }
  }

  // Always use the black/white color for dead players
  WORD wPlayerRole = PARTY_PLAYER(iPartyIndex);
  if (PLAYER.rgwHP[wPlayerRole] == 0)
    bPoisonColor = 0;

  // Draw the player face
  if (bPoisonColor == 0xFF) {
    PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_PLAYERFACE_FIRST + wPlayerRole), gpScreen,
                         PAL_XY_OFFSET(pos, -2, -4));
  } else {
    PAL_RLEBlitMonoColor(PAL_GetUISprite(SPRITENUM_PLAYERFACE_FIRST + wPlayerRole), gpScreen,
                         PAL_XY_OFFSET(pos, -2, -4), bPoisonColor, 0);
  }

  // Draw the HP and MP value
  PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_SLASH), gpScreen, PAL_XY_OFFSET(pos, 49, 6));
  PAL_DrawNumber(PLAYER.rgwMaxHP[wPlayerRole], 4, PAL_XY_OFFSET(pos, 47, 8), kNumColorYellow, kNumAlignRight);
  PAL_DrawNumber(PLAYER.rgwHP[wPlayerRole], 4, PAL_XY_OFFSET(pos, 26, 5), kNumColorYellow, kNumAlignRight);

  PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_SLASH), gpScreen, PAL_XY_OFFSET(pos, 49, 22));
  PAL_DrawNumber(PLAYER.rgwMaxMP[wPlayerRole], 4, PAL_XY_OFFSET(pos, 47, 24), kNumColorCyan, kNumAlignRight);
  PAL_DrawNumber(PLAYER.rgwMP[wPlayerRole], 4, PAL_XY_OFFSET(pos, 26, 21), kNumColorCyan, kNumAlignRight);

  // Draw Statuses
  for (int i = 0; i < kStatusAll; i++) {
    if (PLAYER.rgwHP[wPlayerRole] > 0 && gPlayerStatus[wPlayerRole][i] > 0 && rgwStatusWord[i] != 0) {
      PAL_DrawText(PAL_GetWord(rgwStatusWord[i]), PAL_XY_OFFSET(pos, rgStatusPos[i][0], rgStatusPos[i][1]),
                   rgbStatusColor[i], TRUE, FALSE);
    }
  }

  // Update the screen area if needed
  if (fUpdate) {
    SDL_Rect rect = {PAL_X(pos) - 2, PAL_Y(pos) - 4, 77, 39};
    VIDEO_UpdateScreen(&rect);
  }
}

// Check if the specified action is valid.
static BOOL PAL_BattleUIIsActionValid(BATTLEUIACTION ActionType) {
  WORD wPlayerRole = PARTY_PLAYER(g_Battle.UI.wCurPlayerIndex);
  switch (ActionType) {
  case kBattleUIActionAttack:
  case kBattleUIActionMisc:
    break;
  case kBattleUIActionMagic:
    if (gPlayerStatus[wPlayerRole][kStatusSilence] != 0)
      return FALSE;
    break;
  case kBattleUIActionCoopMagic:
    if (g.wMaxPartyMemberIndex == 0)
      return FALSE;
    int healthyNumber = 0;
    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++)
      if (PAL_IsPlayerHealthy(PARTY_PLAYER(i)))
        healthyNumber++;
    return PAL_IsPlayerHealthy(wPlayerRole) && healthyNumber > 1;
    break;
  }
  return TRUE;
}

static VOID PAL_BattleUIDrawMiscMenu(WORD wCurrentItem, BOOL fConfirmed) {
  MENUITEM rgMenuItem[5] = {{0, BATTLEUI_LABEL_AUTO, TRUE, PAL_XY(16, 32)},
                            {1, BATTLEUI_LABEL_INVENTORY, TRUE, PAL_XY(16, 50)},
                            {2, BATTLEUI_LABEL_DEFEND, TRUE, PAL_XY(16, 68)},
                            {3, BATTLEUI_LABEL_FLEE, TRUE, PAL_XY(16, 86)},
                            {4, BATTLEUI_LABEL_STATUS, TRUE, PAL_XY(16, 104)}};
  PAL_CreateBox(PAL_XY(2, 20), 4, PAL_MenuTextMaxWidth(rgMenuItem, 5) - 1, 0, FALSE);
  for (int i = 0; i < 5; i++) {
    BYTE bColor = i != wCurrentItem ? MENUITEM_COLOR : fConfirmed ? MENUITEM_COLOR_CONFIRMED : MENUITEM_COLOR_SELECTED;
    PAL_DrawText(PAL_GetWord(rgMenuItem[i].wNumWord), rgMenuItem[i].pos, bColor, TRUE, FALSE);
  }
}

static WORD PAL_BattleUIMiscMenuUpdate(VOID) {
  PAL_BattleUIDrawMiscMenu(g_iCurMiscMenuItem, FALSE);
  if (g_InputState.dwKeyPress & (kKeyUp | kKeyLeft)) {
    g_iCurMiscMenuItem--;
    if (g_iCurMiscMenuItem < 0)
      g_iCurMiscMenuItem = 4;
  } else if (g_InputState.dwKeyPress & (kKeyDown | kKeyRight)) {
    g_iCurMiscMenuItem++;
    if (g_iCurMiscMenuItem > 4)
      g_iCurMiscMenuItem = 0;
  } else if (g_InputState.dwKeyPress & kKeySearch) {
    return g_iCurMiscMenuItem + 1;
  } else if (g_InputState.dwKeyPress & kKeyMenu) {
    return 0;
  }
  return 0xFFFF;
}

static WORD PAL_BattleUIMiscItemSubMenuUpdate(VOID) {
  MENUITEM rgMenuItem[] = {
      {0, BATTLEUI_LABEL_USEITEM, TRUE, PAL_XY(44, 62)},
      {1, BATTLEUI_LABEL_THROWITEM, TRUE, PAL_XY(44, 80)},
  };
  PAL_BattleUIDrawMiscMenu(1, TRUE);
  PAL_CreateBox(PAL_XY(30, 50), 1, PAL_MenuTextMaxWidth(rgMenuItem, 2) - 1, 0, FALSE);
  for (int i = 0; i < 2; i++) {
    BYTE bColor = i != g_iCurSubMenuItem ? MENUITEM_COLOR : MENUITEM_COLOR_SELECTED;
    PAL_DrawText(PAL_GetWord(rgMenuItem[i].wNumWord), rgMenuItem[i].pos, bColor, TRUE, FALSE);
  }
  if (g_InputState.dwKeyPress & (kKeyUp | kKeyLeft))
    g_iCurSubMenuItem = 0;
  else if (g_InputState.dwKeyPress & (kKeyDown | kKeyRight))
    g_iCurSubMenuItem = 1;
  else if (g_InputState.dwKeyPress & kKeySearch)
    return g_iCurSubMenuItem + 1;
  else if (g_InputState.dwKeyPress & kKeyMenu)
    return 0;
  return 0xFFFF;
}

static VOID PAL_BattleUIHandleMiscItemSubMenu(VOID) {
  WORD w = PAL_BattleUIMiscItemSubMenuUpdate();
  if (w == 0xFFFF)
    return;
  g_Battle.UI.MenuState = kBattleMenuMain;
  if (w == 1) {
    g_Battle.UI.MenuState = kBattleMenuUseItemSelect;
    PAL_ItemSelectMenuInit(kItemFlagUsable);
  }
  if (w == 2) {
    g_Battle.UI.MenuState = kBattleMenuThrowItemSelect;
    PAL_ItemSelectMenuInit(kItemFlagThrowable);
  }
}

VOID PAL_BattleUIPlayerReady(WORD wPlayerIndex) {
  g_Battle.UI.wCurPlayerIndex = wPlayerIndex;
  g_Battle.UI.state = kBattleUISelectMove;
  g_Battle.UI.wSelectedAction = 0;
  g_Battle.UI.MenuState = kBattleMenuMain;
}

static VOID PAL_BattleUISetActionTarget(BOOL bUsableToEnemy, BOOL bApplyToAll) {
  if (bApplyToAll) {
    g_Battle.UI.iSelectedIndex = (WORD)-1;
    PAL_BattleCommitAction(FALSE);
  } else {
    g_Battle.UI.iSelectedIndex = 0;
    if (bUsableToEnemy) {
      if (g_Battle.UI.iPrevEnemyTarget != -1)
        g_Battle.UI.iSelectedIndex = g_Battle.UI.iPrevEnemyTarget;
      g_Battle.UI.state = kBattleUISelectTargetEnemy;
    } else {
      g_Battle.UI.state = kBattleUISelectTargetPlayer;
    }
  }
}

static VOID PAL_BattleUIMenuMagicSelect(VOID) {
  WORD w = PAL_MagicSelectionMenuUpdate();
  if (w != 0xFFFF) {
    g_Battle.UI.MenuState = kBattleMenuMain;
    if (w != 0) {
      g_Battle.UI.wActionType = kBattleActionMagic;
      g_Battle.UI.wObjectID = w;
      BOOL isEnemy = (OBJECT[w].magic.wFlags & kMagicFlagUsableToEnemy);
      BOOL isAll = (OBJECT[w].magic.wFlags & kMagicFlagApplyToAll);
      PAL_BattleUISetActionTarget(isEnemy, isAll);
    }
  }
}

static VOID PAL_BattleUIUseItemSelect(BOOL bThrow) {
  WORD wSelectedItem = PAL_ItemSelectMenuUpdate();
  if (wSelectedItem != 0xFFFF) {
    if (wSelectedItem != 0) {
      g_Battle.UI.wActionType = bThrow ? kBattleActionThrowItem : kBattleActionUseItem;
      g_Battle.UI.wObjectID = wSelectedItem;
      BOOL isAll = (OBJECT[wSelectedItem].item.wFlags & kItemFlagApplyToAll);
      PAL_BattleUISetActionTarget(bThrow, isAll);
    } else {
      g_Battle.UI.MenuState = kBattleMenuMain;
    }
  }
}

// Pick a magic with max power for the specified player for automatic usage.
static WORD PAL_BattleUIPickAutoMagic(WORD wPlayerRole, WORD wRandomRange) {
  WORD wMagic = 0;
#define BETTER_AUTO_MAGIC
#ifndef BETTER_AUTO_MAGIC
  int iMaxPower = 0;
#endif
  int maxDamage = 0;
  int seletedIndex = 0;
  for (int i = 0; i < MAX_PLAYER_MAGICS; i++) {
    if (gPlayerStatus[wPlayerRole][kStatusSilence] != 0) // 咒封
      continue;
    WORD w = PLAYER.rgwMagic[i][wPlayerRole];
    if (w == 0)
      continue;
    WORD wMagicNum = OBJECT[w].magic.wMagicNumber;
    // skip if the magic is an ultimate move or not enough MP
    if (MAGIC[wMagicNum].wCostMP == 1 || MAGIC[wMagicNum].wCostMP > PLAYER.rgwMP[wPlayerRole] ||
        MAGIC[wMagicNum].wBaseDamage <= 0) {
      continue;
    }
#ifndef BETTER_AUTO_MAGIC
    int iPower = MAGIC[wMagicNum].wBaseDamage + RandomLong(0, wRandomRange);
    if (iPower > iMaxPower) {
      iMaxPower = iPower;
      wMagic = w;
    }
#else
    int currDamage = 0;
    int currIndex = -1;
    for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
      BATTLEENEMY *enemy = &BATTLE_ENEMY[i];
      if (enemy->wObjectID == 0)
        continue;
      WORD str = PAL_GetPlayerMagicStrength(wPlayerRole);
      WORD def = enemy->e.wDefense + (enemy->e.wLevel + 6) * 4;
      SHORT damage = PAL_CalcMagicDamage(str, def, enemy->e.wElemResistance, enemy->e.wPoisonResistance, 1, w);
      damage = min(max(0, damage), enemy->e.wHealth);
      if (OBJECT[w].magic.wFlags & kMagicFlagApplyToAll) {
        currDamage += damage;
      } else {
        if (damage > currDamage) {
          currDamage = damage;
          currIndex = i;
        }
      }
    }
    if (currDamage > maxDamage) {
      maxDamage = currDamage;
      seletedIndex = currIndex;
      wMagic = w;
    }
#endif
  }
  // Update UI.
  if (wMagic == 0) {
    g_Battle.UI.wActionType = kBattleActionAttack;
    g_Battle.UI.iSelectedIndex = PAL_BattleSelectAutoTarget(0);
  } else {
    g_Battle.UI.wActionType = kBattleActionMagic;
    g_Battle.UI.wObjectID = wMagic;
#ifndef BETTER_AUTO_MAGIC
    g_Battle.UI.iSelectedIndex = PAL_BattleSelectAutoTarget(wMagic);
#else
    g_Battle.UI.iSelectedIndex = seletedIndex;
#endif
  }
  return wMagic;
}

static VOID PAL_BattleUIDrawNumbers(VOID) {
  for (int i = 0; i < BATTLEUI_MAX_SHOWNUM; i++) {
    if (g_Battle.UI.rgShowNum[i].wNum > 0) {
      uint64_t pastFrames = ++g_Battle.UI.rgShowNum[i].pastFrames;
      if (pastFrames > 10) {
        g_Battle.UI.rgShowNum[i].wNum = 0;
      } else {
        PAL_POS pos = PAL_XY_OFFSET(g_Battle.UI.rgShowNum[i].pos, 0, -pastFrames);
        PAL_DrawNumber(g_Battle.UI.rgShowNum[i].wNum, 5, pos, g_Battle.UI.rgShowNum[i].color, kNumAlignRight);
      }
    }
  }
}

static VOID PAL_BattleEndFrame(VOID) {
  PAL_BattleUIDrawNumbers();
  PAL_ClearKeyState();
}

// 检查玩家是否因状态异常(昏睡、疯魔等)或自动攻击而跳过指令输入
static BOOL PAL_CheckPlayerStatusAndAuto(WORD wPlayerRole) {
  // 僵尸模式
  if (PLAYER.rgwHP[wPlayerRole] == 0 && gPlayerStatus[wPlayerRole][kStatusPuppet]) {
    g_Battle.UI.wActionType = kBattleActionAttack;
    g_Battle.UI.iSelectedIndex = PAL_BattleSelectAutoTarget(0);
    PAL_BattleCommitAction(FALSE);
    return TRUE;
  }

  // 无法行动 (死亡、昏睡、定身)
  if (PLAYER.rgwHP[wPlayerRole] == 0 || gPlayerStatus[wPlayerRole][kStatusSleep] != 0 ||
      gPlayerStatus[wPlayerRole][kStatusParalyzed] != 0) {
    g_Battle.UI.wActionType = kBattleActionPass;
    PAL_BattleCommitAction(FALSE);
    return TRUE;
  }

  // 疯魔
  if (gPlayerStatus[wPlayerRole][kStatusConfused] != 0) {
    g_Battle.UI.wActionType = kBattleActionAttackMate;
    PAL_BattleCommitAction(FALSE);
    return TRUE;
  }

  // 自动普通攻击模式
  if (g_Battle.UI.fAutoAttack) {
    g_Battle.UI.wActionType = kBattleActionAttack;
    g_Battle.UI.iSelectedIndex = PAL_BattleSelectAutoTarget(0);
    PAL_BattleCommitAction(FALSE);
    return TRUE;
  }
  return FALSE;
}

// 撤销/返回时物品归还逻辑
static VOID PAL_HandleMenuCancel(VOID) {
  while (g_Battle.UI.wCurPlayerIndex > 0) {
    // Revert to the previous player
    --g_Battle.UI.wCurPlayerIndex;
    BATTLE_PLAYER[g_Battle.UI.wCurPlayerIndex].state = kFighterWait;

    // 如果上一个人使用了物品，需要把扣除的数量加回去
    WORD actionType = BATTLE_PLAYER[g_Battle.UI.wCurPlayerIndex].action.ActionType;
    WORD actionID = BATTLE_PLAYER[g_Battle.UI.wCurPlayerIndex].action.wActionID;

    if (actionType == kBattleActionThrowItem ||
        (actionType == kBattleActionUseItem && OBJECT[actionID].item.wFlags & kItemFlagConsuming)) {
      for (int i = 0; i < MAX_INVENTORY; i++) {
        if (g.rgInventory[i].wItem == actionID) {
          g.rgInventory[i].nAmountInUse--;
          break;
        }
      }
    }

    // 如果上一个玩家也是无法控制的状态，继续回退
    WORD idx = PARTY_PLAYER(g_Battle.UI.wCurPlayerIndex);
    BOOL uncontrollable = (PLAYER.rgwHP[idx] == 0 || gPlayerStatus[idx][kStatusConfused] > 0 ||
                           gPlayerStatus[idx][kStatusSleep] > 0 || gPlayerStatus[idx][kStatusParalyzed] > 0);
    if (!uncontrollable)
      break;
  }
}

static VOID PAL_BattleUIDrawMenuMain(int cur) {
  struct {
    int iSpriteNum;
    PAL_POS pos;
  } rgItems[] = {{SPRITENUM_BATTLEICON_ATTACK, PAL_XY(27, 140)},
                 {SPRITENUM_BATTLEICON_MAGIC, PAL_XY(0, 155)},
                 {SPRITENUM_BATTLEICON_COOPMAGIC, PAL_XY(54, 155)},
                 {SPRITENUM_BATTLEICON_MISCMENU, PAL_XY(27, 170)}};

  for (int i = 0; i < 4; i++) {
    if (cur == i) {
      PAL_RLEBlitToSurface(PAL_GetUISprite(rgItems[i].iSpriteNum), gpScreen, rgItems[i].pos);
    } else if (PAL_BattleUIIsActionValid(i)) {
      PAL_RLEBlitMonoColor(PAL_GetUISprite(rgItems[i].iSpriteNum), gpScreen, rgItems[i].pos, 0, -4);
    } else {
      PAL_RLEBlitMonoColor(PAL_GetUISprite(rgItems[i].iSpriteNum), gpScreen, rgItems[i].pos, 0x10, -4);
    }
  }
}

static VOID PAL_BattleUISelectTargetEnemy(int iFrame) {
  int lastAliveIndex = -1;
  int totalAliveCount = 0;

  for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
    if (BATTLE_ENEMY[i].wObjectID != 0) {
      lastAliveIndex = i;
      totalAliveCount++;
    }
  }

  if (lastAliveIndex == -1) {
    g_Battle.UI.state = kBattleUISelectMove;
    return;
  }

  if (g_Battle.UI.wActionType == kBattleActionCoopMagic) {
    if (!PAL_BattleUIIsActionValid(kBattleUIActionCoopMagic)) {
      g_Battle.UI.state = kBattleUISelectMove;
      return;
    }
  }

  // Don't bother selecting when only 1 enemy left
  if (totalAliveCount == 1) {
    PAL_BattleUIDrawMenuMain(g_Battle.UI.wSelectedAction);
    g_Battle.UI.iSelectedIndex = lastAliveIndex;
    PAL_BattleCommitAction(FALSE);
    return;
  }
  PAL_BattleUIDrawMenuMain(-1);

  // 跳过选择已死敌人
  if (g_Battle.UI.iSelectedIndex > lastAliveIndex)
    g_Battle.UI.iSelectedIndex = lastAliveIndex;
  else if (g_Battle.UI.iSelectedIndex < 0)
    g_Battle.UI.iSelectedIndex = 0;

  while (BATTLE_ENEMY[g_Battle.UI.iSelectedIndex].wObjectID == 0) {
    g_Battle.UI.iSelectedIndex++;
    if (g_Battle.UI.iSelectedIndex > lastAliveIndex)
      g_Battle.UI.iSelectedIndex = 0;
  }

  // Highlight the selected enemy
  if (iFrame & 1) {
    BATTLEENEMY *enemy = &BATTLE_ENEMY[g_Battle.UI.iSelectedIndex];
    LPCBITMAPRLE b = PAL_SpriteGetFrame(enemy->lpSprite, enemy->wCurrentFrame);
    PAL_POS pos = PAL_XY_OFFSET(enemy->pos, -PAL_RLEGetWidth(b) / 2, -PAL_RLEGetHeight(b));
    PAL_RLEBlitWithColorShift(b, gpScreen, pos, 7);
  }

  if (g_InputState.dwKeyPress & kKeyMenu) { // ESC
    g_Battle.UI.state = kBattleUISelectMove;
  } else if (g_InputState.dwKeyPress & kKeySearch) { // SPACE or ENTER
    PAL_BattleCommitAction(FALSE);
  } else if (g_InputState.dwKeyPress & (kKeyLeft | kKeyDown)) {
    g_Battle.UI.iSelectedIndex--;
    if (g_Battle.UI.iSelectedIndex < 0)
      g_Battle.UI.iSelectedIndex = MAX_ENEMIES_IN_TEAM - 1;
    while (g_Battle.UI.iSelectedIndex != 0 && BATTLE_ENEMY[g_Battle.UI.iSelectedIndex].wObjectID == 0) {
      g_Battle.UI.iSelectedIndex--;
      if (g_Battle.UI.iSelectedIndex < 0)
        g_Battle.UI.iSelectedIndex = MAX_ENEMIES_IN_TEAM - 1;
    }
  } else if (g_InputState.dwKeyPress & (kKeyRight | kKeyUp)) {
    g_Battle.UI.iSelectedIndex++;
    if (g_Battle.UI.iSelectedIndex >= MAX_ENEMIES_IN_TEAM)
      g_Battle.UI.iSelectedIndex = 0;
    while (g_Battle.UI.iSelectedIndex < MAX_ENEMIES_IN_TEAM &&
           BATTLE_ENEMY[g_Battle.UI.iSelectedIndex].wObjectID == 0) {
      g_Battle.UI.iSelectedIndex++;
      if (g_Battle.UI.iSelectedIndex >= MAX_ENEMIES_IN_TEAM)
        g_Battle.UI.iSelectedIndex = 0;
    }
  }
}

static VOID PAL_BattleUISelectTargetPlayer(int iFrame) {
  // Don't bother selecting when only 1 player is in the party
  if (g.wMaxPartyMemberIndex == 0) {
    PAL_BattleUIDrawMenuMain(g_Battle.UI.wSelectedAction);
    g_Battle.UI.iSelectedIndex = 0;
    PAL_BattleCommitAction(FALSE);
    return;
  }
  PAL_BattleUIDrawMenuMain(-1);

  // Draw arrows on the selected player
  int j = iFrame & 1 ? SPRITENUM_BATTLE_ARROW_SELECTEDPLAYER_RED : SPRITENUM_BATTLE_ARROW_SELECTEDPLAYER;
  int x = g_rgPlayerPos[g.wMaxPartyMemberIndex][g_Battle.UI.iSelectedIndex][0] - 8;
  int y = g_rgPlayerPos[g.wMaxPartyMemberIndex][g_Battle.UI.iSelectedIndex][1] - 67;
  PAL_RLEBlitToSurface(PAL_GetUISprite(j), gpScreen, PAL_XY(x, y));

  if (g_InputState.dwKeyPress & kKeyMenu) { // ESC
    g_Battle.UI.state = kBattleUISelectMove;
  } else if (g_InputState.dwKeyPress & kKeySearch) { // SPACE or RETURN
    PAL_BattleCommitAction(FALSE);
  } else if (g_InputState.dwKeyPress & (kKeyLeft | kKeyDown)) {
    if (g_Battle.UI.iSelectedIndex != 0)
      g_Battle.UI.iSelectedIndex--;
    else
      g_Battle.UI.iSelectedIndex = g.wMaxPartyMemberIndex;
  } else if (g_InputState.dwKeyPress & (kKeyRight | kKeyUp)) {
    if (g_Battle.UI.iSelectedIndex < g.wMaxPartyMemberIndex)
      g_Battle.UI.iSelectedIndex++;
    else
      g_Battle.UI.iSelectedIndex = 0;
  }
}

static VOID PAL_BattleUIOnSelectAction(int wPlayerRole, WORD selectedAction) {
  if (selectedAction == kBattleUIActionAttack) {
    g_Battle.UI.wActionType = kBattleActionAttack;
    BOOL isAll = PAL_PlayerCanAttackAll(PARTY_PLAYER(g_Battle.UI.wCurPlayerIndex));
    PAL_BattleUISetActionTarget(TRUE, isAll);

  } else if (selectedAction == kBattleUIActionMagic) {
    g_Battle.UI.MenuState = kBattleMenuMagicSelect;
    PAL_MagicSelectionMenuInit(wPlayerRole, TRUE, 0);

  } else if (selectedAction == kBattleUIActionCoopMagic) {
    WORD w = PAL_GetPlayerCooperativeMagic(PARTY_PLAYER(g_Battle.UI.wCurPlayerIndex));
    g_Battle.UI.wActionType = kBattleActionCoopMagic;
    g_Battle.UI.wObjectID = w;
    BOOL isEnemy = (OBJECT[w].magic.wFlags & kMagicFlagUsableToEnemy);
    BOOL isAll = (OBJECT[w].magic.wFlags & kMagicFlagApplyToAll);
    PAL_BattleUISetActionTarget(isEnemy, isAll);

  } else if (selectedAction == kBattleUIActionMisc) {
    g_Battle.UI.MenuState = kBattleMenuMisc;
  }
}

static VOID PAL_BattleUIHandleMiscMenu(VOID) {
  WORD w = PAL_BattleUIMiscMenuUpdate();

  // display hp of enemies // by scturtle
  if (w == 0xFFFF) {
    for (int i = 0; i < MAX_ENEMIES_IN_TEAM; i++) {
      BATTLEENEMY *e = &BATTLE_ENEMY[i];
      if (!e->wObjectID)
        continue;
      LPCBITMAPRLE b = PAL_SpriteGetFrame(e->lpSprite, e->wCurrentFrame);
      PAL_POS pos = PAL_XY_OFFSET(e->pos, 0, -PAL_RLEGetHeight(b) / 2);
      PAL_DrawNumber(e->e.wHealth, 5, pos, kNumColorYellow, kNumAlignRight);
    }
    return;
  }

  g_Battle.UI.MenuState = kBattleMenuMain;
  switch (w) {
  case 1: // auto
    g_Battle.UI.fAutoAttack = TRUE;
    break;
  case 2: // item
    g_Battle.UI.MenuState = kBattleMenuMiscItemSubMenu;
    break;
  case 3: // defend
    g_Battle.UI.wActionType = kBattleActionDefend;
    PAL_BattleCommitAction(FALSE);
    break;
  case 4: // flee
    g_Battle.UI.wActionType = kBattleActionFlee;
    PAL_BattleCommitAction(FALSE);
    break;
  case 5: // status
    PAL_PlayerStatus();
    break;
  }
}

static VOID PAL_BattleUIHandleSelectMove(WORD wPlayerRole) {
  if (g_Battle.UI.MenuState == kBattleMenuMain) {
    if (g_InputState.dir == kDirNorth) {
      g_Battle.UI.wSelectedAction = kBattleUIActionAttack;
    } else if (g_InputState.dir == kDirSouth) {
      g_Battle.UI.wSelectedAction = kBattleUIActionMisc;
    } else if (g_InputState.dir == kDirWest) {
      g_Battle.UI.wSelectedAction = kBattleUIActionMagic;
    } else if (g_InputState.dir == kDirEast) {
      g_Battle.UI.wSelectedAction = kBattleUIActionCoopMagic;
    }
  }

  if (!PAL_BattleUIIsActionValid(g_Battle.UI.wSelectedAction))
    g_Battle.UI.wSelectedAction = kBattleUIActionAttack;

  PAL_BattleUIDrawMenuMain(g_Battle.UI.wSelectedAction);

  switch (g_Battle.UI.MenuState) {
  case kBattleMenuMain:
    if (g_InputState.dwKeyPress & kKeySearch) { // SPACE or RETURN
      PAL_BattleUIOnSelectAction(wPlayerRole, g_Battle.UI.wSelectedAction);
    } else if (g_InputState.dwKeyPress & kKeyDefend) { // D
      g_Battle.UI.wActionType = kBattleActionDefend;
      PAL_BattleCommitAction(FALSE);
    } else if (g_InputState.dwKeyPress & kKeyForce) { // F
      PAL_BattleUIPickAutoMagic(PARTY_PLAYER(g_Battle.UI.wCurPlayerIndex), 60);
      PAL_BattleCommitAction(FALSE);
    } else if (g_InputState.dwKeyPress & kKeyFlee) { // Q
      g_Battle.UI.wActionType = kBattleActionFlee;
      PAL_BattleCommitAction(FALSE);
    } else if (g_InputState.dwKeyPress & kKeyUseItem) { // E
      g_Battle.UI.MenuState = kBattleMenuUseItemSelect;
      PAL_ItemSelectMenuInit(kItemFlagUsable);
    } else if (g_InputState.dwKeyPress & kKeyThrowItem) { // W
      g_Battle.UI.MenuState = kBattleMenuThrowItemSelect;
      PAL_ItemSelectMenuInit(kItemFlagThrowable);
    } else if (g_InputState.dwKeyPress & kKeyRepeat) { // R
      PAL_BattleCommitAction(TRUE);
    } else if (g_InputState.dwKeyPress & kKeyMenu) { // ESC
      BATTLE_PLAYER[g_Battle.UI.wCurPlayerIndex].state = kFighterWait;
      g_Battle.UI.state = kBattleUIWait;
      PAL_HandleMenuCancel();
    }
    break;

  case kBattleMenuMagicSelect:
    PAL_BattleUIMenuMagicSelect();
    break;

  case kBattleMenuUseItemSelect:
    PAL_BattleUIUseItemSelect(FALSE);
    break;

  case kBattleMenuThrowItemSelect:
    PAL_BattleUIUseItemSelect(TRUE);
    break;

  case kBattleMenuMisc:
    PAL_BattleUIHandleMiscMenu();
    break;

  case kBattleMenuMiscItemSubMenu:
    PAL_BattleUIHandleMiscItemSubMenu();
    break;
  }
}

VOID PAL_BattleUIUpdate(VOID) {
  static int s_iFrame = 0;
  s_iFrame++;

  // 自动战斗 (非围攻)
  if (gAutoBattle) {
    PAL_BattlePlayerCheckReady();
    if (g_Battle.UI.state != kBattleUIWait) {
      PAL_BattleUIPickAutoMagic(PARTY_PLAYER(g_Battle.UI.wCurPlayerIndex), 9999);
      PAL_BattleCommitAction(FALSE);
    }
    PAL_BattleEndFrame();
    return;
  }

  if (g_Battle.UI.fAutoAttack) {
    // 取消围攻
    if (g_InputState.dwKeyPress & kKeyMenu) {
      g_Battle.UI.fAutoAttack = FALSE;
    } else {
      // 显示围攻
      LPCWSTR itemText = PAL_GetWord(BATTLEUI_LABEL_AUTO);
      PAL_DrawText(itemText, PAL_XY(320 - 8 - PAL_TextWidth(itemText), 10), MENUITEM_COLOR_CONFIRMED, TRUE, FALSE);
    }
  }

  // 动画中也可以切换围攻
  if (g_InputState.dwKeyPress & kKeyAuto) {
    g_Battle.UI.fAutoAttack = !g_Battle.UI.fAutoAttack;
    g_Battle.UI.MenuState = kBattleMenuMain;
  }

  // 动画播放中跳过 UI 逻辑
  if (g_Battle.Phase == kBattlePhasePerformAction) {
    PAL_BattleEndFrame();
    return;
  }

  // 围攻中不显示玩家信息框
  if (!g_Battle.UI.fAutoAttack) {
    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++)
      PAL_PlayerInfoBox(PAL_XY(91 + 77 * i, 165), i, FALSE);
  }

  if (g_InputState.dwKeyPress & kKeyStatus) {
    PAL_PlayerStatus();
    PAL_BattleEndFrame();
    return;
  }

  WORD wPlayerRole = PARTY_PLAYER(g_Battle.UI.wCurPlayerIndex);

  // 选择行动或目标中
  if (g_Battle.UI.state != kBattleUIWait) {
    // 玩家行动前置检查 (如果玩家不能控制，直接提交动作并返回)
    if (PAL_CheckPlayerStatusAndAuto(wPlayerRole)) {
      PAL_BattleEndFrame();
      return;
    }
    // 当前玩家头顶箭头
    int i = s_iFrame & 1 ? SPRITENUM_BATTLE_ARROW_CURRENTPLAYER : SPRITENUM_BATTLE_ARROW_CURRENTPLAYER_RED;
    int x = g_rgPlayerPos[g.wMaxPartyMemberIndex][g_Battle.UI.wCurPlayerIndex][0] - 8;
    int y = g_rgPlayerPos[g.wMaxPartyMemberIndex][g_Battle.UI.wCurPlayerIndex][1] - 74;
    PAL_RLEBlitToSurface(PAL_GetUISprite(i), gpScreen, PAL_XY(x, y));
  }

  // 主 UI 状态机
  switch (g_Battle.UI.state) {
  case kBattleUIWait:
    if (!g_Battle.fEnemyCleared)
      PAL_BattlePlayerCheckReady();
    break;

  case kBattleUISelectMove:
    // 动作菜单
    PAL_BattleUIHandleSelectMove(wPlayerRole);
    break;

  case kBattleUISelectTargetEnemy:
    // 选择敌人
    PAL_BattleUISelectTargetEnemy(s_iFrame);
    break;

  case kBattleUISelectTargetPlayer:
    // 选择队友
    PAL_BattleUISelectTargetPlayer(s_iFrame);
    break;
  }

  PAL_BattleEndFrame();
}

// Show a number on battle screen (indicates HP/MP change).
VOID PAL_BattleUIShowNum(WORD wNum, PAL_POS pos, NUMCOLOR color) {
  for (int i = 0; i < BATTLEUI_MAX_SHOWNUM; i++) {
    if (g_Battle.UI.rgShowNum[i].wNum == 0) {
      g_Battle.UI.rgShowNum[i].wNum = wNum;
      g_Battle.UI.rgShowNum[i].pos = PAL_XY(PAL_X(pos) - 15, PAL_Y(pos));
      g_Battle.UI.rgShowNum[i].color = color;
      g_Battle.UI.rgShowNum[i].pastFrames = 0;
      break;
    }
  }
}
