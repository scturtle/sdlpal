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
#include "palcfg.h"
#include "stddef.h"

SCRIPTENTRY SCRIPT[43557];          // 脚本入口索引表 (用于查找剧情事件或逻辑代码的内存地址)
STORE_T STORE[21];                  // 商店物品列表 (0 为灵壶咒获得物品列表)
ENEMY_T ENEMY[154];                 // 敌人个体属性表
ENEMYTEAM ENEMY_TEAM[380];          // 敌人队伍配置
MAGIC_T MAGIC[104];                 // 技能定义表
BATTLEFIELD_T BATTLEFIELD[58];      // 战场定义
LEVELUPMAGIC_ALL LEVELUP_MAGIC[20]; // 升级习得魔法表
int N_LEVELUP_MAGIC;                // 习得规则的数量
ENEMYPOS ENEMY_POS;                 // 敌人站位坐标表
WORD LEVELUP_EXP[MAX_LEVELS + 1];   // 升级经验表
WORD BATTLE_EFFECT[10][2];

PLAYERROLES gEquipmentEffect[MAX_PLAYER_EQUIPMENTS + 1]; // 装备加成属性
WORD gPlayerStatus[MAX_PLAYER_ROLES][kStatusAll];        // 角色异常状态

DWORD gFrameNum;          // 全局帧计数器
BOOL gEnteringScene;      // 是否正在进入新场景
BOOL gNeedToFadeIn;       // 是否需要淡入画面
BOOL gInBattle;           // 是否处于战斗中
BOOL gAutoBattle;         // 是否处于自动战斗模式
WORD gLastUnequippedItem; // 记录上一个被卸下的装备
PAL_POS gPartyOffset;     // 主角相对于视口的偏移量
WORD gCurPalette;         // 当前调色板 ID
BOOL gNightPalette;       // 是否黑夜
SHORT gWaveProgression;   // 波浪效果的进度计数
WORD gCurrentSaveSlot;

FILE *FP_FBP;  // 战斗背景
FILE *FP_MGO;  // 场景/地图中的动态对象图像 (Map Graphic Object)
FILE *FP_BALL; // 物品道具位图
FILE *FP_DATA; // 非图像数据
FILE *FP_F;    // 角色战斗动作
FILE *FP_FIRE; // 法术特效
FILE *FP_RGM;  // 角色头像 (Role Graphic Map)
FILE *FP_SSS;  // 脚本数据

SAVEDATA g;

INT PAL_InitGlobals(VOID) {
  // init global file handles
  FP_FBP = UTIL_OpenRequiredFile("fbp.mkf");
  FP_MGO = UTIL_OpenRequiredFile("mgo.mkf");
  FP_BALL = UTIL_OpenRequiredFile("ball.mkf");
  FP_DATA = UTIL_OpenRequiredFile("data.mkf");
  FP_F = UTIL_OpenRequiredFile("f.mkf");
  FP_FIRE = UTIL_OpenRequiredFile("fire.mkf");
  FP_RGM = UTIL_OpenRequiredFile("rgm.mkf");
  FP_SSS = UTIL_OpenRequiredFile("sss.mkf");

  // init global static data
  int n_events = PAL_MKFGetChunkSize(0, FP_SSS) / sizeof(EVENTOBJECT);
  if (n_events != MAX_EVENTS)
    TerminateOnError("number of events not matched");
  N_LEVELUP_MAGIC = PAL_MKFGetChunkSize(6, FP_DATA) / sizeof(LEVELUPMAGIC_ALL);

  PAL_MKFReadChunk((LPBYTE)SCRIPT, sizeof(SCRIPT), 4, FP_SSS);
  PAL_MKFReadChunk((LPBYTE)STORE, sizeof(STORE), 0, FP_DATA);
  PAL_MKFReadChunk((LPBYTE)ENEMY, sizeof(ENEMY), 1, FP_DATA);
  PAL_MKFReadChunk((LPBYTE)ENEMY_TEAM, sizeof(ENEMY_TEAM), 2, FP_DATA);
  PAL_MKFReadChunk((LPBYTE)MAGIC, sizeof(MAGIC), 4, FP_DATA);
  PAL_MKFReadChunk((LPBYTE)BATTLEFIELD, sizeof(BATTLEFIELD), 5, FP_DATA);
  PAL_MKFReadChunk((LPBYTE)LEVELUP_MAGIC, sizeof(LEVELUP_MAGIC), 6, FP_DATA);
  PAL_MKFReadChunk((LPBYTE)BATTLE_EFFECT, sizeof(BATTLE_EFFECT), 11, FP_DATA);
  PAL_MKFReadChunk((LPBYTE)(&ENEMY_POS), sizeof(ENEMY_POS), 13, FP_DATA);
  PAL_MKFReadChunk((LPBYTE)LEVELUP_EXP, sizeof(LEVELUP_EXP), 14, FP_DATA);
  return 0;
}

VOID PAL_FreeGlobals(VOID) { PAL_FreeConfig(); }

static VOID PAL_InitGlobalGameData(VOID) {
  // init global vars
  gEnteringScene = TRUE;
  gLastUnequippedItem = 0;

  memset(gPlayerStatus, 0, sizeof(gPlayerStatus));

  // init global data, may be override by saved data later
  memset(&g, 0, sizeof(g));
  g.saveSlot = 1;
  g.wCurScene = 1;
  g.wChaseRange = 1;

  PAL_MKFReadChunk((LPBYTE)(&PLAYER), sizeof(PLAYER), 3, FP_DATA);
  PAL_MKFReadChunk((LPBYTE)g.scenes, sizeof(g.scenes), 1, FP_SSS);
  PAL_MKFReadChunk((LPBYTE)OBJECT, sizeof(OBJECT), 2, FP_SSS);
  PAL_MKFReadChunk((LPBYTE)g.events, sizeof(g.events), 0, FP_SSS);

  for (int i = 0; i < MAX_PLAYER_ROLES; i++) {
    int defaultLevel = PLAYER.rgwLevel[i];
    g.Exp.rgPrimaryExp[i].wLevel = defaultLevel;
    g.Exp.rgHealthExp[i].wLevel = defaultLevel;
    g.Exp.rgMagicExp[i].wLevel = defaultLevel;
    g.Exp.rgAttackExp[i].wLevel = defaultLevel;
    g.Exp.rgMagicPowerExp[i].wLevel = defaultLevel;
    g.Exp.rgDefenseExp[i].wLevel = defaultLevel;
    g.Exp.rgDexterityExp[i].wLevel = defaultLevel;
    g.Exp.rgFleeExp[i].wLevel = defaultLevel;
  }
}

static INT PAL_LoadGame(int iSaveSlot) {
  gCurrentSaveSlot = iSaveSlot;

  // Try to open the specified file
  FILE *fp = UTIL_OpenFile(PAL_va(1, "%d.rpg", iSaveSlot));
  if (!fp)
    return -1;
  fseek(fp, 0, SEEK_END);
  long size = ftell(fp);

  // Validate file size.
  if (size != sizeof(SAVEDATA))
    return -1;

  // Read all data from the file and close.
  fseek(fp, 0, SEEK_SET);
  fread((void *)&g, 1, size, fp);
  fclose(fp);

  // Note: Original code zeroed poison status on load instead of restoring it.
  memset(g.rgPoisonStatus, 0, sizeof(g.rgPoisonStatus));

  gNightPalette = (g._paletteOffset != 0);
  gEnteringScene = FALSE;

  PAL_CompressInventory();
  return 0;
}

VOID PAL_SaveGame(int iSaveSlot, WORD wSavedTimes) {
  gCurrentSaveSlot = iSaveSlot;

  g.saveSlot = wSavedTimes;
  g._paletteOffset = gNightPalette ? 0x180 : 0;
  g.unused = 2; // Hardcoded in original logic
  g.unused2[0] = 0;
  g.unused2[1] = 0;
  g.unused2[2] = 0;

  FILE *fp = UTIL_OpenFileForMode(PAL_va(1, "%d.rpg", iSaveSlot), "wb");
  if (fp != NULL) {
    fwrite(&g, sizeof(SAVEDATA), 1, fp);
    fclose(fp);
  }
}

// Reload the game IN NEXT TICK, avoid reentrant problems.
VOID PAL_ReloadInNextTick(INT iSaveSlot) {
  gCurrentSaveSlot = iSaveSlot; // needed for kLoadGlobalData
  PAL_SetLoadFlags(kLoadGlobalData | kLoadScene | kLoadPlayerSprite);
  gEnteringScene = TRUE;
  gNeedToFadeIn = TRUE;
  gFrameNum = 0;
}

VOID PAL_InitGameData(INT iSaveSlot) {
  PAL_InitGlobalGameData();

  if (iSaveSlot > 0) {
    PAL_LoadGame(iSaveSlot);
  }

  PAL_UpdateEquipmentEffect();
}

INT PAL_CountItem(WORD wObjectID) {
  if (wObjectID == 0)
    return 0;

  int count = 0;

  // Search for the specified item in the inventory
  for (int index = 0; index < MAX_INVENTORY; index++) {
    if (g.rgInventory[index].wItem == wObjectID) {
      count = g.rgInventory[index].nAmount;
      break;
    } else if (g.rgInventory[index].wItem == 0) {
      break;
    }
  }

  // search in player's equipments
  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
    int p = PARTY_PLAYER(i);
    for (int j = 0; j < MAX_PLAYER_EQUIPMENTS; j++) {
      if (PLAYER.rgwEquipment[j][p] == wObjectID) {
        count++;
      }
    }
  }
  return count;
}

BOOL PAL_GetItemIndexToInventory(WORD wObjectID, INT *index) {
  for (*index = 0; *index < MAX_INVENTORY; (*index)++) {
    if (g.rgInventory[*index].wItem == wObjectID) {
      return TRUE;
    } else if (g.rgInventory[*index].wItem == 0) {
      break;
    }
  }
  return FALSE;
}

INT PAL_AddItemToInventory(WORD wObjectID, INT iNum) {
  if (wObjectID == 0)
    return FALSE;
  if (iNum == 0)
    iNum = 1;

  int index = 0;
  bool fFound = PAL_GetItemIndexToInventory(wObjectID, &index);

  if (iNum > 0) {
    // 增加物品
    if (index >= MAX_INVENTORY)
      return FALSE;
    INVENTORY *pItem = &g.rgInventory[index];
    if (!fFound) {
      pItem->wItem = wObjectID;
      pItem->nAmount = 0;
    }
    pItem->nAmount += iNum;
    if (pItem->nAmount > 99)
      pItem->nAmount = 99;
    return TRUE;
  } else {
    // 移除物品
    iNum *= -1;
    if (!fFound)
      return FALSE;
    INVENTORY *pItem = &g.rgInventory[index];
    if (pItem->nAmount < iNum) {
      iNum -= pItem->nAmount;
      pItem->nAmount = 0;
    } else {
      pItem->nAmount -= iNum;
      iNum = 0;
    }
    // 整理物品
    PAL_CompressInventory();
    // 返回缺少的数量 (负数)
    return iNum ? -iNum : TRUE;
  }
}

INT PAL_GetItemAmount(WORD wItem) {
  for (int i = 0; i < MAX_INVENTORY; i++) {
    if (g.rgInventory[i].wItem == 0)
      break;
    if (g.rgInventory[i].wItem == wItem)
      return g.rgInventory[i].nAmount;
  }
  return 0;
}

// Remove all the items in inventory which has a number of zero.
VOID PAL_CompressInventory(VOID) {
  int j = 0;
  for (int i = 0; i < MAX_INVENTORY; i++) {
    if (g.rgInventory[i].nAmount > 0) {
      g.rgInventory[j] = g.rgInventory[i];
      j++;
    }
  }
  for (; j < MAX_INVENTORY; j++) {
    g.rgInventory[j].wItem = 0;
    g.rgInventory[j].nAmount = 0;
    g.rgInventory[j].nAmountInUse = 0;
  }
}

BOOL PAL_IncreaseHPMP(WORD wPlayerRole, SHORT sHP, SHORT sMP) {
  WORD *pHP = &PLAYER.rgwHP[wPlayerRole];
  WORD maxHP = PLAYER.rgwMaxHP[wPlayerRole];
  WORD *pMP = &PLAYER.rgwMP[wPlayerRole];
  WORD maxMP = PLAYER.rgwMaxMP[wPlayerRole];
  WORD origHP = *pHP;
  WORD origMP = *pMP;

  // Only care about alive players
  if (*pHP == 0)
    return FALSE;

  // Change HP
  SHORT newHP = (SHORT)(*pHP) + sHP;
  if (newHP < 0) {
    *pHP = 0;
  } else if (newHP > maxHP) {
    *pHP = maxHP;
  } else {
    *pHP = newHP;
  }

  // Change MP
  SHORT newMP = (SHORT)(*pMP) + sMP;
  if (newMP < 0) {
    *pMP = 0;
  } else if (newMP > maxMP) {
    *pMP = maxMP;
  } else {
    *pMP = newMP;
  }

  // Avoid over treatment
  return origHP != *pHP || origMP != *pMP;
}

// Update the effects of all equipped items for all players.
VOID PAL_UpdateEquipmentEffect(VOID) {
  memset(&(gEquipmentEffect), 0, sizeof(gEquipmentEffect));

  for (int i = 0; i < MAX_PLAYER_ROLES; i++) {
    for (int j = 0; j < MAX_PLAYER_EQUIPMENTS; j++) {
      WORD w = PLAYER.rgwEquipment[j][i];
      if (w != 0) {
        OBJECT[w].item.wScriptOnEquip = PAL_RunTriggerScript(OBJECT[w].item.wScriptOnEquip, (WORD)i);
      }
    }
  }
}

VOID PAL_RemoveEquipmentEffect(WORD wPlayerRole, WORD wEquipPart) {
  WORD *p = (WORD *)(&gEquipmentEffect[wEquipPart]); // HACK
  for (int i = 0; i < sizeof(PLAYERROLES) / sizeof(PLAYERS); i++) {
    p[i * MAX_PLAYER_ROLES + wPlayerRole] = 0;
  }

  // 重置双重攻击
  if (wEquipPart == kBodyPartHand) {
    gPlayerStatus[wPlayerRole][kStatusDualAttack] = 0;
  }
  // 重置毒性抵抗
  if (wEquipPart == kBodyPartWear) {
    // Remove all poisons leveled 99
    short index;
    for (index = 0; index <= (short)g.wMaxPartyMemberIndex; index++) {
      if (PARTY_PLAYER(index) == wPlayerRole)
        break;
    }
    if (index <= (short)g.wMaxPartyMemberIndex) {
      for (int i = 0; i < MAX_POISONS; i++) {
        WORD w = g.rgPoisonStatus[i][index].wPoisonID;
        if (w > 0 && OBJECT[w].poison.wPoisonLevel == 99) {
          g.rgPoisonStatus[i][index].wPoisonID = 0;
          g.rgPoisonStatus[i][index].wPoisonScript = 0;
        }
      }
    }
  }
}

VOID PAL_AddPoisonForPlayer(WORD wPlayerRole, WORD wPoisonID) {
  int index;
  for (index = 0; index <= g.wMaxPartyMemberIndex; index++) {
    if (PARTY_PLAYER(index) == wPlayerRole)
      break;
  }
  if (index > g.wMaxPartyMemberIndex)
    return;

  int i;
  for (i = 0; i < MAX_POISONS; i++) {
    WORD w = g.rgPoisonStatus[i][index].wPoisonID;
    if (w == 0) // empty slot
      break;
    if (w == wPoisonID)
      return; // already poisoned
  }

  if (i < MAX_POISONS) {
    g.rgPoisonStatus[i][index].wPoisonID = wPoisonID;
    g.rgPoisonStatus[i][index].wPoisonScript =
        PAL_RunTriggerScript(OBJECT[wPoisonID].poison.wPlayerScript, wPlayerRole);
  }
}

VOID PAL_CurePoisonByKind(WORD wPlayerRole, WORD wPoisonID) {
  int index;
  for (index = 0; index <= g.wMaxPartyMemberIndex; index++) {
    if (PARTY_PLAYER(index) == wPlayerRole)
      break;
  }
  if (index > g.wMaxPartyMemberIndex)
    return;

  for (int i = 0; i < MAX_POISONS; i++) {
    if (g.rgPoisonStatus[i][index].wPoisonID == wPoisonID) {
      g.rgPoisonStatus[i][index].wPoisonID = 0;
      g.rgPoisonStatus[i][index].wPoisonScript = 0;
    }
  }
}

VOID PAL_CurePoisonByLevel(WORD wPlayerRole, WORD wMaxLevel) {
  int index;
  for (index = 0; index <= g.wMaxPartyMemberIndex; index++) {
    if (PARTY_PLAYER(index) == wPlayerRole)
      break;
  }
  if (index > g.wMaxPartyMemberIndex)
    return;

  for (int i = 0; i < MAX_POISONS; i++) {
    WORD w = g.rgPoisonStatus[i][index].wPoisonID;
    if (w > 0 && OBJECT[w].poison.wPoisonLevel <= wMaxLevel) {
      g.rgPoisonStatus[i][index].wPoisonID = 0;
      g.rgPoisonStatus[i][index].wPoisonScript = 0;
    }
  }
}

BOOL PAL_IsPlayerPoisonedByLevel(WORD wPlayerRole, WORD wMinLevel) {
  int index;
  for (index = 0; index <= g.wMaxPartyMemberIndex; index++) {
    if (PARTY_PLAYER(index) == wPlayerRole)
      break;
  }
  if (index > g.wMaxPartyMemberIndex)
    return FALSE;

  for (int i = 0; i < MAX_POISONS; i++) {
    WORD w = g.rgPoisonStatus[i][index].wPoisonID;
    if (w == 0)
      continue;
    WORD level = OBJECT[w].poison.wPoisonLevel;
    if (level >= wMinLevel && level < 99)
      return TRUE;
  }
  return FALSE;
}

BOOL PAL_IsPlayerPoisonedByKind(WORD wPlayerRole, WORD wPoisonID) {
  int index;
  for (index = 0; index <= g.wMaxPartyMemberIndex; index++) {
    if (PARTY_PLAYER(index) == wPlayerRole)
      break;
  }
  if (index > g.wMaxPartyMemberIndex)
    return FALSE;

  for (int i = 0; i < MAX_POISONS; i++)
    if (g.rgPoisonStatus[i][index].wPoisonID == wPoisonID)
      return TRUE;
  return FALSE;
}

WORD PAL_GetPlayerAttackStrength(WORD wPlayerRole) {
  WORD w = PLAYER.rgwAttackStrength[wPlayerRole];
  for (int i = 0; i <= MAX_PLAYER_EQUIPMENTS; i++)
    w += gEquipmentEffect[i].rgwAttackStrength[wPlayerRole];
  return w;
}

WORD PAL_GetPlayerMagicStrength(WORD wPlayerRole) {
  WORD w = PLAYER.rgwMagicStrength[wPlayerRole];
  for (int i = 0; i <= MAX_PLAYER_EQUIPMENTS; i++)
    w += gEquipmentEffect[i].rgwMagicStrength[wPlayerRole];
  return w;
}

WORD PAL_GetPlayerDefense(WORD wPlayerRole) {
  WORD w = PLAYER.rgwDefense[wPlayerRole];
  for (int i = 0; i <= MAX_PLAYER_EQUIPMENTS; i++)
    w += gEquipmentEffect[i].rgwDefense[wPlayerRole];
  return w;
}

WORD PAL_GetPlayerDexterity(WORD wPlayerRole) {
  WORD w = PLAYER.rgwDexterity[wPlayerRole];
  for (int i = 0; i <= MAX_PLAYER_EQUIPMENTS; i++)
    w += gEquipmentEffect[i].rgwDexterity[wPlayerRole];
  return w;
}

WORD PAL_GetPlayerFleeRate(WORD wPlayerRole) {
  WORD w = PLAYER.rgwFleeRate[wPlayerRole];
  for (int i = 0; i <= MAX_PLAYER_EQUIPMENTS; i++)
    w += gEquipmentEffect[i].rgwFleeRate[wPlayerRole];
  return w;
}

WORD PAL_GetPlayerPoisonResistance(WORD wPlayerRole) {
  WORD w = PLAYER.rgwPoisonResistance[wPlayerRole];
  for (int i = 0; i <= MAX_PLAYER_EQUIPMENTS; i++)
    w += gEquipmentEffect[i].rgwPoisonResistance[wPlayerRole];
  if (w > 100)
    w = 100;
  return w;
}

WORD PAL_GetPlayerElementalResistance(WORD wPlayerRole, INT iAttrib) {
  WORD w = PLAYER.rgwElementalResistance[iAttrib][wPlayerRole];
  for (int i = 0; i <= MAX_PLAYER_EQUIPMENTS; i++)
    w += gEquipmentEffect[i].rgwElementalResistance[iAttrib][wPlayerRole];
  if (w > 100)
    w = 100;
  return w;
}

WORD PAL_GetPlayerBattleSprite(WORD wPlayerRole) {
  WORD w = PLAYER.rgwSpriteNumInBattle[wPlayerRole];
  for (int i = 0; i <= MAX_PLAYER_EQUIPMENTS; i++)
    if (gEquipmentEffect[i].rgwSpriteNumInBattle[wPlayerRole] != 0)
      w = gEquipmentEffect[i].rgwSpriteNumInBattle[wPlayerRole];
  return w;
}

WORD PAL_GetPlayerCooperativeMagic(WORD wPlayerRole) {
  WORD w = PLAYER.rgwCooperativeMagic[wPlayerRole];
  for (int i = 0; i <= MAX_PLAYER_EQUIPMENTS; i++)
    if (gEquipmentEffect[i].rgwCooperativeMagic[wPlayerRole] != 0)
      w = gEquipmentEffect[i].rgwCooperativeMagic[wPlayerRole];
  return w;
}

BOOL PAL_PlayerCanAttackAll(WORD wPlayerRole) {
  for (int i = 0; i <= MAX_PLAYER_EQUIPMENTS; i++)
    if (gEquipmentEffect[i].rgwAttackAll[wPlayerRole] != 0)
      return TRUE;
  return FALSE;
}

BOOL PAL_AddMagic(WORD wPlayerRole, WORD wMagic) {
  for (int i = 0; i < MAX_PLAYER_MAGICS; i++)
    if (PLAYER.rgwMagic[i][wPlayerRole] == wMagic)
      return FALSE;

  int i;
  for (i = 0; i < MAX_PLAYER_MAGICS; i++)
    if (PLAYER.rgwMagic[i][wPlayerRole] == 0)
      break;
  if (i >= MAX_PLAYER_MAGICS)
    return FALSE;

  PLAYER.rgwMagic[i][wPlayerRole] = wMagic;
  return TRUE;
}

VOID PAL_RemoveMagic(WORD wPlayerRole, WORD wMagic) {
  for (int i = 0; i < MAX_PLAYER_MAGICS; i++) {
    if (PLAYER.rgwMagic[i][wPlayerRole] == wMagic) {
      PLAYER.rgwMagic[i][wPlayerRole] = 0;
      break;
    }
  }
}

BOOL PAL_SetPlayerStatus(WORD wPlayerRole, WORD wStatusID, WORD wNumRound) {

  switch (wStatusID) {
  case kStatusConfused:
  case kStatusSleep:
  case kStatusSilence:
  case kStatusParalyzed:
    // for "bad" statuses, don't set the status when we already have it
    if (gPlayerStatus[wPlayerRole][wStatusID] == 0)
      gPlayerStatus[wPlayerRole][wStatusID] = wNumRound;
    return TRUE;

  case kStatusPuppet:
    // only allow dead players for "puppet" status
    if (PLAYER.rgwHP[wPlayerRole] == 0) {
      if (gPlayerStatus[wPlayerRole][wStatusID] < wNumRound) {
        gPlayerStatus[wPlayerRole][wStatusID] = wNumRound;
      }
      return TRUE;
    } else {
      return FALSE;
    }

  case kStatusBravery:
  case kStatusProtect:
  case kStatusDualAttack:
  case kStatusHaste:
    // for "good" statuses, reset the status if the status to be set lasts longer
    if (PLAYER.rgwHP[wPlayerRole] != 0 && gPlayerStatus[wPlayerRole][wStatusID] < wNumRound) {
      gPlayerStatus[wPlayerRole][wStatusID] = wNumRound;
    }
    return TRUE;

  default:
    assert(FALSE);
    return FALSE;
  }
}

VOID PAL_RemovePlayerStatus(WORD wPlayerRole, WORD wStatusID) {
  // Don't remove effects of equipments (两次攻击)
  if (gPlayerStatus[wPlayerRole][wStatusID] <= 999)
    gPlayerStatus[wPlayerRole][wStatusID] = 0;
}

VOID PAL_ClearAllPlayerStatus(VOID) {
  for (int i = 0; i < MAX_PLAYER_ROLES; i++)
    for (int j = 0; j < kStatusAll; j++)
      // Don't remove effects of equipments (两次攻击)
      if (gPlayerStatus[i][j] <= 999)
        gPlayerStatus[i][j] = 0;
}

VOID PAL_PlayerLevelUp(WORD wPlayerRole, WORD wNumLevel) {
  // Add the level
  PLAYER.rgwLevel[wPlayerRole] += wNumLevel;
  if (PLAYER.rgwLevel[wPlayerRole] > MAX_LEVELS)
    PLAYER.rgwLevel[wPlayerRole] = MAX_LEVELS;

  // Increase player's stats
  for (WORD i = 0; i < wNumLevel; i++) {
    PLAYER.rgwMaxHP[wPlayerRole] += 10 + RandomLong(0, 7);
    PLAYER.rgwMaxMP[wPlayerRole] += 8 + RandomLong(0, 5);
    PLAYER.rgwAttackStrength[wPlayerRole] += 4 + RandomLong(0, 1);
    PLAYER.rgwMagicStrength[wPlayerRole] += 4 + RandomLong(0, 1);
    PLAYER.rgwDefense[wPlayerRole] += 2 + RandomLong(0, 1);
    PLAYER.rgwDexterity[wPlayerRole] += 2 + RandomLong(0, 1);
    PLAYER.rgwFleeRate[wPlayerRole] += 2;
  }

#define STAT_LIMIT(t)                                                                                                  \
  {                                                                                                                    \
    if ((t) > 999)                                                                                                     \
      (t) = 999;                                                                                                       \
  }
  STAT_LIMIT(PLAYER.rgwMaxHP[wPlayerRole]);
  STAT_LIMIT(PLAYER.rgwMaxMP[wPlayerRole]);
  STAT_LIMIT(PLAYER.rgwAttackStrength[wPlayerRole]);
  STAT_LIMIT(PLAYER.rgwMagicStrength[wPlayerRole]);
  STAT_LIMIT(PLAYER.rgwDefense[wPlayerRole]);
  STAT_LIMIT(PLAYER.rgwDexterity[wPlayerRole]);
  STAT_LIMIT(PLAYER.rgwFleeRate[wPlayerRole]);
#undef STAT_LIMIT

  // Reset experience points to zero
  g.Exp.rgPrimaryExp[wPlayerRole].wExp = 0;
  g.Exp.rgPrimaryExp[wPlayerRole].wLevel = PLAYER.rgwLevel[wPlayerRole];
}

BOOL PAL_collectItem() {
  if (g.wCollectValue == 0)
    return FALSE;

  int i = RandomLong(1, min(9, g.wCollectValue));
  g.wCollectValue -= i;

  WORD wObject = STORE[0].rgwItems[i - 1];
  PAL_AddItemToInventory(wObject, 1);

  PAL_SetDialogShadow(5);
  PAL_StartDialogWithOffset(kDialogCenterWindow, 0, 0, FALSE, 0, -10);
  WCHAR s[256];
  PAL_swprintf(s, sizeof(s) / sizeof(WCHAR), L"%ls@%ls@", PAL_GetWord(42), PAL_GetWord(wObject));
  LPCBITMAPRLE bg = PAL_GetUISprite(SPRITENUM_ITEMBOX);
  INT width = PAL_RLEGetWidth(bg), height = PAL_RLEGetHeight(bg);
  INT x = (320 - width) / 2, y = (200 - height) / 2;
  PAL_POS pos = PAL_XY(x, y);
  SDL_Rect rect = {x, y, width, height};
  PAL_RLEBlitToSurface(bg, gpScreen, pos);

  BYTE bufImage[2048];
  if (PAL_MKFReadChunk(bufImage, 2048, OBJECT[wObject].item.wBitmap, FP_BALL) > 0) {
    PAL_RLEBlitToSurface(bufImage, gpScreen, PAL_XY(PAL_X(pos) + 8, PAL_Y(pos) + 7));
  }

  VIDEO_UpdateScreen(&rect);

  PAL_ShowDialogText(s);
  PAL_SetDialogShadow(0);
  return TRUE;
}
