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
#include <math.h>

extern WORD g_rgPlayerPos[3][3][2];

BOOL PAL_IsPlayerDying(WORD wPlayerRole) {
  return PLAYER.rgwHP[wPlayerRole] < min(100, PLAYER.rgwMaxHP[wPlayerRole] / 5);
}

BOOL PAL_IsPlayerHealthy(WORD wPlayerRole) {
  return !PAL_IsPlayerDying(wPlayerRole) && gPlayerStatus[wPlayerRole][kStatusSleep] == 0 &&
         gPlayerStatus[wPlayerRole][kStatusConfused] == 0 && gPlayerStatus[wPlayerRole][kStatusSilence] == 0 &&
         gPlayerStatus[wPlayerRole][kStatusParalyzed] == 0 && gPlayerStatus[wPlayerRole][kStatusPuppet] == 0;
}

static BOOL PAL_CanPlayerAct(WORD wPlayerRole, BOOL fConsiderConfused) {
  return PLAYER.rgwHP[wPlayerRole] != 0 && gPlayerStatus[wPlayerRole][kStatusSleep] == 0 &&
         gPlayerStatus[wPlayerRole][kStatusParalyzed] == 0 &&
         (!fConsiderConfused || gPlayerStatus[wPlayerRole][kStatusConfused] == 0);
}

static BOOL PAL_isPlayerBadStatus(WORD wPlayerRole) {
  return gPlayerStatus[wPlayerRole][kStatusConfused] > 0 || gPlayerStatus[wPlayerRole][kStatusSleep] > 0 ||
         gPlayerStatus[wPlayerRole][kStatusParalyzed] > 0;
}

static INT PAL_BattleSelectAutoTargetFrom(INT begin) {
  int prev = g_Battle.UI.iPrevEnemyTarget;

  if (prev >= 0 && prev <= g_Battle.wMaxEnemyIndex && BATTLE_ENEMY[prev].wObjectID != 0 &&
      BATTLE_ENEMY[prev].e.wHealth > 0) {
    return prev;
  }

  for (int count = 0, i = (begin >= 0 ? begin : 0); count < MAX_ENEMIES_IN_TEAM; count++) {
    if (BATTLE_ENEMY[i].wObjectID != 0 && BATTLE_ENEMY[i].e.wHealth > 0)
      return i;
    i = (i + 1) % (g_Battle.wMaxEnemyIndex + 1);
  }

  return -1;
}

INT PAL_BattleSelectAutoTarget(VOID) { return PAL_BattleSelectAutoTargetFrom(0); }

// https://github.com/palxex/palresearch/blob/master/EXE/txts/%E9%9A%8F%E8%AE%B0.txt
static SHORT PAL_CalcBaseDamage(WORD wAttackStrength, WORD wDefense) {
  if (wAttackStrength > wDefense)
    return (SHORT)((wAttackStrength - wDefense * 0.8) * 2 + 0.5);
  else if (wAttackStrength > wDefense * 0.6)
    return (SHORT)(wAttackStrength - wDefense * 0.6 + 0.5);
  else
    return 0;
}

// https://github.com/palxex/palresearch/blob/master/EXE/txts/%E9%9A%8F%E8%AE%B0.txt
static SHORT PAL_CalcMagicDamage(WORD wMagicStrength, WORD wDefense,
                                 const WORD rgwElementalResistance[NUM_MAGIC_ELEMENTAL], WORD wPoisonResistance,
                                 WORD wResistanceMultiplier, WORD wMagicObject) {
  MAGIC_T *magic = &MAGIC[OBJECT[wMagicObject].magic.wMagicNumber];

  // 上浮 10 %
  wMagicStrength *= RandomFloat(10, 11);
  wMagicStrength /= 10;

  SHORT sDamage = PAL_CalcBaseDamage(wMagicStrength, wDefense) / 4 + magic->wBaseDamage;

  WORD wElem = magic->wElemental;
  if (wElem != 0) {
    if (wElem > NUM_MAGIC_ELEMENTAL)
      sDamage *= 10 - ((FLOAT)wPoisonResistance / wResistanceMultiplier); // 毒抗
    else
      sDamage *= 10 - ((FLOAT)rgwElementalResistance[wElem - 1] / wResistanceMultiplier); // 五行抗
    sDamage /= 5;

    if (wElem <= NUM_MAGIC_ELEMENTAL) {
      sDamage *= 10 + BATTLEFIELD[g.wCurBattleField].rgsMagicEffect[wElem - 1]; // 战地属性加成
      sDamage /= 10;
    }
  }
  return sDamage;
}

static SHORT PAL_CalcPhysicalAttackDamage(WORD wAttackStrength, WORD wDefense, WORD wAttackResistance) {
  SHORT sDamage = PAL_CalcBaseDamage(wAttackStrength, wDefense);
  if (wAttackResistance != 0)
    sDamage /= wAttackResistance;
  return sDamage;
}

static SHORT PAL_GetEnemyDexterity(WORD wEnemyIndex) {
  return BATTLE_ENEMY[wEnemyIndex].e.wDexterity + (BATTLE_ENEMY[wEnemyIndex].e.wLevel + 6) * 3;
}

static WORD PAL_GetPlayerActualDexterity(WORD wPlayerRole) {
  WORD wDexterity = PAL_GetPlayerDexterity(wPlayerRole);
  if (gPlayerStatus[wPlayerRole][kStatusHaste] != 0) // 身法
    wDexterity *= 3;
  if (wDexterity > 999)
    wDexterity = 999;
  return wDexterity;
}

static VOID PAL_BattleUpdateAllEnemyFrame() {
  for (int j = 0; j <= g_Battle.wMaxEnemyIndex; j++) {
    BATTLEENEMY *enemy = &BATTLE_ENEMY[j];
    if (enemy->wObjectID == 0 || enemy->rgwStatus[kStatusSleep] != 0 || enemy->rgwStatus[kStatusParalyzed] != 0)
      continue;
    // 切换下一张 idle frame
    if (--enemy->e.wIdleAnimSpeed == 0) {
      enemy->wCurrentFrame++;
      enemy->e.wIdleAnimSpeed = ENEMY[OBJECT[enemy->wObjectID].enemy.wEnemyID].wIdleAnimSpeed;
    }
    if (enemy->wCurrentFrame >= enemy->e.wIdleFrames)
      enemy->wCurrentFrame = 0;
  }
}

VOID PAL_BattleDelay(WORD wDuration, WORD wObjectID, BOOL fUpdateGesture) {
  DWORD dwTime = PAL_GetTicks() + BATTLE_FRAME_TIME;

  for (int i = 0; i < wDuration; i++) {
    // Update the gesture of enemies.
    if (fUpdateGesture)
      PAL_BattleUpdateAllEnemyFrame();

    // Wait for the time of one frame. Accept input here.
    PAL_DelayUntil(dwTime);
    // Set the time of the next frame.
    dwTime = PAL_GetTicks() + BATTLE_FRAME_TIME;

    PAL_BattleMakeScene();
    VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);
    PAL_BattleUIUpdate();

    if (wObjectID != 0) {
      if (wObjectID == BATTLE_LABEL_ESCAPEFAIL) // HACKHACK
        PAL_DrawText(PAL_GetWord(wObjectID), PAL_XY(130, 75), 15, TRUE, FALSE);
      else
        PAL_DrawText(PAL_GetWord(wObjectID), PAL_XY(210, 50), 15, TRUE, FALSE);
    }

    VIDEO_UpdateScreen(NULL);
  }
}

// Backup HP and MP values of all players and enemies.
static VOID PAL_BattleBackupStat(VOID) {
  for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
    if (BATTLE_ENEMY[i].wObjectID == 0)
      continue;
    BATTLE_ENEMY[i].wPrevHP = BATTLE_ENEMY[i].e.wHealth;
  }
  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
    WORD wPlayerRole = PARTY_PLAYER(i);
    BATTLE_PLAYER[i].wPrevHP = PLAYER.rgwHP[wPlayerRole];
    BATTLE_PLAYER[i].wPrevMP = PLAYER.rgwMP[wPlayerRole];
  }
}

// Display the HP and MP changes of all players and enemies.
// Return TRUE if there are any number displayed, FALSE if not.
static BOOL PAL_BattleDisplayStatChange(VOID) {
  BOOL f = FALSE;

  for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
    BATTLEENEMY *enemy = &BATTLE_ENEMY[i];
    if (enemy->wObjectID == 0)
      continue;

    if (enemy->wPrevHP != enemy->e.wHealth) {
      int x = PAL_X(enemy->pos) - 9;
      int y = PAL_Y(enemy->pos) - 115;
      if (y < 10)
        y = 10;
      SHORT sDamage = enemy->e.wHealth - enemy->wPrevHP;
      PAL_BattleUIShowNum(abs(sDamage), PAL_XY(x, y), sDamage < 0 ? kNumColorBlue : kNumColorYellow);
      f = TRUE;
    }
  }

  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
    WORD wPlayerRole = PARTY_PLAYER(i);
    BATTLEPLAYER *player = &BATTLE_PLAYER[i];

    if (player->wPrevHP != PLAYER.rgwHP[wPlayerRole]) {
      int x = PAL_X(player->pos) - 9;
      int y = PAL_Y(player->pos) - 75;
      if (y < 10)
        y = 10;
      SHORT sDamage = PLAYER.rgwHP[wPlayerRole] - player->wPrevHP;
      PAL_BattleUIShowNum(abs(sDamage), PAL_XY(x, y), sDamage < 0 ? kNumColorBlue : kNumColorYellow);
      f = TRUE;
    }

    if (player->wPrevMP != PLAYER.rgwMP[wPlayerRole]) {
      int x = PAL_X(player->pos) - 9;
      int y = PAL_Y(player->pos) - 67;
      if (y < 10)
        y = 10;
      SHORT sDamage = PLAYER.rgwMP[wPlayerRole] - player->wPrevMP;
      // Only show MP increasing
      if (sDamage > 0)
        PAL_BattleUIShowNum((WORD)(sDamage), PAL_XY(x, y), kNumColorCyan);
      f = TRUE;
    }
  }

  return f;
}

// Essential checks after an action is executed.
static VOID PAL_BattlePostActionCheck(BOOL fCheckPlayers) {
  BOOL fFade = FALSE;
  BOOL fEnemyRemaining = FALSE;

  for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
    BATTLEENEMY *enemy = &BATTLE_ENEMY[i];
    if (enemy->wObjectID == 0)
      continue;

    if ((SHORT)(enemy->e.wHealth) <= 0) {
      // This enemy is KO'ed
      g_Battle.iExpGained += enemy->e.wExp;
      g_Battle.iCashGained += enemy->e.wCash;

      AUDIO_PlaySound(enemy->e.wDeathSound);
      enemy->wObjectID = 0;
      if (enemy->lpSprite)
        free(enemy->lpSprite);
      enemy->lpSprite = NULL;
      fFade = TRUE;

      continue;
    }

    fEnemyRemaining = TRUE;
  }

  if (!fEnemyRemaining) {
    g_Battle.fEnemyCleared = TRUE;
    g_Battle.UI.state = kBattleUIWait;
  }

  if (fCheckPlayers && !gAutoBattle) {
    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
      WORD w = PARTY_PLAYER(i);

      // This player is just KO'ed
      if (PLAYER.rgwHP[w] < BATTLE_PLAYER[i].wPrevHP && PLAYER.rgwHP[w] == 0) {
        WORD wCover = PLAYER.rgwCoveredBy[w];

        if (!PAL_CanPlayerAct(wCover, TRUE))
          continue;

        int j;
        for (j = 0; j <= g.wMaxPartyMemberIndex; j++)
          if (PARTY_PLAYER(j) == wCover)
            break;
        if (j > g.wMaxPartyMemberIndex)
          continue;

        WORD wName = PLAYER.rgwName[wCover];

        if (OBJECT[wName].player.wScriptOnFriendDeath != 0) {
          PAL_BattleDelay(10, 0, TRUE);

          PAL_BattleMakeScene();
          VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);
          VIDEO_UpdateScreen(NULL);

          g_Battle.BattleResult = kBattleResultPause;

          // 触发友方愤怒
          OBJECT[wName].player.wScriptOnFriendDeath =
              PAL_RunTriggerScript(OBJECT[wName].player.wScriptOnFriendDeath, wCover);

          g_Battle.BattleResult = kBattleResultOnGoing;

          PAL_ClearKeyState();
          goto end;
        }
      }
    }

    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
      WORD w = PARTY_PLAYER(i);

      if (gPlayerStatus[w][kStatusSleep] != 0 || gPlayerStatus[w][kStatusConfused] != 0)
        continue;

      if (PLAYER.rgwHP[w] < BATTLE_PLAYER[i].wPrevHP && PLAYER.rgwHP[w] > 0 && PAL_IsPlayerDying(w) &&
          BATTLE_PLAYER[i].wPrevHP >= PLAYER.rgwMaxHP[w] / 5) {
        WORD wCover = PLAYER.rgwCoveredBy[w];

        if (!PAL_CanPlayerAct(wCover, TRUE))
          continue;

        int j;
        for (j = 0; j <= g.wMaxPartyMemberIndex; j++)
          if (PARTY_PLAYER(j) == wCover)
            break;
        if (j > g.wMaxPartyMemberIndex)
          continue;

        WORD wName = PLAYER.rgwName[w];

        AUDIO_PlaySound(PLAYER.rgwDyingSound[w]);

        if (OBJECT[wName].player.wScriptOnDying != 0) {
          PAL_BattleDelay(10, 0, TRUE);

          PAL_BattleMakeScene();
          VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);
          VIDEO_UpdateScreen(NULL);

          g_Battle.BattleResult = kBattleResultPause;

          // 触发自己虚弱
          OBJECT[wName].player.wScriptOnDying = PAL_RunTriggerScript(OBJECT[wName].player.wScriptOnDying, w);

          g_Battle.BattleResult = kBattleResultOnGoing;
          PAL_ClearKeyState();
        }

        goto end;
      }
    }
  }

end:
  if (fFade) {
    VIDEO_BackupScreen(g_Battle.lpSceneBuf);
    PAL_BattleMakeScene();
    PAL_BattleFadeScene();
  }

  // Fade out the summoned god
  if (g_Battle.lpSummonSprite != NULL) {
    PAL_BattleUpdateFighters();
    PAL_BattleDelay(1, 0, FALSE);

    free(g_Battle.lpSummonSprite);
    g_Battle.lpSummonSprite = NULL;

    g_Battle.sBackgroundColorShift = 0;

    VIDEO_BackupScreen(g_Battle.lpSceneBuf);
    PAL_BattleMakeScene();
    PAL_BattleFadeScene();
  }
}

// Update players' and enemies' gesture frames and locations in battle.
VOID PAL_BattleUpdateFighters(VOID) {
  // Update the gesture frame for all players
  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
    WORD wPlayerRole = PARTY_PLAYER(i);
    BATTLEPLAYER *player = &BATTLE_PLAYER[i];

    if (!player->fDefending)
      player->pos = player->posOriginal;

    player->iColorShift = 0;

    if (PLAYER.rgwHP[wPlayerRole] == 0) {
      if (gPlayerStatus[wPlayerRole][kStatusPuppet] == 0)
        player->wCurrentFrame = 2; // dead
      else
        player->wCurrentFrame = 0; // puppet
    } else {
      if (gPlayerStatus[wPlayerRole][kStatusSleep] != 0 || PAL_IsPlayerDying(wPlayerRole))
        player->wCurrentFrame = 1;
      else if (player->fDefending && !g_Battle.fEnemyCleared)
        player->wCurrentFrame = 3;
      else
        player->wCurrentFrame = 0;
    }
  }

  // Reset the pos / color / frame for all enemies
  for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
    BATTLEENEMY *enemy = &BATTLE_ENEMY[i];
    if (enemy->wObjectID == 0)
      continue;

    enemy->pos = enemy->posOriginal;
    enemy->iColorShift = 0;

    if (enemy->rgwStatus[kStatusSleep] > 0 || enemy->rgwStatus[kStatusParalyzed] > 0) {
      enemy->wCurrentFrame = 0;
    }
  }

  // Update the gesture frame for all enemies
  PAL_BattleUpdateAllEnemyFrame();
}

static WORD PAL_getPlayerDexterity(int i) {
  WORD wPlayerRole = PARTY_PLAYER(i);
  WORD wDexterity = PAL_GetPlayerActualDexterity(wPlayerRole);

  switch (BATTLE_PLAYER[i].action.ActionType) {
  case kBattleActionCoopMagic:
    wDexterity *= 10;
    break;
  case kBattleActionDefend:
    wDexterity *= 5;
    break;
  case kBattleActionMagic:
    if ((OBJECT[BATTLE_PLAYER[i].action.wActionID].magic.wFlags & kMagicFlagUsableToEnemy) == 0) // 对友方施法
      wDexterity *= 3;
    break;
  case kBattleActionFlee:
    wDexterity /= 2;
    break;
  case kBattleActionUseItem:
    wDexterity *= 3;
    break;
  default:
    break;
  }

  if (PAL_IsPlayerDying(wPlayerRole))
    wDexterity /= 2;

  return wDexterity;
}

VOID PAL_BattlePlayerCheckReady(VOID) {
  // 有人处于指令状态，就调用 UI 显示行动选择菜单, 一次只处理一个人
  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
    if (BATTLE_PLAYER[i].state == kFighterCom) {
      PAL_BattleUIPlayerReady(i);
      break;
    }
  }
}

// 在战斗界面的每一帧被调用，负责更新画面、检测胜负、处理玩家输入（选菜单）、计算行动顺序（速度）、以及执行具体的攻击或法术逻辑。
VOID PAL_BattleStartFrame(VOID) {
  if (!g_Battle.fEnemyCleared)
    PAL_BattleUpdateFighters();

  // Update the scene
  PAL_BattleMakeScene();
  VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);

  // If all enemies are cleared. Won the battle.
  if (g_Battle.fEnemyCleared) {
    g_Battle.BattleResult = kBattleResultWon;
    AUDIO_PlaySound(0);
    return;
  }

  // If all players are dead. Lost the battle.
  BOOL fEnded = TRUE;
  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
    WORD wPlayerRole = PARTY_PLAYER(i);
    if (PLAYER.rgwHP[wPlayerRole] != 0) {
      fEnded = FALSE;
      break;
    }
  }
  if (fEnded) {
    g_Battle.BattleResult = kBattleResultLost;
    return;
  }

  if (g_Battle.Phase == kBattlePhaseSelectAction) {
    if (g_Battle.UI.state == kBattleUIWait) {
      int idx;
      for (idx = 0; idx <= g.wMaxPartyMemberIndex; idx++) {
        WORD wPlayerRole = PARTY_PLAYER(idx);

        // Don't select action for this player cannot act
        if (!PAL_CanPlayerAct(wPlayerRole, TRUE))
          continue;

        // Start the menu for the first player whose action is not yet selected.
        if (BATTLE_PLAYER[idx].state == kFighterWait) {
          g_Battle.wMovingPlayerIndex = idx;
          BATTLE_PLAYER[idx].state = kFighterCom;
          PAL_BattleUIPlayerReady(idx);
          break;
        } else if (BATTLE_PLAYER[idx].action.ActionType == kBattleActionCoopMagic) {
          // 如果有人选择了合体技, 这通常消耗所有人的回合, 因此跳过后续队员的指令选择
          idx = g.wMaxPartyMemberIndex + 1;
          break;
        }
      }

      // 所有角色的指令都下达完毕, 进入回合结算准备
      if (idx > g.wMaxPartyMemberIndex) {
        // Backup all actions once not repeating.
        if (!g_Battle.fRepeat) {
          for (int i = 0; i <= g.wMaxPartyMemberIndex; i++)
            BATTLE_PLAYER[i].prevAction = BATTLE_PLAYER[i].action;
        }

        // Actions for all players are decided, fill in the action queue.
        g_Battle.fRepeat = FALSE;
        g_Battle.fForce = FALSE;
        g_Battle.fFlee = FALSE;
        g_Battle.fPrevAutoAtk = g_Battle.UI.fAutoAttack;
        g_Battle.fPrevPlayerAutoAtk = FALSE;

        g_Battle.iCurAction = 0;

        // Reset queue.
        for (int i = 0; i < MAX_ACTIONQUEUE_ITEMS; i++) {
          g_Battle.ActionQueue[i].wIndex = 0xFFFF;
          g_Battle.ActionQueue[i].fIsSecond = FALSE;
          g_Battle.ActionQueue[i].wDexterity = 0xFFFF;
        }

        int j = 0; // index in queue

        // Put all enemies into action queue.
        for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
          if (BATTLE_ENEMY[i].wObjectID == 0)
            continue;

          g_Battle.ActionQueue[j].fIsEnemy = TRUE;
          g_Battle.ActionQueue[j].wIndex = i;
          g_Battle.ActionQueue[j].fIsSecond = FALSE;
          g_Battle.ActionQueue[j].wDexterity = PAL_GetEnemyDexterity(i) * RandomFloat(0.9f, 1.1f);
          j++;

          if (BATTLE_ENEMY[i].e.wDualMove) {
            g_Battle.ActionQueue[j].fIsEnemy = TRUE;
            g_Battle.ActionQueue[j].wIndex = i;
            g_Battle.ActionQueue[j].fIsSecond = FALSE;
            g_Battle.ActionQueue[j].wDexterity = PAL_GetEnemyDexterity(i) * RandomFloat(0.9f, 1.1f);
            // 比较两次攻击的身法, 强制让身法较低 (较慢) 的那次攻击标记为第二次
            if (g_Battle.ActionQueue[j].wDexterity <= g_Battle.ActionQueue[j - 1].wDexterity)
              g_Battle.ActionQueue[j].fIsSecond = TRUE;
            else
              g_Battle.ActionQueue[j - 1].fIsSecond = TRUE;
            j++;
          }
        }

        // Put all players into action queue.
        for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
          WORD wPlayerRole = PARTY_PLAYER(i);

          g_Battle.ActionQueue[j].fIsEnemy = FALSE;
          g_Battle.ActionQueue[j].wIndex = i;

          if (!PAL_CanPlayerAct(wPlayerRole, FALSE)) {
            // players who are unable to move should attack physically if recovered in the same turn
            g_Battle.ActionQueue[j].wDexterity = 0;
            BATTLE_PLAYER[i].action.ActionType = kBattleActionAttack;
            BATTLE_PLAYER[i].action.wActionID = 0;
            BATTLE_PLAYER[i].state = kFighterAct;
          } else {
            // 混乱状态强制改为普攻
            if (gPlayerStatus[wPlayerRole][kStatusConfused] > 0) {
              BATTLE_PLAYER[i].action.ActionType = kBattleActionAttack;
              BATTLE_PLAYER[i].action.wActionID = 0; // avoid be deduced to autoattack
              BATTLE_PLAYER[i].state = kFighterAct;
            }
            g_Battle.ActionQueue[j].wDexterity = PAL_getPlayerDexterity(i) * RandomFloat(0.9f, 1.1f);
          }

          j++;
        }

        // Sort the action queue by dexterity value
        for (int i = 0; i < MAX_ACTIONQUEUE_ITEMS; i++) {
          for (int j = i; j < MAX_ACTIONQUEUE_ITEMS; j++) {
            if ((SHORT)g_Battle.ActionQueue[i].wDexterity < (SHORT)g_Battle.ActionQueue[j].wDexterity) {
              ACTIONQUEUE t = g_Battle.ActionQueue[i];
              g_Battle.ActionQueue[i] = g_Battle.ActionQueue[j];
              g_Battle.ActionQueue[j] = t;
            }
          }
        }

        // Perform the actions
        g_Battle.Phase = kBattlePhasePerformAction;
      }
    }
  } else {
    // Are all actions finished?
    if (g_Battle.iCurAction >= MAX_ACTIONQUEUE_ITEMS ||
        g_Battle.ActionQueue[g_Battle.iCurAction].wDexterity == 0xFFFF) {
      // Restore player from defending
      for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
        BATTLE_PLAYER[i].fDefending = FALSE;
        BATTLE_PLAYER[i].pos = BATTLE_PLAYER[i].posOriginal;
      }

      // 执行中毒脚本
      PAL_BattleBackupStat();

      for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
        WORD wPlayerRole = PARTY_PLAYER(i);

        for (int j = 0; j < MAX_POISONS; j++) {
          if (g.rgPoisonStatus[j][i].wPoisonID != 0) {
            g.rgPoisonStatus[j][i].wPoisonScript =
                PAL_RunTriggerScript(g.rgPoisonStatus[j][i].wPoisonScript, wPlayerRole);
          }
        }

        // 递减状态持续回合数
        for (int j = 0; j < kStatusAll; j++)
          if (gPlayerStatus[wPlayerRole][j] > 0)
            gPlayerStatus[wPlayerRole][j]--;
      }

      for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
        for (int j = 0; j < MAX_POISONS; j++) {
          if (BATTLE_ENEMY[i].rgPoisons[j].wPoisonID != 0) {
            BATTLE_ENEMY[i].rgPoisons[j].wPoisonScript =
                PAL_RunTriggerScript(BATTLE_ENEMY[i].rgPoisons[j].wPoisonScript, (WORD)i);
          }
        }

        // 递减状态持续回合数
        for (int j = 0; j < kStatusAll; j++)
          if (BATTLE_ENEMY[i].rgwStatus[j] > 0)
            BATTLE_ENEMY[i].rgwStatus[j]--;
      }

      PAL_BattlePostActionCheck(FALSE);

      if (PAL_BattleDisplayStatChange())
        PAL_BattleDelay(8, 0, TRUE);

      // 更新隐身剩余回合
      if (g_Battle.iHidingTime > 0) {
        if (--g_Battle.iHidingTime == 0) {
          VIDEO_BackupScreen(g_Battle.lpSceneBuf);
          PAL_BattleMakeScene();
          PAL_BattleFadeScene();
        }
      }

      // 运行敌方回合开始脚本
      if (g_Battle.iHidingTime == 0) {
        for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
          if (BATTLE_ENEMY[i].wObjectID == 0)
            continue;
          BATTLE_ENEMY[i].wScriptOnTurnStart = PAL_RunTriggerScript(BATTLE_ENEMY[i].wScriptOnTurnStart, i);
        }
      }

      // Clear all item-using records
      for (int i = 0; i < MAX_INVENTORY; i++)
        g.rgInventory[i].nAmountInUse = 0;

      // Proceed to next turn...
      g_Battle.Phase = kBattlePhaseSelectAction;
      g_Battle.fThisTurnCoop = FALSE;
    } else {
      // Perform action in queue.
      int idx = g_Battle.ActionQueue[g_Battle.iCurAction].wIndex;
      BATTLEENEMY *enemy = &BATTLE_ENEMY[idx];

      if (g_Battle.ActionQueue[g_Battle.iCurAction].fIsEnemy) {
        // 敌人行动
        if (g_Battle.iHidingTime == 0 && BATTLE_ENEMY[idx].wObjectID != 0) {
          if (enemy->rgwStatus[kStatusConfused] == 0 && enemy->rgwStatus[kStatusParalyzed] == 0 &&
              enemy->rgwStatus[kStatusSleep] == 0)
            enemy->wScriptOnReady = PAL_RunTriggerScript(enemy->wScriptOnReady, idx);

          g_Battle.fEnemyMoving = TRUE;
          PAL_BattleEnemyPerformAction(idx);
          g_Battle.fEnemyMoving = FALSE;
        }
      } else if (BATTLE_PLAYER[idx].state == kFighterAct) {
        // 玩家行动
        BATTLEPLAYER *player = &BATTLE_PLAYER[idx];
        WORD wPlayerRole = PARTY_PLAYER(idx);

        // 行动前再次检查状态, 防止中间状态改变
        if (PLAYER.rgwHP[wPlayerRole] == 0) {
          if (gPlayerStatus[wPlayerRole][kStatusPuppet] == 0)
            player->action.ActionType = kBattleActionPass;
        } else if (gPlayerStatus[wPlayerRole][kStatusSleep] > 0 || gPlayerStatus[wPlayerRole][kStatusParalyzed] > 0) {
          player->action.ActionType = kBattleActionPass;
        } else if (gPlayerStatus[wPlayerRole][kStatusConfused] > 0) {
          player->action.ActionType = (PAL_IsPlayerDying(wPlayerRole) ? kBattleActionPass : kBattleActionAttackMate);
        } else if (player->action.ActionType == kBattleActionAttack && player->action.wActionID != 0) {
          // ref: PAL_BattleCommitAction
          g_Battle.fPrevPlayerAutoAtk = TRUE;
        } else if (g_Battle.fPrevPlayerAutoAtk) {
          // 本回合有人选择围攻, 强制转换为普通攻击
          g_Battle.UI.wCurPlayerIndex = idx;
          g_Battle.UI.iSelectedIndex = player->action.sTarget;
          g_Battle.UI.wActionType = kBattleActionAttack;
          PAL_BattleCommitAction(FALSE);
        }

        // Perform the action for this player.
        g_Battle.wMovingPlayerIndex = idx;
        PAL_BattlePlayerPerformAction(idx);
      }

      g_Battle.iCurAction++;
    }
  }

  // The R and F keys and Fleeing should affect all players
  if (g_Battle.UI.MenuState == kBattleMenuMain && g_Battle.UI.state == kBattleUISelectMove) {
    if (g_InputState.dwKeyPress & kKeyRepeat) {
      g_Battle.fRepeat = TRUE;
      g_Battle.UI.fAutoAttack = g_Battle.fPrevAutoAtk;
    } else if (g_InputState.dwKeyPress & kKeyForce) {
      g_Battle.fForce = TRUE;
    }
  }

  if (g_Battle.fRepeat)
    g_InputState.dwKeyPress = kKeyRepeat;
  else if (g_Battle.fForce)
    g_InputState.dwKeyPress = kKeyForce;
  else if (g_Battle.fFlee)
    g_InputState.dwKeyPress = kKeyFlee;

  // Update the battle UI
  PAL_BattleUIUpdate();
}

// Commit the action which the player decided.
VOID PAL_BattleCommitAction(BOOL fRepeat) {
  BATTLEPLAYER *player = &BATTLE_PLAYER[g_Battle.UI.wCurPlayerIndex];

  if (!fRepeat) {
    // clear action cache first; avoid cache pollution
    memset(&player->action, 0, sizeof(player->action));
    player->action.ActionType = g_Battle.UI.wActionType;
    player->action.sTarget = (SHORT)g_Battle.UI.iSelectedIndex;

    if (g_Battle.UI.wActionType == kBattleActionAttack)
      player->action.wActionID = (g_Battle.UI.fAutoAttack ? 1 : 0);
    else
      player->action.wActionID = g_Battle.UI.wObjectID;

  } else {
    // 重复上回合的动作
    SHORT target = player->action.sTarget;
    player->action = player->prevAction;
    player->action.sTarget = target;

    if (player->action.ActionType == kBattleActionPass) {
      player->action.ActionType = kBattleActionAttack;
      player->action.wActionID = 0;
      player->action.sTarget = -1;
    }
  }

  // 有效性检查与资源预扣除
  if (player->action.ActionType == kBattleActionMagic) {
    MAGIC_T *magic = &MAGIC[OBJECT[player->action.wActionID].magic.wMagicNumber];

    if (PLAYER.rgwMP[PARTY_PLAYER(g_Battle.UI.wCurPlayerIndex)] < magic->wCostMP) {
      WORD type = magic->wType;
      // MP 不足降级处理
      if (type == kMagicTypeApplyToPlayer || type == kMagicTypeApplyToParty || type == kMagicTypeTrance) {
        player->action.ActionType = kBattleActionDefend;
      } else {
        player->action.ActionType = kBattleActionAttack;
        if (player->action.sTarget == -1)
          player->action.sTarget = 0;
        player->action.wActionID = 0;
      }
    }
  } else if ((player->action.ActionType == kBattleActionUseItem &&
              (OBJECT[player->action.wActionID].item.wFlags & kItemFlagConsuming)) ||
             player->action.ActionType == kBattleActionThrowItem) {

    for (WORD w = 0; w < MAX_INVENTORY; w++) {
      if (g.rgInventory[w].wItem == player->action.wActionID) {
        g.rgInventory[w].nAmountInUse++; // 标记物品被占用
        break;
      }
    }
  }

  // 如果动作是逃跑，设置全局战斗标志
  if (g_Battle.UI.wActionType == kBattleActionFlee)
    g_Battle.fFlee = TRUE;

  player->state = kFighterAct;
  g_Battle.UI.state = kBattleUIWait;
}

// Show the physical attack effect for player.
static VOID PAL_BattleShowPlayerAttackAnim(WORD wPlayerIndex, BOOL fCritical) {
  WORD wPlayerRole = PARTY_PLAYER(wPlayerIndex);
  BATTLEPLAYER *player = &BATTLE_PLAYER[wPlayerIndex];
  SHORT sTarget = player->action.sTarget;

  int enemy_x = 0, enemy_y = 0, enemy_h = 0, dist = 0;
  if (sTarget != -1) {
    // 单体攻击，获取目标敌人的坐标、高度
    enemy_x = PAL_X(BATTLE_ENEMY[sTarget].pos);
    enemy_y = PAL_Y(BATTLE_ENEMY[sTarget].pos);
    enemy_h = PAL_RLEGetHeight(PAL_SpriteGetFrame(BATTLE_ENEMY[sTarget].lpSprite, BATTLE_ENEMY[sTarget].wCurrentFrame));
    // 目标在后排，计算偏移量
    if (sTarget >= 3)
      dist = (sTarget - wPlayerIndex) * 8;
  } else {
    // 全体攻击默认中心点
    enemy_x = 150;
    enemy_y = 100;
  }

  // Play the attack voice
  if (PLAYER.rgwHP[wPlayerRole] > 0) {
    if (!fCritical)
      AUDIO_PlaySound(PLAYER.rgwAttackSound[wPlayerRole]);
    else
      AUDIO_PlaySound(PLAYER.rgwCriticalSound[wPlayerRole]);
  }

  // Show the animation
  int x = enemy_x - dist + 64;
  int y = enemy_y + dist + 20;

  // 抬手近身
  player->wCurrentFrame = 8;
  if (gPlayerStatus[wPlayerRole][kStatusDualAttack] > 0 && PAL_PlayerCanAttackAll(wPlayerRole)) {
    // 原地
    x = g_rgPlayerPos[g.wMaxPartyMemberIndex][wPlayerIndex][0] - 8;
    y = g_rgPlayerPos[g.wMaxPartyMemberIndex][wPlayerIndex][1] - 4;
    player->pos = player->fSecondAttack ? PAL_XY(x - 12, y - 8) : PAL_XY(x, y);
  } else {
    player->pos = PAL_XY(x, y);
  }

  PAL_BattleDelay(2, 0, TRUE);

  x -= 10;
  y -= 2;
  if (gPlayerStatus[wPlayerRole][kStatusDualAttack] > 0 && PAL_PlayerCanAttackAll(wPlayerRole)) {
    // 原地
    x = g_rgPlayerPos[g.wMaxPartyMemberIndex][wPlayerIndex][0] - 8;
    y = g_rgPlayerPos[g.wMaxPartyMemberIndex][wPlayerIndex][1] - 4;
    player->pos = player->fSecondAttack ? PAL_XY(x - 12, y - 8) : PAL_XY(x, y);
  } else {
    player->pos = PAL_XY(x, y);
  }

  PAL_BattleDelay(1, 0, TRUE);

  // 攻击动作
  player->wCurrentFrame = 9;
  x -= 16;
  y -= 4;

  AUDIO_PlaySound(PLAYER.rgwWeaponSound[wPlayerRole]);

  x = enemy_x;
  y = enemy_y - enemy_h / 3 + 10;

  DWORD dwTime = PAL_GetTicks() + BATTLE_FRAME_TIME;

  int index = BATTLE_EFFECT[PAL_GetPlayerBattleSprite(wPlayerRole)][1] * 3;

  for (int i = 0; i < 3; i++) {
    LPCBITMAPRLE b = PAL_SpriteGetFrame(g_Battle.lpEffectSprite, index++); // 攻击特效

    // Wait for the time of one frame. Accept input here.
    PAL_DelayUntil(dwTime);

    // Set the time of the next frame.
    dwTime = PAL_GetTicks() + BATTLE_FRAME_TIME;

    // Update the gesture of enemies.
    PAL_BattleUpdateAllEnemyFrame();

    PAL_BattleMakeScene();
    VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);

    if (PAL_PlayerCanAttackAll(wPlayerRole)) {
      for (int j = 0; j < MAX_ENEMIES_IN_TEAM; j++) {
        if (BATTLE_ENEMY[j].wObjectID != 0) {
          x = ENEMY_POS.pos[j][g_Battle.wMaxEnemyIndex].x;
          y = ENEMY_POS.pos[j][g_Battle.wMaxEnemyIndex].y;
          y += BATTLE_ENEMY[j].e.wYPosOffset;
          PAL_RLEBlitToSurface(b, gpScreen, PAL_XY(x - PAL_RLEGetWidth(b) / 2, y - PAL_RLEGetHeight(b)));
        }
      }
    } else {
      PAL_RLEBlitToSurface(b, gpScreen, PAL_XY(x - PAL_RLEGetWidth(b) / 2, y - PAL_RLEGetHeight(b)));
    }

    x -= 16;
    y += 16;

    PAL_BattleUIUpdate();

    // 后两帧敌人受击高亮
    if (i == 0) {
      if (sTarget == -1) {
        for (int j = 0; j <= g_Battle.wMaxEnemyIndex; j++)
          BATTLE_ENEMY[j].iColorShift = 6;
      } else {
        BATTLE_ENEMY[sTarget].iColorShift = 6;
      }

      PAL_BattleDisplayStatChange();
      PAL_BattleBackupStat();
    }

    VIDEO_UpdateScreen(NULL);

    // 最后一帧小撤步
    if (i == 1)
      player->pos = PAL_XY_OFFSET(player->pos, 2, 1);
  }

  dist = 8;

  for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++)
    BATTLE_ENEMY[i].iColorShift = 0;

  // 击退效果
  if (sTarget == -1) {
    for (int i = 0; i < 3; i++) {
      for (int j = 0; j <= g_Battle.wMaxEnemyIndex; j++)
        BATTLE_ENEMY[j].pos = PAL_XY_OFFSET(BATTLE_ENEMY[j].pos, -dist, 0);
      PAL_BattleDelay(1, 0, TRUE);
      dist /= -2;
    }
  } else {
    for (int i = 0; i < 3; i++) {
      BATTLE_ENEMY[sTarget].pos = PAL_XY_OFFSET(BATTLE_ENEMY[sTarget].pos, -dist, dist / (-2));
      PAL_BattleDelay(1, 0, TRUE);
      dist /= -2;
    }
  }
}

static VOID PAL_BattleShowPlayerUseItemAnim(WORD wPlayerIndex, WORD wObjectID, SHORT sTarget) {
  PAL_BattleDelay(4, 0, TRUE);
  BATTLE_PLAYER[wPlayerIndex].pos = PAL_XY_OFFSET(BATTLE_PLAYER[wPlayerIndex].pos, -15, -7);
  BATTLE_PLAYER[wPlayerIndex].wCurrentFrame = 5;
  AUDIO_PlaySound(28);
  // 渐亮
  for (int i = 0; i <= 6; i++) {
    if (sTarget == -1) {
      for (int j = 0; j <= g.wMaxPartyMemberIndex; j++)
        BATTLE_PLAYER[j].iColorShift = i;
    } else {
      BATTLE_PLAYER[sTarget].iColorShift = i;
    }
    PAL_BattleDelay(1, wObjectID, TRUE);
  }
  // 渐暗
  for (int i = 5; i >= 0; i--) {
    if (sTarget == -1) {
      for (int j = 0; j <= g.wMaxPartyMemberIndex; j++)
        BATTLE_PLAYER[j].iColorShift = i;
    } else {
      BATTLE_PLAYER[sTarget].iColorShift = i;
    }
    PAL_BattleDelay(1, wObjectID, TRUE);
  }
}

// Show the effect for player before using a magic.
VOID PAL_BattleShowPlayerPreMagicAnim(WORD wPlayerIndex, BOOL fSummon) {
  WORD wPlayerRole = PARTY_PLAYER(wPlayerIndex);

  // 前进几步
  for (int i = 0; i < 4; i++) {
    BATTLE_PLAYER[wPlayerIndex].pos = PAL_XY_OFFSET(BATTLE_PLAYER[wPlayerIndex].pos, -(4 - i), -(4 - i) / 2);
    PAL_BattleDelay(1, 0, TRUE);
  }
  PAL_BattleDelay(2, 0, TRUE);

  BATTLE_PLAYER[wPlayerIndex].wCurrentFrame = 5;

  if (!fSummon) {
    AUDIO_PlaySound(PLAYER.rgwMagicSound[wPlayerRole]);

    DWORD dwTime = PAL_GetTicks();
    PAL_POS pos = BATTLE_PLAYER[wPlayerIndex].pos;
    int index = BATTLE_EFFECT[PAL_GetPlayerBattleSprite(wPlayerRole)][0] * 10 + 15;

    for (int i = 0; i < 10; i++) {
      LPCBITMAPRLE b = PAL_SpriteGetFrame(g_Battle.lpEffectSprite, index++);
      int width = PAL_RLEGetWidth(b);
      int height = PAL_RLEGetHeight(b);

      PAL_DelayUntil(dwTime);
      dwTime = PAL_GetTicks() + BATTLE_FRAME_TIME;

      PAL_BattleUpdateAllEnemyFrame();

      PAL_BattleMakeScene();
      VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);

      PAL_RLEBlitToSurface(b, gpScreen, PAL_XY_OFFSET(pos, -width / 2, -height));

      PAL_BattleUIUpdate();
      VIDEO_UpdateScreen(NULL);
    }
  }
  PAL_BattleDelay(1, 0, TRUE);
}

// Show the defensive magic effect for player.
static VOID PAL_BattleShowPlayerDefMagicAnim(WORD wPlayerIndex, WORD wObjectID, SHORT sTarget) {
  int iMagicNum = OBJECT[wObjectID].magic.wMagicNumber;
  MAGIC_T *magic = &MAGIC[iMagicNum];

  int l = PAL_MKFGetDecompressedSize(magic->wEffect, FP_FIRE);
  if (l <= 0)
    return;
  LPSPRITE lpSpriteEffect = (LPSPRITE)UTIL_malloc(l);
  PAL_MKFDecompressChunk((LPBYTE)lpSpriteEffect, l, magic->wEffect, FP_FIRE);

  BATTLE_PLAYER[wPlayerIndex].wCurrentFrame = 6;
  PAL_BattleDelay(1, 0, TRUE);

  DWORD dwTime = PAL_GetTicks();
  for (int i = 0; i < PAL_SpriteGetNumFrames(lpSpriteEffect); i++) {
    g_Battle.lpMagicBitmap = PAL_SpriteGetFrame(lpSpriteEffect, i);

    if (i == 0)
      AUDIO_PlaySound(magic->wSound);

    PAL_DelayUntil(dwTime);
    dwTime = PAL_GetTicks() + (magic->wSpeed + 5) * 10;

    // Magic layers offset
    SHORT sLayerOffset = magic->sLayerOffset;

    // Unlocks the sprite sequence to add some image objects.
    PAL_BattleSpriteAddUnlock();

    if (magic->wType == kMagicTypeApplyToParty) {
      assert(sTarget == -1);
      for (int j = 0; j <= g.wMaxPartyMemberIndex; j++) {
        PAL_POS pos = PAL_XY_OFFSET(BATTLE_PLAYER[j].pos, magic->wXOffset, magic->wYOffset);
        PAL_BattleAddSpriteObject(kBattleSpriteTypeMagic, iMagicNum, pos, sLayerOffset, FALSE);
      }
    } else if (magic->wType == kMagicTypeApplyToPlayer) {
      assert(sTarget != -1);
      PAL_POS pos = PAL_XY_OFFSET(BATTLE_PLAYER[sTarget].pos, magic->wXOffset, magic->wYOffset);
      PAL_BattleAddSpriteObject(kBattleSpriteTypeMagic, iMagicNum, pos, sLayerOffset, FALSE);
    } else {
      assert(FALSE);
    }

    PAL_BattleMakeScene();
    VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);
    PAL_BattleUIUpdate();
    VIDEO_UpdateScreen(NULL);
  }

  free(lpSpriteEffect);

  for (int i = 0; i < 6; i++) {
    if (magic->wType == kMagicTypeApplyToParty) {
      for (int j = 0; j <= g.wMaxPartyMemberIndex; j++)
        BATTLE_PLAYER[j].iColorShift = i;
    } else {
      BATTLE_PLAYER[sTarget].iColorShift = i;
    }
    PAL_BattleDelay(1, 0, TRUE);
  }

  for (int i = 6; i >= 0; i--) {
    if (magic->wType == kMagicTypeApplyToParty) {
      for (int j = 0; j <= g.wMaxPartyMemberIndex; j++)
        BATTLE_PLAYER[j].iColorShift = i;
    } else {
      BATTLE_PLAYER[sTarget].iColorShift = i;
    }
    PAL_BattleDelay(1, 0, TRUE);
  }
}

// Show the offensive magic animation for player.
static VOID PAL_BattleShowPlayerOffMagicAnim(WORD wPlayerIndex, WORD wObjectID, SHORT sTarget, BOOL fSummon) {
  int iMagicNum = OBJECT[wObjectID].magic.wMagicNumber;
  MAGIC_T *magic = &MAGIC[iMagicNum];

  int len = PAL_MKFGetDecompressedSize(magic->wEffect, FP_FIRE);
  if (len <= 0)
    return;
  LPSPRITE lpSpriteEffect = (LPSPRITE)UTIL_malloc(len);
  PAL_MKFDecompressChunk((LPBYTE)lpSpriteEffect, len, magic->wEffect, FP_FIRE);

  int nFrames = PAL_SpriteGetNumFrames(lpSpriteEffect);

  if (wPlayerIndex != (WORD)-1)
    BATTLE_PLAYER[wPlayerIndex].wCurrentFrame = 6;
  PAL_BattleDelay(1, 0, TRUE);

  int n = nFrames;
  n += (nFrames - magic->wFireDelay) * magic->wEffectTimes;
  n += magic->wShake;

  int wave = g.wScreenWave;
  g.wScreenWave += magic->wWave;

  if (!fSummon && magic->wSound != 0)
    AUDIO_PlaySound(magic->wSound);

  DWORD dwTime = PAL_GetTicks();
  for (int i = 0; i < n; i++) {
    // 风吹/击飞效果
    int blow = ((g_Battle.iBlow > 0) ? RandomLong(0, g_Battle.iBlow) : RandomLong(g_Battle.iBlow, 0));
    for (int k = 0; k <= g_Battle.wMaxEnemyIndex; k++)
      if (BATTLE_ENEMY[k].wObjectID != 0)
        BATTLE_ENEMY[k].pos = PAL_XY_OFFSET(BATTLE_ENEMY[k].pos, blow, blow / 2);

    LPCBITMAPRLE *b = &g_Battle.lpMagicBitmap;
    if (i < n - magic->wShake) {
      int k;
      if (i < nFrames) {
        k = i; // 第一次完整播放
      } else {
        // 在 [wFireDelay, nFrames) 区间里循环播放
        k = i - magic->wFireDelay;
        k %= nFrames - magic->wFireDelay;
        k += magic->wFireDelay;
      }
      *b = PAL_SpriteGetFrame(lpSpriteEffect, k);
    } else {
      // 最后震动效果
      VIDEO_ShakeScreen(i, 3);
      int k = (n - magic->wShake - 1) % nFrames; // 震动开始前最后一帧, 假设无 delay
      *b = PAL_SpriteGetFrame(lpSpriteEffect, k);
    }

    PAL_DelayUntil(dwTime);
    dwTime = PAL_GetTicks() + (magic->wSpeed + 5) * 10;

    // Magic layers offset
    SHORT sLayerOffset = magic->sLayerOffset;

    // Unlocks the sprite sequence to add some image objects.
    PAL_BattleSpriteAddUnlock();

    if (magic->wType == kMagicTypeNormal) {
      // 单体攻击
      assert(sTarget != -1);
      PAL_POS pos = PAL_XY_OFFSET(BATTLE_ENEMY[sTarget].pos, magic->wXOffset, magic->wYOffset);
      PAL_BattleAddSpriteObject(kBattleSpriteTypeMagic, iMagicNum, pos, sLayerOffset, FALSE);
      // 场景烧焦痕迹
      if (i == n - 1 && g.wScreenWave < 9 && magic->wKeepEffect == 0xFFFF) {
        PAL_RLEBlitToSurface(*b, g_Battle.lpBackground,
                             PAL_XY_OFFSET(pos, -PAL_RLEGetWidth(*b) / 2, -PAL_RLEGetHeight(*b)));
      }
    } else if (magic->wType == kMagicTypeAttackAll) {
      // 群体攻击
      assert(sTarget == -1);
      const int effectpos[MAX_BATTLE_MAGICSPRITE_ITEMS][2] = {{70, 140}, {100, 110}, {160, 100}};
      for (int k = 0; k < MAX_BATTLE_MAGICSPRITE_ITEMS; k++) {
        PAL_POS pos = PAL_XY(effectpos[k][0], effectpos[k][1]);
        pos = PAL_XY_OFFSET(pos, magic->wXOffset, magic->wYOffset);
        PAL_BattleAddSpriteObject(kBattleSpriteTypeMagic, iMagicNum, pos, sLayerOffset, FALSE);
        // 场景烧焦痕迹
        if (i == n - 1 && g.wScreenWave < 9 && magic->wKeepEffect == 0xFFFF) {
          PAL_RLEBlitToSurface(*b, g_Battle.lpBackground,
                               PAL_XY_OFFSET(pos, -PAL_RLEGetWidth(*b) / 2, -PAL_RLEGetHeight(*b)));
        }
      }
    } else if (magic->wType == kMagicTypeAttackWhole || magic->wType == kMagicTypeAttackField) {
      assert(sTarget == -1);
      PAL_POS pos;
      if (magic->wType == kMagicTypeAttackWhole)
        pos = PAL_XY(120, 100);
      else
        pos = PAL_XY(160, 200);
      pos = PAL_XY_OFFSET(pos, magic->wXOffset, magic->wYOffset);
      PAL_BattleAddSpriteObject(kBattleSpriteTypeMagic, iMagicNum, pos, sLayerOffset, FALSE);
      // 场景烧焦痕迹
      if (i == n - 1 && g.wScreenWave < 9 && magic->wKeepEffect == 0xFFFF) {
        PAL_RLEBlitToSurface(*b, g_Battle.lpBackground,
                             PAL_XY_OFFSET(pos, -PAL_RLEGetWidth(*b) / 2, -PAL_RLEGetHeight(*b)));
      }
    } else {
      assert(FALSE);
    }

    PAL_BattleMakeScene();
    VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);
    PAL_BattleUIUpdate();
    VIDEO_UpdateScreen(NULL);
  }

  g.wScreenWave = wave;
  VIDEO_ShakeScreen(0, 0);

  free(lpSpriteEffect);

  // 恢复敌人坐标
  for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++)
    BATTLE_ENEMY[i].pos = BATTLE_ENEMY[i].posOriginal;
}

// Show the offensive magic animation for enemy.
static VOID PAL_BattleShowEnemyMagicAnim(WORD wEnemyIndex, WORD wObjectID, SHORT sTarget) {
  int iMagicNum = OBJECT[wObjectID].magic.wMagicNumber;
  MAGIC_T *magic = &MAGIC[iMagicNum];

  int len = PAL_MKFGetDecompressedSize(magic->wEffect, FP_FIRE);
  if (len <= 0)
    return;
  LPSPRITE lpSpriteEffect = (LPSPRITE)UTIL_malloc(len);
  PAL_MKFDecompressChunk((LPBYTE)lpSpriteEffect, len, magic->wEffect, FP_FIRE);

  int nFrames = PAL_SpriteGetNumFrames(lpSpriteEffect);

  int n = nFrames;
  n += (nFrames - magic->wFireDelay) * magic->wEffectTimes;
  n += magic->wShake;

  int wave = g.wScreenWave;
  g.wScreenWave += magic->wWave;

  DWORD dwTime = PAL_GetTicks();
  for (int i = 0; i < n; i++) {
    int blow = ((g_Battle.iBlow > 0) ? RandomLong(0, g_Battle.iBlow) : RandomLong(g_Battle.iBlow, 0));
    for (int k = 0; k <= g.wMaxPartyMemberIndex; k++)
      BATTLE_PLAYER[k].pos = PAL_XY_OFFSET(BATTLE_PLAYER[k].pos, blow, blow / 2);

    LPCBITMAPRLE *b = &g_Battle.lpMagicBitmap;
    if (i < n - magic->wShake) {
      int k;
      if (i < nFrames) {
        k = i;
      } else {
        k = i - magic->wFireDelay;
        k %= nFrames - magic->wFireDelay;
        k += magic->wFireDelay;
      }
      *b = PAL_SpriteGetFrame(lpSpriteEffect, k);

      BATTLEENEMY *enemy = &BATTLE_ENEMY[wEnemyIndex];

      if (i == 0 && enemy->e.wMagicSound >= 0)
        AUDIO_PlaySound(magic->wSound);

      // 更新敌人姿势
      if (magic->wFireDelay > 0 && i >= magic->wFireDelay && i < magic->wFireDelay + enemy->e.wAttackFrames) {
        enemy->wCurrentFrame = i - magic->wFireDelay + enemy->e.wIdleFrames + enemy->e.wMagicFrames;
      }
    } else {
      VIDEO_ShakeScreen(i, 3);
      *b = PAL_SpriteGetFrame(lpSpriteEffect, (n - magic->wShake - 1) % nFrames);
    }

    PAL_DelayUntil(dwTime);
    dwTime = PAL_GetTicks() + (magic->wSpeed + 5) * 10;

    // Magic layers offset
    SHORT sLayerOffset = magic->sLayerOffset;

    // Unlocks the sprite sequence to add some image objects.
    PAL_BattleSpriteAddUnlock();

    if (magic->wType == kMagicTypeNormal) {
      assert(sTarget != -1);
      PAL_POS pos = PAL_XY_OFFSET(BATTLE_PLAYER[sTarget].pos, magic->wXOffset, magic->wYOffset);
      PAL_BattleAddSpriteObject(kBattleSpriteTypeMagic, iMagicNum, pos, sLayerOffset, FALSE);

      if (i == n - 1 && g.wScreenWave < 9 && magic->wKeepEffect == 0xFFFF) {
        PAL_RLEBlitToSurface(*b, g_Battle.lpBackground,
                             PAL_XY_OFFSET(pos, -PAL_RLEGetWidth(*b) / 2, -PAL_RLEGetHeight(*b)));
      }
    } else if (magic->wType == kMagicTypeAttackAll) {
      assert(sTarget == -1);
      const int effectpos[MAX_BATTLE_MAGICSPRITE_ITEMS][2] = {{180, 180}, {234, 170}, {270, 146}};
      for (int k = 0; k < MAX_BATTLE_MAGICSPRITE_ITEMS; k++) {
        PAL_POS pos = PAL_XY(effectpos[k][0], effectpos[k][1]);
        pos = PAL_XY_OFFSET(pos, magic->wXOffset, magic->wYOffset);
        PAL_BattleAddSpriteObject(kBattleSpriteTypeMagic, iMagicNum, pos, sLayerOffset, FALSE);

        if (i == n - 1 && g.wScreenWave < 9 && magic->wKeepEffect == 0xFFFF) {
          PAL_RLEBlitToSurface(*b, g_Battle.lpBackground,
                               PAL_XY_OFFSET(pos, -PAL_RLEGetWidth(*b) / 2, -PAL_RLEGetHeight(*b)));
        }
      }
    } else if (magic->wType == kMagicTypeAttackWhole || magic->wType == kMagicTypeAttackField) {
      assert(sTarget == -1);

      PAL_POS pos;
      if (magic->wType == kMagicTypeAttackWhole)
        pos = PAL_XY(240, 150);
      else
        pos = PAL_XY(160, 200);
      pos = PAL_XY_OFFSET(pos, magic->wXOffset, magic->wYOffset);
      PAL_BattleAddSpriteObject(kBattleSpriteTypeMagic, iMagicNum, pos, sLayerOffset, FALSE);

      if (i == n - 1 && g.wScreenWave < 9 && magic->wKeepEffect == 0xFFFF) {
        PAL_RLEBlitToSurface(*b, g_Battle.lpBackground,
                             PAL_XY_OFFSET(pos, -PAL_RLEGetWidth(*b) / 2, -PAL_RLEGetHeight(*b)));
      }
    } else {
      assert(FALSE);
    }

    PAL_BattleMakeScene();
    VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);
    PAL_BattleUIUpdate();
    VIDEO_UpdateScreen(NULL);
  }

  g.wScreenWave = wave;
  VIDEO_ShakeScreen(0, 0);

  free(lpSpriteEffect);

  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++)
    BATTLE_PLAYER[i].pos = BATTLE_PLAYER[i].posOriginal;
}

static VOID PAL_BattleShowPlayerSummonMagicAnim(WORD wObjectID) {
  WORD iMagicNum = OBJECT[wObjectID].magic.wMagicNumber;
  SUMMONGOD_T *magic = (SUMMONGOD_T *)&MAGIC[iMagicNum];

  WORD wEffectMagicID = 0;
  for (wEffectMagicID = 0; wEffectMagicID < MAX_OBJECTS; wEffectMagicID++)
    if (OBJECT[wEffectMagicID].magic.wMagicNumber == magic->wMagicNumber)
      break;
  assert(wEffectMagicID < MAX_OBJECTS);

  // Brighten the players
  for (int i = 1; i <= 10; i++) {
    for (int j = 0; j <= g.wMaxPartyMemberIndex; j++)
      BATTLE_PLAYER[j].iColorShift = i;
    PAL_BattleDelay(1, wObjectID, TRUE);
  }
  VIDEO_BackupScreen(g_Battle.lpSceneBuf);

  // Load the sprite of the summoned god
  int num = magic->wEffect + 10;
  int len = PAL_MKFGetDecompressedSize(num, FP_F);
  g_Battle.lpSummonSprite = UTIL_malloc(len);
  PAL_MKFDecompressChunk(g_Battle.lpSummonSprite, len, num, FP_F);

  g_Battle.iSummonFrame = 0;
  g_Battle.posSummon = PAL_XY(240 + magic->wXOffset, 165 + magic->wYOffset);
  g_Battle.sBackgroundColorShift = magic->sColorShift;
  g_Battle.fSummonColorShift = TRUE;

  // Fade in the summoned god
  PAL_BattleMakeScene();
  PAL_BattleFadeScene();
  g_Battle.fSummonColorShift = FALSE;

  AUDIO_PlaySound(magic->wSound);

  // Show the animation of the summoned god
  int stage = 0;
  int endFrame = magic->wIdleFrames;
  int duration = 0;
  int nFrames = PAL_SpriteGetNumFrames(g_Battle.lpSummonSprite);
  DWORD dwTime = PAL_GetTicks();
  while (g_Battle.iSummonFrame < nFrames) {
    // (两遍)原地蠕动帧, 施法动作帧, 攻击帧
    switch (++stage) {
    case 1:
    case 2:
      duration = FRAME_TIME;
      g_Battle.iSummonFrame = 0;
      break;
    case 3:
      duration *= 2;
      endFrame += magic->wMagicFrames;
      break;
    case 4:
      duration = BATTLE_FRAME_TIME;
      endFrame += magic->wAttackFrames - 1;
      break;
    case 5:
      g_Battle.iSummonFrame--;
      break;
    default:
      nFrames = 0;
      endFrame = 0;
      break;
    }

    while (g_Battle.iSummonFrame < endFrame) {
      PAL_DelayUntil(dwTime);
      dwTime = SDL_GetTicks() + duration;

      PAL_BattleMakeScene();
      VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);
      PAL_BattleUIUpdate();
      VIDEO_UpdateScreen(NULL);

      g_Battle.iSummonFrame++;
    }
  }

  // Show the actual magic effect
  PAL_BattleShowPlayerOffMagicAnim((WORD)-1, wEffectMagicID, -1, TRUE);
}

static VOID PAL_BattleShowPostMagicAnim(VOID) {
  PAL_POS rgEnemyPosBak[MAX_ENEMIES_IN_TEAM];
  for (int i = 0; i < MAX_ENEMIES_IN_TEAM; i++)
    rgEnemyPosBak[i] = BATTLE_ENEMY[i].pos;

  int dist = 8;
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j <= g_Battle.wMaxEnemyIndex; j++) {
      if (BATTLE_ENEMY[j].e.wHealth == BATTLE_ENEMY[j].wPrevHP)
        continue;
      BATTLE_ENEMY[j].pos = PAL_XY_OFFSET(BATTLE_ENEMY[j].pos, -dist, 0);
      BATTLE_ENEMY[j].iColorShift = ((i == 1) ? 6 : 0);
    }
    PAL_BattleDelay(1, 0, TRUE);
    dist /= -2;
  }

  for (int i = 0; i < MAX_ENEMIES_IN_TEAM; i++)
    BATTLE_ENEMY[i].pos = rgEnemyPosBak[i];
  PAL_BattleDelay(1, 0, TRUE);
}

// Validate player's action, fallback to other action when needed.
static VOID PAL_BattlePlayerValidateAction(WORD wPlayerIndex) {
  const WORD wPlayerRole = PARTY_PLAYER(wPlayerIndex);
  BATTLEPLAYER *player = &BATTLE_PLAYER[wPlayerIndex];
  const WORD wObjectID = player->action.wActionID;
  BOOL fValid = TRUE;
  BOOL fToEnemy = FALSE;

  switch (player->action.ActionType) {
  case kBattleActionAttack:
    fToEnemy = TRUE;
    break;

  case kBattleActionPass:
    break;

  case kBattleActionDefend:
    break;

  case kBattleActionMagic: {
    // Make sure player actually has the magic to be used
    int idx;
    for (idx = 0; idx < MAX_PLAYER_MAGICS; idx++)
      if (PLAYER.rgwMagic[idx][wPlayerRole] == wObjectID)
        break; // player has this magic
    if (idx >= MAX_PLAYER_MAGICS)
      fValid = FALSE;

    if (gPlayerStatus[wPlayerRole][kStatusSilence] > 0) // Player is silenced
      fValid = FALSE;

    if (PLAYER.rgwMP[wPlayerRole] < MAGIC[OBJECT[wObjectID].magic.wMagicNumber].wCostMP)
      fValid = FALSE;

    // Fallback to physical attack if player is using an offensive magic,
    // defend if player is using a defensive or healing magic
    if (OBJECT[wObjectID].magic.wFlags & kMagicFlagUsableToEnemy) {
      fToEnemy = TRUE;
      if (!fValid) {
        player->action.ActionType = kBattleActionAttack;
        player->action.wActionID = 0;
      } else if (OBJECT[wObjectID].magic.wFlags & kMagicFlagApplyToAll) {
        player->action.sTarget = -1;
      } else if (player->action.sTarget == -1) {
        player->action.sTarget = PAL_BattleSelectAutoTargetFrom(0);
      }
    } else {
      if (!fValid) {
        player->action.ActionType = kBattleActionDefend;
      } else if (OBJECT[wObjectID].magic.wFlags & kMagicFlagApplyToAll) {
        player->action.sTarget = -1;
      } else if (player->action.sTarget == -1) {
        player->action.sTarget = wPlayerIndex;
      }
    }
    break;
  }

  case kBattleActionCoopMagic:
    fToEnemy = TRUE;
    {
      int iTotalHealthy = 0;
      for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
        if (PAL_IsPlayerHealthy(PARTY_PLAYER(i))) {
          g_Battle.coopContributors[i] = TRUE;
          iTotalHealthy++;
        }
      }
      if (iTotalHealthy <= 1) {
        player->action.ActionType = kBattleActionAttack;
        player->action.wActionID = 0;
      }
    }

    if (player->action.ActionType == kBattleActionCoopMagic) {
      if (OBJECT[wObjectID].magic.wFlags & kMagicFlagApplyToAll) {
        player->action.sTarget = -1;
      } else if (player->action.sTarget == -1) {
        player->action.sTarget = PAL_BattleSelectAutoTargetFrom(0);
      }
    }
    break;

  case kBattleActionFlee:
    break;

  case kBattleActionThrowItem:
    fToEnemy = TRUE;

    if (PAL_GetItemAmount(wObjectID) == 0) {
      player->action.ActionType = kBattleActionAttack;
      player->action.wActionID = 0;
    } else if (OBJECT[wObjectID].item.wFlags & kItemFlagApplyToAll) {
      player->action.sTarget = -1;
    } else if (player->action.sTarget == -1) {
      player->action.sTarget = PAL_BattleSelectAutoTargetFrom(0);
    }
    break;

  case kBattleActionUseItem:
    if (PAL_GetItemAmount(wObjectID) == 0) {
      player->action.ActionType = kBattleActionDefend;
    } else if (OBJECT[wObjectID].item.wFlags & kItemFlagApplyToAll) {
      player->action.sTarget = -1;
    } else if (player->action.sTarget == -1) {
      player->action.sTarget = wPlayerIndex;
    }
    break;

  case kBattleActionAttackMate:
    if (gPlayerStatus[wPlayerRole][kStatusConfused] == 0) {
      // Attack enemies instead if player is not confused
      fToEnemy = TRUE;
      player->action.ActionType = kBattleActionAttack;
      player->action.wActionID = 0;
    } else {
      int idx;
      for (idx = 0; idx <= g.wMaxPartyMemberIndex; idx++)
        if (idx != wPlayerIndex && PLAYER.rgwHP[PARTY_PLAYER(idx)] != 0)
          break;

      if (idx > g.wMaxPartyMemberIndex) {
        // DISABLE Attack enemies if no one else is alive; since original version behaviour is not same
        // fToEnemy = TRUE;
        player->action.ActionType = kBattleActionPass;
        player->action.wActionID = 0;
      }
    }
    break;
  }

  // Check if player can attack all enemies at once, or attack one enemy
  if (player->action.ActionType == kBattleActionAttack) {
    if (player->action.sTarget == -1) {
      if (!PAL_PlayerCanAttackAll(wPlayerRole))
        player->action.sTarget = PAL_BattleSelectAutoTargetFrom(0);
    } else if (PAL_PlayerCanAttackAll(wPlayerRole)) {
      player->action.sTarget = -1;
    }
  }

  // Check if target enemy is dead and switch to another one
  if (fToEnemy && player->action.sTarget >= 0) {
    if (BATTLE_ENEMY[player->action.sTarget].wObjectID == 0) {
      player->action.sTarget = PAL_BattleSelectAutoTargetFrom(player->action.sTarget);
      assert(player->action.sTarget >= 0);
    }
  }
}

// Check if we should enter hiding state after using items or magics.
static VOID PAL_BattleCheckHidingEffect(VOID) {
  if (g_Battle.iHidingTime < 0) {
    g_Battle.iHidingTime = -g_Battle.iHidingTime;
    VIDEO_BackupScreen(g_Battle.lpSceneBuf);
    PAL_BattleMakeScene();
    PAL_BattleFadeScene();
  }
}

INT FIGHT_DetectMagicTargetChange(WORD wMagicNum, INT sTarget) {
  if (sTarget == -1 &&
      (MAGIC[wMagicNum].wType == kMagicTypeNormal || MAGIC[wMagicNum].wType == kMagicTypeApplyToPlayer ||
       MAGIC[wMagicNum].wType == kMagicTypeTrance))
    sTarget = 0;

  if (sTarget != -1 &&
      (MAGIC[wMagicNum].wType == kMagicTypeAttackAll || MAGIC[wMagicNum].wType == kMagicTypeAttackWhole ||
       MAGIC[wMagicNum].wType == kMagicTypeAttackField || MAGIC[wMagicNum].wType == kMagicTypeApplyToParty ||
       MAGIC[wMagicNum].wType == kMagicTypeSummon))
    sTarget = -1;

  return sTarget;
}

// Perform the selected action for a player.
VOID PAL_BattlePlayerPerformAction(WORD wPlayerIndex) {
  WORD wPlayerRole = PARTY_PLAYER(wPlayerIndex);
  BATTLEPLAYER *player = &BATTLE_PLAYER[wPlayerIndex];

  g_Battle.wMovingPlayerIndex = wPlayerIndex;
  g_Battle.iBlow = 0;

  SHORT origTarget = player->action.sTarget;
  PAL_BattlePlayerValidateAction(wPlayerIndex);
  // orig target may be dead, and target is re-selected in above function
  SHORT sTarget = player->action.sTarget;
  PAL_BattleBackupStat();

  // process different action types
  if (player->action.ActionType == kBattleActionAttack && !g_Battle.fThisTurnCoop) {
    if (sTarget != -1) {
      // Attack one enemy
      BATTLEENEMY *enemy = &BATTLE_ENEMY[sTarget];
      for (int t = 0; t < (gPlayerStatus[wPlayerRole][kStatusDualAttack] ? 2 : 1); t++) {
        WORD str = PAL_GetPlayerAttackStrength(wPlayerRole);
        WORD def = enemy->e.wDefense + (enemy->e.wLevel + 6) * 4;
        WORD res = enemy->e.wPhysicalResistance;
        BOOL fCritical = FALSE;

        SHORT sDamage = PAL_CalcPhysicalAttackDamage(str, def, res);
        sDamage += RandomLong(1, 2);

        if (RandomLong(0, 5) == 0 || gPlayerStatus[wPlayerRole][kStatusBravery] > 0) {
          // Critical Hit
          sDamage *= 3;
          fCritical = TRUE;
        }

        if (wPlayerRole == 0 && RandomLong(0, 11) == 0) {
          // Bonus hit for Li Xiaoyao
          sDamage *= 2;
          fCritical = TRUE;
        }

        sDamage = (SHORT)(sDamage * RandomFloat(1, 1.125));

        if (sDamage <= 0)
          sDamage = 1;

        enemy->e.wHealth -= sDamage;

        if (t == 0) {
          player->wCurrentFrame = 7;
          PAL_BattleDelay(4, 0, TRUE);
        }

        PAL_BattleShowPlayerAttackAnim(wPlayerIndex, fCritical);
      }
    } else {
      // Attack all enemies
      for (int t = 0; t < (gPlayerStatus[wPlayerRole][kStatusDualAttack] ? 2 : 1); t++) {
        int division = 1;
        const int index[MAX_ENEMIES_IN_TEAM] = {2, 1, 0, 4, 3};
        int x = 1;

        BOOL fCritical = (RandomLong(0, 5) == 0 || gPlayerStatus[wPlayerRole][kStatusBravery] > 0);

        if (t == 0) {
          player->wCurrentFrame = 7;
          PAL_BattleDelay(4, 0, TRUE);
        }

        for (int i = 0; i < MAX_ENEMIES_IN_TEAM; i++) {
          if (index[i] > g_Battle.wMaxEnemyIndex)
            continue;

          BATTLEENEMY *enemy = &BATTLE_ENEMY[index[i]];
          if (enemy->wObjectID == 0)
            continue;

          WORD str = PAL_GetPlayerAttackStrength(wPlayerRole);
          WORD def = enemy->e.wDefense + (enemy->e.wLevel + 6) * 4;
          WORD res = enemy->e.wPhysicalResistance;

          FLOAT sDamage = PAL_CalcPhysicalAttackDamage(str, def, res);

          if (fCritical)
            sDamage *= 3;

          sDamage /= division;

          if (sDamage <= 0)
            sDamage = 1;

          enemy->e.wHealth -= sDamage;

          division *= 2;
        }

        if (t > 0) {
          if (x == 1) {
            player->fSecondAttack = TRUE;
            x--;
          } else {
            player->fSecondAttack = FALSE;
          }
        }

        PAL_BattleShowPlayerAttackAnim(wPlayerIndex, fCritical);
        PAL_BattleDelay(4, 0, TRUE);
      }
    }

    player->fSecondAttack = FALSE;
    PAL_BattleUpdateFighters();
    PAL_BattleMakeScene();
    PAL_BattleDelay(3, 0, TRUE);

    g.Exp.rgAttackExp[wPlayerRole].wCount++;
    g.Exp.rgHealthExp[wPlayerRole].wCount += RandomLong(2, 3);

  } else if (player->action.ActionType == kBattleActionAttackMate && !g_Battle.fThisTurnCoop) {
    // Check if there is someone else who is alive
    int idx;
    for (idx = 0; idx <= g.wMaxPartyMemberIndex; idx++)
      if (idx != wPlayerIndex && PLAYER.rgwHP[PARTY_PLAYER(idx)] > 0)
        break;

    if (idx <= g.wMaxPartyMemberIndex) {
      // Pick a target randomly
      do {
        sTarget = RandomLong(0, g.wMaxPartyMemberIndex);
      } while (sTarget == wPlayerIndex || PLAYER.rgwHP[PARTY_PLAYER(sTarget)] == 0);

      WORD targetRole = PARTY_PLAYER(sTarget);
      BATTLEPLAYER *target = &BATTLE_PLAYER[sTarget];

      for (int j = 0; j < 2; j++) {
        player->wCurrentFrame = 8;
        PAL_BattleDelay(1, 0, TRUE);
        player->wCurrentFrame = 0;
        PAL_BattleDelay(1, 0, TRUE);
      }
      PAL_BattleDelay(2, 0, TRUE);

      player->pos = PAL_XY_OFFSET(target->pos, 30, 12);
      player->wCurrentFrame = 8;
      PAL_BattleDelay(5, 0, TRUE);

      player->wCurrentFrame = 9;
      AUDIO_PlaySound(PLAYER.rgwWeaponSound[wPlayerRole]);

      WORD str = PAL_GetPlayerAttackStrength(wPlayerRole);
      WORD def = PAL_GetPlayerDefense(targetRole);
      if (target->fDefending)
        def *= 2;

      SHORT sDamage = PAL_CalcPhysicalAttackDamage(str, def, 2);

      if (gPlayerStatus[targetRole][kStatusProtect] > 0)
        sDamage /= 2;

      if (sDamage <= 0)
        sDamage = 1;

      if (sDamage > (SHORT)PLAYER.rgwHP[targetRole])
        sDamage = PLAYER.rgwHP[targetRole];

      PLAYER.rgwHP[targetRole] -= sDamage;

      target->pos = PAL_XY_OFFSET(target->pos, -12, -6);
      PAL_BattleDelay(1, 0, TRUE);

      target->iColorShift = 6;
      PAL_BattleDelay(1, 0, TRUE);

      PAL_BattleDisplayStatChange();

      target->iColorShift = 0;
      PAL_BattleDelay(4, 0, TRUE);

      PAL_BattleUpdateFighters();
      PAL_BattleDelay(4, 0, TRUE);
    }

  } else if (player->action.ActionType == kBattleActionCoopMagic) {
    g_Battle.fThisTurnCoop = TRUE;

    WORD rgwCoopPos[3][2] = {{208, 157}, {234, 170}, {260, 183}};

    WORD wObject = PAL_GetPlayerCooperativeMagic(PARTY_PLAYER(wPlayerIndex));
    WORD wMagicNum = OBJECT[wObject].magic.wMagicNumber;

    sTarget = FIGHT_DetectMagicTargetChange(wMagicNum, sTarget);

    if (MAGIC[wMagicNum].wType == kMagicTypeSummon) {
      PAL_BattleShowPlayerPreMagicAnim(wPlayerIndex, TRUE);
      PAL_BattleShowPlayerSummonMagicAnim(wObject);
    } else {
      // Sound should be played before action begins
      AUDIO_PlaySound(29);

      for (int i = 1; i <= 6; i++) {
        // Update the position for the player who invoked the action
        int x = (PAL_X(player->posOriginal) * (6 - i) + rgwCoopPos[0][0] * i) / 6;
        int y = (PAL_Y(player->posOriginal) * (6 - i) + rgwCoopPos[0][1] * i) / 6;
        player->pos = PAL_XY(x, y);

        // Update the position for other players
        for (int j = 0, t = 0; j <= g.wMaxPartyMemberIndex; j++) {
          if ((WORD)j == wPlayerIndex)
            continue;
          if (g_Battle.coopContributors[j] == FALSE)
            continue;
          t++;
          int x = (PAL_X(BATTLE_PLAYER[j].posOriginal) * (6 - i) + rgwCoopPos[t][0] * i) / 6;
          int y = (PAL_Y(BATTLE_PLAYER[j].posOriginal) * (6 - i) + rgwCoopPos[t][1] * i) / 6;
          BATTLE_PLAYER[j].pos = PAL_XY(x, y);
        }

        PAL_BattleDelay(1, 0, TRUE);
      }

      for (int i = g.wMaxPartyMemberIndex; i >= 0; i--) {
        if ((WORD)i == wPlayerIndex)
          continue;
        if (g_Battle.coopContributors[i] == FALSE)
          continue;
        BATTLE_PLAYER[i].wCurrentFrame = 5;
        PAL_BattleDelay(3, 0, TRUE);
      }

      player->iColorShift = 6;
      player->wCurrentFrame = 5;
      PAL_BattleDelay(5, 0, TRUE);

      player->wCurrentFrame = 6;
      player->iColorShift = 0;
      PAL_BattleDelay(3, 0, TRUE);

      PAL_BattleShowPlayerOffMagicAnim((WORD)-1, wObject, sTarget, FALSE);
    }

    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
      if (g_Battle.coopContributors[i] == FALSE)
        continue;

      PLAYER.rgwHP[PARTY_PLAYER(i)] -= MAGIC[wMagicNum].wCostMP;
      if ((SHORT)(PLAYER.rgwHP[PARTY_PLAYER(i)]) <= 0)
        PLAYER.rgwHP[PARTY_PLAYER(i)] = 1;

      // Reset the time meter for everyone when using coopmagic
      BATTLE_PLAYER[i].state = kFighterWait;
    }

    PAL_BattleBackupStat(); // so that "damages" to players won't be shown

    WORD str = 0;
    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
      if (g_Battle.coopContributors[i] == FALSE)
        continue;
      str += PAL_GetPlayerAttackStrength(PARTY_PLAYER(i));
      str += PAL_GetPlayerMagicStrength(PARTY_PLAYER(i));
    }
    str /= 4;

    // Inflict damage to enemies
    for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
      BATTLEENEMY *enemy = &BATTLE_ENEMY[i];
      if (enemy->wObjectID == 0)
        continue;
      if (sTarget != -1 && sTarget != i)
        continue;

      WORD def = enemy->e.wDefense + (enemy->e.wLevel + 6) * 4;
      SHORT sDamage = PAL_CalcMagicDamage(str, def, enemy->e.wElemResistance, enemy->e.wPoisonResistance, 1, wObject);

      if (sDamage <= 0)
        sDamage = 1;
      enemy->e.wHealth -= sDamage;
    }

    PAL_BattleDisplayStatChange();
    PAL_BattleShowPostMagicAnim();
    PAL_BattleDelay(5, 0, TRUE);

    if (MAGIC[wMagicNum].wType != kMagicTypeSummon) {
      PAL_BattlePostActionCheck(FALSE);

      // Move all players back to the original position
      for (int i = 1; i <= 6; i++) {
        // Update the position for the player who invoked the action
        int x = (PAL_X(player->posOriginal) * i + rgwCoopPos[0][0] * (6 - i)) / 6;
        int y = (PAL_Y(player->posOriginal) * i + rgwCoopPos[0][1] * (6 - i)) / 6;
        player->pos = PAL_XY(x, y);

        // Update the position for other players
        for (int j = 0, t = 0; j <= g.wMaxPartyMemberIndex; j++) {
          if (g_Battle.coopContributors[j] == FALSE)
            continue;
          BATTLE_PLAYER[j].wCurrentFrame = 0;
          if ((WORD)j == wPlayerIndex)
            continue;
          t++;
          x = (PAL_X(BATTLE_PLAYER[j].posOriginal) * i + rgwCoopPos[t][0] * (6 - i)) / 6;
          y = (PAL_Y(BATTLE_PLAYER[j].posOriginal) * i + rgwCoopPos[t][1] * (6 - i)) / 6;
          BATTLE_PLAYER[j].pos = PAL_XY(x, y);
        }

        PAL_BattleDelay(1, 0, TRUE);
      }
    }

  } else if (player->action.ActionType == kBattleActionDefend && !g_Battle.fThisTurnCoop) {
    player->fDefending = TRUE;
    g.Exp.rgDefenseExp[wPlayerRole].wCount += 2;

  } else if (player->action.ActionType == kBattleActionFlee && !g_Battle.fThisTurnCoop) {
    WORD str = PAL_GetPlayerFleeRate(wPlayerRole);

    WORD dex = 0;
    for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++)
      if (BATTLE_ENEMY[i].wObjectID != 0)
        dex += BATTLE_ENEMY[i].e.wDexterity + (BATTLE_ENEMY[i].e.wLevel + 6) * 4;

    if (str >= RandomLong(0, dex) && !g_Battle.fIsBoss) {
      // Successful escape
      PAL_BattlePlayerEscape();
    } else {
      // Failed escape
      player->wCurrentFrame = 0;
      for (int i = 0; i < 3; i++) {
        player->pos = PAL_XY_OFFSET(player->pos, 4, 2);
        PAL_BattleDelay(1, 0, TRUE);
      }
      player->wCurrentFrame = 1;
      PAL_BattleDelay(8, BATTLE_LABEL_ESCAPEFAIL, TRUE);
      g.Exp.rgFleeExp[wPlayerRole].wCount += 2;
    }

  } else if (player->action.ActionType == kBattleActionMagic && !g_Battle.fThisTurnCoop) {
    WORD wObject = player->action.wActionID;
    WORD wMagicNum = OBJECT[wObject].magic.wMagicNumber;
    MAGIC_T *magic = &MAGIC[wMagicNum];

    sTarget = FIGHT_DetectMagicTargetChange(wMagicNum, sTarget);

    PAL_BattleShowPlayerPreMagicAnim(wPlayerIndex, (magic->wType == kMagicTypeSummon));

    if (!gAutoBattle) {
      PLAYER.rgwMP[wPlayerRole] -= magic->wCostMP;
      if ((SHORT)(PLAYER.rgwMP[wPlayerRole]) < 0)
        PLAYER.rgwMP[wPlayerRole] = 0;
    }

    if (magic->wType == kMagicTypeApplyToPlayer || magic->wType == kMagicTypeApplyToParty ||
        magic->wType == kMagicTypeTrance) {
      // Using a defensive magic
      WORD w = 0;
      if (magic->wType == kMagicTypeTrance)
        w = wPlayerRole;
      else if (sTarget != -1)
        w = PARTY_PLAYER(sTarget);

      OBJECT[wObject].magic.wScriptOnUse = PAL_RunTriggerScript(OBJECT[wObject].magic.wScriptOnUse, wPlayerRole);

      if (g_fScriptSuccess) {
        PAL_BattleShowPlayerDefMagicAnim(wPlayerIndex, wObject, sTarget);

        OBJECT[wObject].magic.wScriptOnSuccess = PAL_RunTriggerScript(OBJECT[wObject].magic.wScriptOnSuccess, w);

        if (g_fScriptSuccess) {
          if (magic->wType == kMagicTypeTrance) {
            for (int i = 0; i < 6; i++) {
              player->iColorShift = i * 2;
              PAL_BattleDelay(1, 0, TRUE);
            }

            VIDEO_BackupScreen(g_Battle.lpSceneBuf);
            PAL_LoadBattleSprites();

            player->iColorShift = 0;

            PAL_BattleMakeScene();
            PAL_BattleFadeScene();
          }
        }
      }
    } else {
      // Using an offensive magic
      OBJECT[wObject].magic.wScriptOnUse = PAL_RunTriggerScript(OBJECT[wObject].magic.wScriptOnUse, wPlayerRole);

      if (g_fScriptSuccess) {
        if (magic->wType == kMagicTypeSummon) {
          PAL_BattleShowPlayerSummonMagicAnim(wObject);
        } else {
          PAL_BattleShowPlayerOffMagicAnim(wPlayerIndex, wObject, sTarget, FALSE);
        }

        OBJECT[wObject].magic.wScriptOnSuccess =
            PAL_RunTriggerScript(OBJECT[wObject].magic.wScriptOnSuccess, (WORD)sTarget);

        // Inflict damage to enemies
        if (magic->wBaseDamage > 0) {
          for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
            BATTLEENEMY *enemy = &BATTLE_ENEMY[i];
            if (enemy->wObjectID == 0)
              continue;
            if (sTarget != -1 && sTarget != i)
              continue;

            WORD str = PAL_GetPlayerMagicStrength(wPlayerRole);
            WORD def = enemy->e.wDefense + (enemy->e.wLevel + 6) * 4;
            SHORT sDamage =
                PAL_CalcMagicDamage(str, def, enemy->e.wElemResistance, enemy->e.wPoisonResistance, 1, wObject);

            if (sDamage <= 0)
              sDamage = 1;
            enemy->e.wHealth -= sDamage;
          }
        }
      }
    }

    PAL_BattleDisplayStatChange();
    PAL_BattleShowPostMagicAnim();
    PAL_BattleDelay(5, 0, TRUE);

    PAL_BattleCheckHidingEffect();

    g.Exp.rgMagicExp[wPlayerRole].wCount += RandomLong(2, 3);
    g.Exp.rgMagicPowerExp[wPlayerRole].wCount++;

  } else if (player->action.ActionType == kBattleActionThrowItem && !g_Battle.fThisTurnCoop) {
    WORD wObject = player->action.wActionID;

    for (int i = 0; i < 4; i++) {
      player->pos = PAL_XY_OFFSET(player->pos, -(4 - i), -(4 - i) / 2);
      PAL_BattleDelay(1, 0, TRUE);
    }

    PAL_BattleDelay(2, wObject, TRUE);

    player->wCurrentFrame = 5;
    AUDIO_PlaySound(PLAYER.rgwMagicSound[wPlayerRole]);

    PAL_BattleDelay(8, wObject, TRUE);

    player->wCurrentFrame = 6;
    PAL_BattleDelay(2, wObject, TRUE);

    // Run the script
    OBJECT[wObject].item.wScriptOnThrow = PAL_RunTriggerScript(OBJECT[wObject].item.wScriptOnThrow, (WORD)sTarget);

    // Remove the thrown item from inventory
    PAL_AddItemToInventory(wObject, -1);

    PAL_BattleDisplayStatChange();
    PAL_BattleDelay(4, 0, TRUE);
    PAL_BattleUpdateFighters();
    PAL_BattleDelay(4, 0, TRUE);

    PAL_BattleCheckHidingEffect();

  } else if (player->action.ActionType == kBattleActionUseItem && !g_Battle.fThisTurnCoop) {
    WORD wObject = player->action.wActionID;

    PAL_BattleShowPlayerUseItemAnim(wPlayerIndex, wObject, sTarget);

    // Run the script
    OBJECT[wObject].item.wScriptOnUse =
        PAL_RunTriggerScript(OBJECT[wObject].item.wScriptOnUse, (sTarget == -1) ? 0xFFFF : PARTY_PLAYER(sTarget));

    // Remove the item if the item is consuming
    if (OBJECT[wObject].item.wFlags & kItemFlagConsuming) {
      PAL_AddItemToInventory(wObject, -1);
    }

    PAL_BattleCheckHidingEffect();

    PAL_BattleUpdateFighters();
    PAL_BattleDisplayStatChange();
    PAL_BattleDelay(8, 0, TRUE);

  } else if (player->action.ActionType == kBattleActionPass) {
  }

  // Revert this player back to waiting state.
  player->state = kFighterWait;

  PAL_BattlePostActionCheck(FALSE);

  // Revert target slot of this player
  player->action.sTarget = origTarget;
}

static INT PAL_BattleEnemySelectEnemyTargetIndex(VOID) {
  int i = RandomLong(0, g_Battle.wMaxEnemyIndex);
  while (BATTLE_ENEMY[i].wObjectID == 0 || BATTLE_ENEMY[i].e.wHealth == 0)
    i = RandomLong(0, g_Battle.wMaxEnemyIndex);
  return i;
}

static INT PAL_BattleEnemySelectTargetIndex(VOID) {
  int i = RandomLong(0, g.wMaxPartyMemberIndex);
  while (PLAYER.rgwHP[PARTY_PLAYER(i)] == 0)
    i = RandomLong(0, g.wMaxPartyMemberIndex);
  return i;
}

VOID PAL_BattleEnemyPerformAction(WORD wEnemyIndex) {
  PAL_BattleBackupStat();
  g_Battle.iBlow = 0;

  SHORT sTarget = PAL_BattleEnemySelectTargetIndex();
  WORD wPlayerRole = PARTY_PLAYER(sTarget);
  BATTLEENEMY *enemy = &BATTLE_ENEMY[wEnemyIndex];
  WORD wMagic = enemy->e.wMagic;

  // 如果敌人睡眠、定身, 或者我方使用了隐蛊
  if (enemy->rgwStatus[kStatusSleep] > 0 || enemy->rgwStatus[kStatusParalyzed] > 0 || g_Battle.iHidingTime > 0)
    return; // Do nothing

  if (enemy->rgwStatus[kStatusConfused] > 0) {
    // 敌人混乱攻击敌人
    INT iTarget = PAL_BattleEnemySelectEnemyTargetIndex();
    if (iTarget == wEnemyIndex)
      return; // Do nothing

    BATTLEENEMY *target = &BATTLE_ENEMY[iTarget];

    // 攻击者移动到目标位置的中间
    for (int i = 0; i < 3; i++) {
      int x = (PAL_X(enemy->pos) + PAL_X(target->pos)) / 2;
      int y = (PAL_Y(enemy->pos) + PAL_Y(target->pos)) / 2;
      enemy->pos = PAL_XY(x, y);
      PAL_BattleDelay(1, 0, TRUE);
    }

    // 播放打击特效
    DWORD dwTime = PAL_GetTicks() + BATTLE_FRAME_TIME;
    int x = (PAL_X(enemy->pos) + PAL_X(target->pos)) / 2;
    int y = PAL_Y(target->pos) - PAL_RLEGetHeight(PAL_SpriteGetFrame(target->lpSprite, 0)) / 3 + 10;
    for (int i = 9; i < 12; i++) {
      LPCBITMAPRLE b = PAL_SpriteGetFrame(g_Battle.lpEffectSprite, i);

      PAL_DelayUntil(dwTime);
      dwTime = PAL_GetTicks() + BATTLE_FRAME_TIME;

      PAL_BattleMakeScene();
      VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);
      PAL_RLEBlitToSurface(b, gpScreen, PAL_XY(x - PAL_RLEGetWidth(b) / 2, y - PAL_RLEGetHeight(b)));
      PAL_BattleUIUpdate();
      VIDEO_UpdateScreen(NULL);
    }

    // 伤害计算
    int str = enemy->e.wAttackStrength + (enemy->e.wLevel + 6) * 6;
    int def = target->e.wDefense + (target->e.wLevel + 6) * 4;
    SHORT sDamage = PAL_CalcBaseDamage(str, def) * 2 / target->e.wPhysicalResistance;

    if (sDamage <= 0)
      sDamage = 1;

    target->e.wHealth -= sDamage;

    // 显示伤害数字并归位
    PAL_BattleDisplayStatChange();
    PAL_BattleShowPostMagicAnim();
    PAL_BattleDelay(5, 0, TRUE);

    enemy->pos = enemy->posOriginal;
    PAL_BattleDelay(2, 0, TRUE);

    PAL_BattlePostActionCheck(FALSE);

  } else if (wMagic != 0 && RandomLong(0, 9) < enemy->e.wMagicRate && enemy->rgwStatus[kStatusSilence] == 0) {
    // Magical attack

    if (wMagic == 0xFFFF)
      return; // Do nothing

    WORD wMagicNum = OBJECT[wMagic].magic.wMagicNumber;

    int str = enemy->e.wMagicStrength + (enemy->e.wLevel + 6) * 6;
    if (str < 0) // 绿叶妖精
      str = 0;

    enemy->pos = PAL_XY_OFFSET(enemy->pos, 12, 6);
    PAL_BattleDelay(1, 0, FALSE);
    enemy->pos = PAL_XY_OFFSET(enemy->pos, 4, 2);
    PAL_BattleDelay(1, 0, FALSE);

    AUDIO_PlaySound(enemy->e.wMagicSound);

    for (int i = 0; i < enemy->e.wMagicFrames; i++) {
      enemy->wCurrentFrame = enemy->e.wIdleFrames + i;
      PAL_BattleDelay(enemy->e.wActWaitFrames, 0, FALSE);
    }

    if (enemy->e.wMagicFrames == 0)
      PAL_BattleDelay(1, 0, FALSE);

    if (MAGIC[wMagicNum].wFireDelay == 0) {
      for (int i = 0; i <= enemy->e.wAttackFrames; i++) {
        enemy->wCurrentFrame = i - 1 + enemy->e.wIdleFrames + enemy->e.wMagicFrames;
        PAL_BattleDelay(enemy->e.wActWaitFrames, 0, FALSE);
      }
    }

    // 是否随机防御
    BOOL rgfMagAutoDefend[MAX_PLAYERS_IN_PARTY] = {FALSE};

    if (MAGIC[wMagicNum].wType != kMagicTypeNormal) {
      sTarget = -1;

      for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
        if (PAL_CanPlayerAct(PARTY_PLAYER(i), TRUE) && RandomLong(0, 2) == 0) {
          rgfMagAutoDefend[i] = TRUE;
          BATTLE_PLAYER[i].wCurrentFrame = 3;
        }
      }
    } else if (PAL_CanPlayerAct(wPlayerRole, TRUE) && RandomLong(0, 2) == 0) {
      rgfMagAutoDefend[sTarget] = TRUE;
      BATTLE_PLAYER[sTarget].wCurrentFrame = 3;
    }

    OBJECT[wMagic].magic.wScriptOnUse = PAL_RunTriggerScript(OBJECT[wMagic].magic.wScriptOnUse, wPlayerRole);

    if (g_fScriptSuccess) {
      // 播放法术的具体动画
      PAL_BattleShowEnemyMagicAnim(wEnemyIndex, wMagic, sTarget);

      OBJECT[wMagic].magic.wScriptOnSuccess = PAL_RunTriggerScript(OBJECT[wMagic].magic.wScriptOnSuccess, wPlayerRole);
    }

    // 伤害计算
    if (MAGIC[wMagicNum].wBaseDamage > 0) {
      for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
        WORD w = PARTY_PLAYER(i);
        if (PLAYER.rgwHP[w] == 0)
          continue; // skip dead players

        if (sTarget != -1 && sTarget != i)
          continue;

        int def = PAL_GetPlayerDefense(w);

        // 计算元素抗性
        WORD rgwElementalResistance[NUM_MAGIC_ELEMENTAL];
        for (int x = 0; x < NUM_MAGIC_ELEMENTAL; x++)
          rgwElementalResistance[x] = 100 + PAL_GetPlayerElementalResistance(w, x);

        SHORT sDamage =
            PAL_CalcMagicDamage(str, def, rgwElementalResistance, 100 + PAL_GetPlayerPoisonResistance(w), 20, wMagic);

        sDamage /= ((BATTLE_PLAYER[i].fDefending ? 2 : 1) * ((gPlayerStatus[w][kStatusProtect] > 0) ? 2 : 1)) +
                   (rgfMagAutoDefend[i] ? 1 : 0);

        if (sDamage > PLAYER.rgwHP[w])
          sDamage = PLAYER.rgwHP[w];
        PLAYER.rgwHP[w] -= sDamage;
        if (PLAYER.rgwHP[w] == 0)
          AUDIO_PlaySound(PLAYER.rgwDeathSound[w]);
      }
    }

    if (!gAutoBattle)
      PAL_BattleDisplayStatChange();

    // 我方角色身体闪烁变色震动
    for (int i = 0; i < 5; i++) {
      for (int x = 0; x <= g.wMaxPartyMemberIndex; x++) {
        if (BATTLE_PLAYER[x].wPrevHP == PLAYER.rgwHP[PARTY_PLAYER(x)])
          continue; // Skip unaffected players

        if (sTarget != -1 && sTarget != x)
          continue;

        BATTLE_PLAYER[x].wCurrentFrame = 4;
        if (i > 0)
          BATTLE_PLAYER[x].pos = PAL_XY_OFFSET(BATTLE_PLAYER[x].pos, (8 >> i), (4 >> i));
        BATTLE_PLAYER[x].iColorShift = ((i < 3) ? 6 : 0);
      }
      PAL_BattleDelay(1, 0, FALSE);
    }

    enemy->wCurrentFrame = 0;
    enemy->pos = enemy->posOriginal;

    PAL_BattleDelay(1, 0, FALSE);
    PAL_BattleUpdateFighters();

    PAL_BattlePostActionCheck(TRUE);
    PAL_BattleDelay(8, 0, TRUE);

  } else {
    // Physical attack
    BATTLEPLAYER *target = &BATTLE_PLAYER[sTarget];

    WORD wFrameBak = target->wCurrentFrame;

    if (g_Battle.ActionQueue[g_Battle.iCurAction].fIsSecond && enemy->e.wMagic == 0)
      AUDIO_PlaySound(enemy->e.wMagicSound);
    else
      AUDIO_PlaySound(enemy->e.wAttackSound);

    int iCoverIndex = -1;

    BOOL fAutoDefend = (RandomLong(0, 16) >= 10); // 完全闪避

    // 如果目标处于濒死、混乱、睡眠或定身状态，且触发了随机保护机率
    if ((PAL_IsPlayerDying(wPlayerRole) || PAL_isPlayerBadStatus(wPlayerRole)) && fAutoDefend) {
      WORD w = PLAYER.rgwCoveredBy[wPlayerRole];
      for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
        if (PARTY_PLAYER(i) == w) {
          // 检查援护者是否健在且状态正常
          if (!PAL_IsPlayerDying(w) && !PAL_isPlayerBadStatus(w))
            iCoverIndex = i;
          break;
        }
      }
    }

    // If no one can cover the inflictor and inflictor is in a bad status, don't evade
    if (iCoverIndex == -1 && PAL_isPlayerBadStatus(wPlayerRole))
      fAutoDefend = FALSE;

    for (int i = 0; i < enemy->e.wMagicFrames; i++) {
      enemy->wCurrentFrame = enemy->e.wIdleFrames + i;
      PAL_BattleDelay(2, 0, FALSE);
    }

    for (int i = 0; i < 3 - enemy->e.wMagicFrames; i++) {
      enemy->pos = PAL_XY_OFFSET(enemy->pos, -2, -1);
      PAL_BattleDelay(1, 0, FALSE);
    }

    if (enemy->e.wActionSound != 0)
      AUDIO_PlaySound(enemy->e.wActionSound);
    PAL_BattleDelay(1, 0, FALSE);

    int iSound = enemy->e.wCallSound;

    if (iCoverIndex != -1) {
      // 瞬移阻挡
      iSound = PLAYER.rgwCoverSound[PARTY_PLAYER(iCoverIndex)];
      BATTLE_PLAYER[iCoverIndex].wCurrentFrame = 3;
      BATTLE_PLAYER[iCoverIndex].pos = PAL_XY_OFFSET(target->pos, -24, -12);
    } else if (fAutoDefend) {
      target->wCurrentFrame = 3;
      iSound = PLAYER.rgwCoverSound[wPlayerRole];
    }

    if (enemy->e.wAttackFrames == 0) {
      enemy->wCurrentFrame = enemy->e.wIdleFrames - 1;
      enemy->pos = PAL_XY_OFFSET(target->pos, -44, -16);
      PAL_BattleDelay(2, 0, FALSE);
    } else {
      for (int i = 0; i <= enemy->e.wAttackFrames; i++) {
        enemy->wCurrentFrame = enemy->e.wIdleFrames + enemy->e.wMagicFrames + i - 1;
        enemy->pos = PAL_XY_OFFSET(target->pos, -44, -16);
        PAL_BattleDelay(enemy->e.wActWaitFrames, 0, FALSE);
      }
    }

    if (!fAutoDefend) {
      target->wCurrentFrame = 4;

      int str = enemy->e.wAttackStrength + (enemy->e.wLevel + 6) * 6;

      int def = PAL_GetPlayerDefense(wPlayerRole);
      if (target->fDefending)
        def *= 2;

      SHORT sDamage = PAL_CalcPhysicalAttackDamage(str + RandomLong(0, 2), def, 2);
      sDamage += RandomLong(0, 1);

      if (gPlayerStatus[wPlayerRole][kStatusProtect])
        sDamage /= 2;

      if ((SHORT)PLAYER.rgwHP[wPlayerRole] < sDamage)
        sDamage = PLAYER.rgwHP[wPlayerRole];

      if (sDamage <= 0)
        sDamage = 1;

      PLAYER.rgwHP[wPlayerRole] -= sDamage;

      PAL_BattleDisplayStatChange();

      target->iColorShift = 6;
    }

    if (iSound != 0)
      AUDIO_PlaySound(iSound);

    PAL_BattleDelay(1, 0, FALSE);

    target->iColorShift = 0;

    if (iCoverIndex != -1) {
      enemy->pos = PAL_XY_OFFSET(enemy->pos, -10, -8);
      BATTLE_PLAYER[iCoverIndex].pos = PAL_XY_OFFSET(BATTLE_PLAYER[iCoverIndex].pos, 4, 2);
    } else {
      target->pos = PAL_XY_OFFSET(target->pos, 8, 4);
    }

    PAL_BattleDelay(1, 0, FALSE);

    if (PLAYER.rgwHP[wPlayerRole] == 0) {
      AUDIO_PlaySound(PLAYER.rgwDeathSound[wPlayerRole]);
      wFrameBak = 2; // 尸体
    } else if (PAL_IsPlayerDying(wPlayerRole)) {
      wFrameBak = 1; // 跪地
    }

    if (iCoverIndex == -1) {
      target->pos = PAL_XY_OFFSET(target->pos, 2, 1);
    }

    PAL_BattleDelay(3, 0, FALSE);

    enemy->pos = enemy->posOriginal;
    enemy->wCurrentFrame = 0;

    PAL_BattleDelay(1, 0, FALSE);

    target->wCurrentFrame = wFrameBak;
    PAL_BattleDelay(1, 0, TRUE);

    PAL_BattleDelay(4, 0, TRUE);

    PAL_BattleUpdateFighters();

    // 运行怪物自带毒攻击的脚本
    if (iCoverIndex == -1 && !fAutoDefend && enemy->e.wAttackEquivItemRate >= RandomLong(1, 10) &&
        PAL_GetPlayerPoisonResistance(wPlayerRole) < RandomLong(1, 100)) {
      int weapon = enemy->e.wAttackEquivItem;
      OBJECT[weapon].item.wScriptOnUse = PAL_RunTriggerScript(OBJECT[weapon].item.wScriptOnUse, wPlayerRole);
    }

    PAL_BattlePostActionCheck(TRUE);
  }
}

VOID PAL_BattleStealFromEnemy(WORD wTarget, WORD wStealRate) {
  int iPlayerIndex = g_Battle.wMovingPlayerIndex;
  BATTLEPLAYER *player = &BATTLE_PLAYER[iPlayerIndex];
  player->wCurrentFrame = 10;

  BATTLEENEMY *target = &BATTLE_ENEMY[wTarget];

  int offset = ((INT)wTarget - iPlayerIndex) * 8;
  PAL_POS pos = PAL_XY_OFFSET(target->pos, 64 - offset, 22 + offset);
  player->pos = pos;
  PAL_BattleDelay(1, 0, TRUE);

  for (int i = 0; i < 5; i++) {
    pos = PAL_XY_OFFSET(pos, -(i + 8), -4);
    player->pos = pos;

    if (i == 4)
      target->iColorShift = 6;

    PAL_BattleDelay(1, 0, TRUE);
  }

  target->iColorShift = 0;
  player->pos = PAL_XY_OFFSET(pos, -1, 0);
  PAL_BattleDelay(3, 0, TRUE);

  player->state = kFighterWait;
  PAL_BattleUpdateFighters();
  PAL_BattleDelay(1, 0, TRUE);

  WCHAR s[256] = L"";
  if (target->e.nStealItem > 0 && (RandomLong(0, 10) <= wStealRate || wStealRate == 0)) {
    if (target->e.wStealItem == 0) {
      // stolen coins
      int coin = target->e.nStealItem / RandomLong(2, 3);
      target->e.nStealItem -= coin;
      g.dwCash += coin;
      if (coin > 0)
        PAL_swprintf(s, sizeof(s) / sizeof(WCHAR), L"@%ls @%d @%ls@", PAL_GetWord(34), coin, PAL_GetWord(10));
    } else {
      // stolen item
      target->e.nStealItem--;
      PAL_AddItemToInventory(target->e.wStealItem, 1);
      PAL_swprintf(s, sizeof(s) / sizeof(WCHAR), L"%ls@%ls@", PAL_GetWord(34), PAL_GetWord(target->e.wStealItem));
    }
    if (s[0] != '\0') {
      PAL_StartDialog(kDialogCenterWindow, 0, 0, FALSE);
      PAL_ShowDialogText(s);
    }
  }
}

// Simulate a magic for players. Mostly used in item throwing script.
VOID PAL_BattleSimulateMagic(SHORT sTarget, WORD wMagicObjectID, WORD wBaseDamage) {
  if (OBJECT[wMagicObjectID].magic.wFlags & kMagicFlagApplyToAll)
    sTarget = -1;
  else if (sTarget == -1)
    sTarget = PAL_BattleSelectAutoTargetFrom(sTarget);

  // Show the magic animation
  PAL_BattleShowPlayerOffMagicAnim(0xFFFF, wMagicObjectID, sTarget, FALSE);

  if (MAGIC[OBJECT[wMagicObjectID].magic.wMagicNumber].wBaseDamage > 0 || wBaseDamage > 0) {
    if (sTarget == -1) {
      // Apply to all enemies
      for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
        if (BATTLE_ENEMY[i].wObjectID == 0)
          continue;

        int def = BATTLE_ENEMY[i].e.wDefense + (BATTLE_ENEMY[i].e.wLevel + 6) * 4;

        SHORT sDamage = PAL_CalcMagicDamage(wBaseDamage, (WORD)def, BATTLE_ENEMY[i].e.wElemResistance,
                                            BATTLE_ENEMY[i].e.wPoisonResistance, 1, wMagicObjectID);

        if (sDamage < 0)
          sDamage = 0;

        if (BATTLE_ENEMY[i].e.wHealth < sDamage)
          sDamage = BATTLE_ENEMY[i].e.wHealth;

        BATTLE_ENEMY[i].e.wHealth -= sDamage;
      }
    } else {
      // Apply to one enemy
      int def = BATTLE_ENEMY[sTarget].e.wDefense + (BATTLE_ENEMY[sTarget].e.wLevel + 6) * 4;

      SHORT sDamage = PAL_CalcMagicDamage(wBaseDamage, (WORD)def, BATTLE_ENEMY[sTarget].e.wElemResistance,
                                          BATTLE_ENEMY[sTarget].e.wPoisonResistance, 1, wMagicObjectID);

      if (sDamage < 0)
        sDamage = 0;

      if (BATTLE_ENEMY[sTarget].e.wHealth < sDamage)
        sDamage = BATTLE_ENEMY[sTarget].e.wHealth;

      BATTLE_ENEMY[sTarget].e.wHealth -= sDamage;
    }
  }
}

BOOL PAL_EnemyDivisionItself(WORD count, WORD wEventObjectID) {
  WORD alive = 0;
  for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++)
    if (BATTLE_ENEMY[i].wObjectID != 0)
      alive++;

  // Division is only possible when only 1 enemy left
  // health too low also cannot division
  if (alive != 1 || BATTLE_ENEMY[wEventObjectID].e.wHealth <= 1)
    return FALSE;

  if (count == 0)
    count = 1;
  WORD newHealth = (BATTLE_ENEMY[wEventObjectID].e.wHealth + count) / (count + 1);

  for (int i = 0; i < MAX_ENEMIES_IN_TEAM; i++) {
    if (count > 0 && BATTLE_ENEMY[i].wObjectID == 0) {
      count--;
      memset(&(BATTLE_ENEMY[i]), 0, sizeof(BATTLEENEMY));
      BATTLE_ENEMY[i].wObjectID = BATTLE_ENEMY[wEventObjectID].wObjectID;
      BATTLE_ENEMY[i].e = BATTLE_ENEMY[wEventObjectID].e;
      BATTLE_ENEMY[i].e.wHealth = newHealth;
      BATTLE_ENEMY[i].wScriptOnTurnStart = BATTLE_ENEMY[wEventObjectID].wScriptOnTurnStart;
      BATTLE_ENEMY[i].wScriptOnBattleEnd = BATTLE_ENEMY[wEventObjectID].wScriptOnBattleEnd;
      BATTLE_ENEMY[i].wScriptOnReady = BATTLE_ENEMY[wEventObjectID].wScriptOnReady;
      BATTLE_ENEMY[i].iColorShift = 0;
    }
  }
  BATTLE_ENEMY[wEventObjectID].e.wHealth = newHealth;

  // update wMaxEnemyIndex
  for (int i = 0; i < MAX_ENEMIES_IN_TEAM; i++)
    if (BATTLE_ENEMY[i].wObjectID != 0)
      g_Battle.wMaxEnemyIndex = i;

  PAL_LoadBattleSprites();

  for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++)
    if (BATTLE_ENEMY[i].wObjectID != 0)
      BATTLE_ENEMY[i].pos = BATTLE_ENEMY[wEventObjectID].pos;

  for (int i = 0; i < 10; i++) {
    for (int j = 0; j <= g_Battle.wMaxEnemyIndex; j++) {
      int x = (PAL_X(BATTLE_ENEMY[j].pos) + PAL_X(BATTLE_ENEMY[j].posOriginal)) / 2;
      int y = (PAL_Y(BATTLE_ENEMY[j].pos) + PAL_Y(BATTLE_ENEMY[j].posOriginal)) / 2;
      BATTLE_ENEMY[j].pos = PAL_XY(x, y);
    }
    PAL_BattleDelay(1, 0, TRUE);
  }

  PAL_BattleUpdateFighters();
  PAL_BattleDelay(1, 0, TRUE);
  return TRUE;
}

BOOL PAL_EnemySummonMonster(WORD monsterObjectID, WORD count, WORD wEventObjectID) {
  for (int i = 0; i < BATTLE_ENEMY[wEventObjectID].e.wMagicFrames; i++) {
    BATTLE_ENEMY[wEventObjectID].wCurrentFrame = BATTLE_ENEMY[wEventObjectID].e.wIdleFrames + i;
    PAL_BattleDelay(BATTLE_ENEMY[wEventObjectID].e.wActWaitFrames, 0, FALSE);
  }

  int w = monsterObjectID ? monsterObjectID : BATTLE_ENEMY[wEventObjectID].wObjectID;

  count = count == 0 ? 1 : count;

  int slot = 0;
  for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++)
    if (BATTLE_ENEMY[i].wObjectID == 0)
      slot++;

  if (slot < count || g_Battle.iHidingTime > 0 || //
      BATTLE_ENEMY[wEventObjectID].rgwStatus[kStatusSleep] != 0 ||
      BATTLE_ENEMY[wEventObjectID].rgwStatus[kStatusParalyzed] != 0 ||
      BATTLE_ENEMY[wEventObjectID].rgwStatus[kStatusConfused] != 0) {
    return FALSE;
  } else {
    for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
      if (count > 0 && BATTLE_ENEMY[i].wObjectID == 0) {
        count--;
        memset(&(BATTLE_ENEMY[i]), 0, sizeof(BATTLEENEMY));
        BATTLE_ENEMY[i].wObjectID = w;
        BATTLE_ENEMY[i].e = ENEMY[OBJECT[w].enemy.wEnemyID];
        BATTLE_ENEMY[i].wScriptOnTurnStart = OBJECT[w].enemy.wScriptOnTurnStart;
        BATTLE_ENEMY[i].wScriptOnBattleEnd = OBJECT[w].enemy.wScriptOnBattleEnd;
        BATTLE_ENEMY[i].wScriptOnReady = OBJECT[w].enemy.wScriptOnReady;
        BATTLE_ENEMY[i].iColorShift = 8;
      }
    }

    VIDEO_BackupScreen(g_Battle.lpSceneBuf);
    PAL_LoadBattleSprites();
    PAL_BattleMakeScene();
    AUDIO_PlaySound(212);
    PAL_BattleFadeScene();

    // avoid releasing gesture disappears before summon done
    PAL_BattleDelay(2, 0, TRUE);

    for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++)
      BATTLE_ENEMY[i].iColorShift = 0;

    VIDEO_BackupScreen(g_Battle.lpSceneBuf);
    PAL_BattleMakeScene();
    PAL_BattleFadeScene();
  }
  return TRUE;
}
