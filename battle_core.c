#include "main.h"

BATTLE g_Battle;

BOOL PAL_IsPlayerDying(WORD wPlayerRole) {
  return PLAYER.rgwHP[wPlayerRole] < min(100, PLAYER.rgwMaxHP[wPlayerRole] / 5);
}

BOOL PAL_IsPlayerHealthy(WORD wPlayerRole) {
  return !PAL_IsPlayerDying(wPlayerRole) && gPlayerStatus[wPlayerRole][kStatusSleep] == 0 &&
         gPlayerStatus[wPlayerRole][kStatusConfused] == 0 && gPlayerStatus[wPlayerRole][kStatusSilence] == 0 &&
         gPlayerStatus[wPlayerRole][kStatusParalyzed] == 0 && gPlayerStatus[wPlayerRole][kStatusPuppet] == 0;
}

BOOL PAL_CanPlayerAct(WORD wPlayerRole, BOOL fConsiderConfused) {
  return PLAYER.rgwHP[wPlayerRole] != 0 && gPlayerStatus[wPlayerRole][kStatusSleep] == 0 &&
         gPlayerStatus[wPlayerRole][kStatusParalyzed] == 0 &&
         (!fConsiderConfused || gPlayerStatus[wPlayerRole][kStatusConfused] == 0);
}

BOOL PAL_isPlayerBadStatus(WORD wPlayerRole) {
  return gPlayerStatus[wPlayerRole][kStatusConfused] > 0 || gPlayerStatus[wPlayerRole][kStatusSleep] > 0 ||
         gPlayerStatus[wPlayerRole][kStatusParalyzed] > 0;
}

// https://github.com/palxex/palresearch/blob/master/EXE/txts/%E9%9A%8F%E8%AE%B0.txt
SHORT PAL_CalcBaseDamage(WORD wAttackStrength, WORD wDefense) {
  if (wAttackStrength > wDefense)
    return (SHORT)((wAttackStrength - wDefense * 0.8) * 2 + 0.5);
  else if (wAttackStrength > wDefense * 0.6)
    return (SHORT)(wAttackStrength - wDefense * 0.6 + 0.5);
  else
    return 0;
}

// https://github.com/palxex/palresearch/blob/master/EXE/txts/%E9%9A%8F%E8%AE%B0.txt
SHORT PAL_CalcMagicDamage(WORD wMagicStrength, WORD wDefense, const WORD rgwElementalResistance[NUM_MAGIC_ELEMENTAL],
                          WORD wPoisonResistance, WORD wResistanceMultiplier, WORD wMagicObject) {
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

SHORT PAL_CalcPhysicalAttackDamage(WORD wAttackStrength, WORD wDefense, WORD wAttackResistance) {
  SHORT sDamage = PAL_CalcBaseDamage(wAttackStrength, wDefense);
  if (wAttackResistance != 0)
    sDamage /= wAttackResistance;
  return sDamage;
}

static WORD PAL_getPlayerDexterity(int i) {
  WORD wPlayerRole = PARTY_PLAYER(i);

  WORD wDexterity = PAL_GetPlayerDexterity(wPlayerRole);
  if (gPlayerStatus[wPlayerRole][kStatusHaste] != 0) // 身法
    wDexterity *= 3;
  if (wDexterity > 999)
    wDexterity = 999;

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

static SHORT PAL_GetEnemyDexterity(WORD wEnemyIndex) {
  return BATTLE_ENEMY[wEnemyIndex].e.wDexterity + (BATTLE_ENEMY[wEnemyIndex].e.wLevel + 6) * 3;
}

VOID PAL_BattleBackupStat(VOID) {
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
BOOL PAL_BattleDisplayStatChange(VOID) {
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

VOID PAL_BattleHandleKeyPressed() {
  if (g_Battle.UI.state == kBattleMenuMain) {
    if (g_InputState.dwKeyPress & kKeyRepeat) { // R
      g_Battle.fRepeat = TRUE;
      g_Battle.UI.fAutoAttack = g_Battle.fPrevAutoAtk;
    } else if (g_InputState.dwKeyPress & kKeyForce) { // F
      g_Battle.fForce = TRUE;
    }
  }

  // R/F/Q 应用到所有角色
  if (g_Battle.fRepeat)
    g_InputState.dwKeyPress = kKeyRepeat;
  else if (g_Battle.fForce)
    g_InputState.dwKeyPress = kKeyForce;
  else if (g_Battle.fFlee)
    g_InputState.dwKeyPress = kKeyFlee;

  // 取消围攻
  if (g_Battle.UI.fAutoAttack)
    if (g_InputState.dwKeyPress & kKeyMenu)
      g_Battle.UI.fAutoAttack = FALSE;

  // 切换围攻
  if (g_InputState.dwKeyPress & kKeyAuto) {
    g_Battle.UI.fAutoAttack = !g_Battle.UI.fAutoAttack;
    g_Battle.UI.state = kBattleUIWait;
  }

  if (g_InputState.dwKeyPress & kKeyStatus)
    PAL_PlayerStatus();
}

VOID PAL_BattleRenderStart() {
  PAL_BattleMakeScene();
  VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);

  PAL_BattleHandleKeyPressed();
}

VOID PAL_BattleRenderEnd() {
  PAL_BattleUIDrawNumbers();

  if (g_Battle.UI.fAutoAttack) {
    LPCWSTR itemText = PAL_GetWord(56); // 围攻
    PAL_DrawText(itemText, PAL_XY(320 - 8 - PAL_TextWidth(itemText), 10), MENUITEM_COLOR_CONFIRMED, TRUE, FALSE);
  }

  VIDEO_UpdateScreen(NULL);

  PAL_ClearKeyState();
}

VOID PAL_BattleDelay(WORD wDuration, WORD wObjectID, BOOL fUpdateGesture) {
  DWORD dwTime = PAL_GetTicks() + BATTLE_FRAME_TIME;

  for (int i = 0; i < wDuration; i++) {
    PAL_DelayUntil(dwTime);
    dwTime = PAL_GetTicks() + BATTLE_FRAME_TIME;

    // Update the gesture of enemies.
    if (fUpdateGesture)
      PAL_BattleUpdateAllEnemyFrame();

    PAL_BattleRenderStart();

    if (wObjectID != 0) {
      if (wObjectID == BATTLE_LABEL_ESCAPEFAIL) // HACKHACK
        PAL_DrawText(PAL_GetWord(wObjectID), PAL_XY(130, 75), 15, TRUE, FALSE);
      else
        PAL_DrawText(PAL_GetWord(wObjectID), PAL_XY(210, 50), 15, TRUE, FALSE);
    }

    PAL_BattleRenderEnd();
  }
}

INT PAL_BattleSelectAutoTargetFrom(INT begin) {
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

INT PAL_BattleSelectAutoTarget(WORD wMagic) {
  if (wMagic == 0) {
    if (PAL_PlayerCanAttackAll(PARTY_PLAYER(g_Battle.UI.wCurPlayerIndex)))
      return -1;
  } else {
    if (OBJECT[wMagic].magic.wFlags & kMagicFlagApplyToAll)
      return -1;
  }
  return PAL_BattleSelectAutoTargetFrom(0);
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

static BOOL PAL_CheckBattleEnd() {
  // If all enemies are cleared. Won the battle.
  if (g_Battle.fEnemyCleared) {
    g_Battle.BattleResult = kBattleResultWon;
    AUDIO_PlaySound(0);
    return TRUE;
  }
  // If all players are dead. Lost the battle.
  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
    WORD wPlayerRole = PARTY_PLAYER(i);
    if (PLAYER.rgwHP[wPlayerRole] != 0)
      return FALSE;
  }
  g_Battle.BattleResult = kBattleResultLost;
  return TRUE;
}

static VOID PAL_ProcessActionSelection() {
  if (g_Battle.UI.state == kBattleUIWait) {
    int idx;
    for (idx = 0; idx <= g.wMaxPartyMemberIndex; idx++) {
      WORD wPlayerRole = PARTY_PLAYER(idx);

      // Don't select action for player that cannot act
      if (!PAL_CanPlayerAct(wPlayerRole, TRUE))
        continue;

      // Start the menu for the first player whose action is not yet selected.
      if (BATTLE_PLAYER[idx].state == kFighterWait) {
        break;
      } else if (BATTLE_PLAYER[idx].action.ActionType == kBattleActionCoopMagic) {
        // 如果有人选择了合体技, 这通常消耗所有人的回合, 因此跳过后续队员的指令选择
        idx = g.wMaxPartyMemberIndex + 1;
        break;
      }
    }

    // 所有角色的行动都选择完毕, 生成敌我双方的行动序列
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

      // Reset queue.
      g_Battle.iCurAction = 0;
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
          BATTLE_PLAYER[i].action.ActionType = kBattleActionAttack;
          BATTLE_PLAYER[i].action.wActionID = 0;
          BATTLE_PLAYER[i].state = kFighterAct;
          g_Battle.ActionQueue[j].wDexterity = 0;
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
}

static VOID PAL_ProcessActionInQueue() {
  // Are all actions finished?
  if (g_Battle.iCurAction >= MAX_ACTIONQUEUE_ITEMS) {
    // Restore player from defending
    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
      BATTLE_PLAYER[i].fDefending = FALSE;
      BATTLE_PLAYER[i].pos = BATTLE_PLAYER[i].posOriginal;
    }

    PAL_BattleBackupStat();

    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
      WORD wPlayerRole = PARTY_PLAYER(i);
      // 执行中毒脚本
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
      // 执行中毒脚本
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
      PAL_BattleDelay(10, 0, TRUE);

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
    g_Battle.UI.state = kBattleUIWait;
  } else {
    // Perform action in queue.
    int idx = g_Battle.ActionQueue[g_Battle.iCurAction].wIndex;
    BOOL isEnemy = g_Battle.ActionQueue[g_Battle.iCurAction].fIsEnemy;
    g_Battle.iCurAction++;

    if (isEnemy) {
      // 敌人行动
      BATTLEENEMY *enemy = &BATTLE_ENEMY[idx];
      if (g_Battle.iHidingTime == 0 && BATTLE_ENEMY[idx].wObjectID != 0) {
        if (enemy->rgwStatus[kStatusConfused] == 0 && enemy->rgwStatus[kStatusParalyzed] == 0 &&
            enemy->rgwStatus[kStatusSleep] == 0)
          enemy->wScriptOnReady = PAL_RunTriggerScript(enemy->wScriptOnReady, idx);

        // 用于脚本 0x0068 判断毒或异常状态施加我方还是敌方
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
        player->action.ActionType = PAL_IsPlayerDying(wPlayerRole) ? kBattleActionPass : kBattleActionAttackMate;
      } else if (player->action.ActionType == kBattleActionAttack && player->action.wActionID != 0) {
        // PAL_BattleCommitAction 中若选择了围攻则设置 wActionID = 1
        // 然后这里会设置 fPrevPlayerAutoAtk = 1 使得后续我方攻击都变成普攻
        g_Battle.fPrevPlayerAutoAtk = TRUE;
      } else if (g_Battle.fPrevPlayerAutoAtk) {
        // 本回合有人选择围攻, 强制转换为普通攻击
        player->action.ActionType = kBattleActionAttack;
        player->action.wActionID = 0;
      }

      // Perform the action for this player.
      PAL_BattlePlayerPerformAction(idx);
    }
  }
}

// Show the "you win" message and add the experience points for players.
static VOID PAL_BattleWon(VOID) {
  const SDL_Rect rect = {0, 60, 320, 100};
  SDL_Rect rect1 = {80, 0, 180, 200};

  // Backup the initial player stats
  PLAYERROLES ORIG_PLAYER = PLAYER;

  VIDEO_BackupScreen(gpScreen);

  if (g_Battle.iExpGained > 0) {
    // Play the "battle win" music
    AUDIO_PlayMusic(g_Battle.fIsBoss ? 2 : 3, FALSE, 0);

    // Show the message about the total number of exp. and cash gained
    int w1 = PAL_WordWidth(BATTLEWIN_GETEXP_LABEL) + 3;
    int ww1 = (w1 - 8) << 3;
    PAL_CreateSingleLineBox(PAL_XY(83 - ww1, 60), w1, FALSE);
    PAL_CreateSingleLineBox(PAL_XY(65, 105), 10, FALSE);

    PAL_DrawText(PAL_GetWord(BATTLEWIN_GETEXP_LABEL), PAL_XY(95 - ww1, 70), 0, FALSE, FALSE);
    PAL_DrawText(PAL_GetWord(BATTLEWIN_BEATENEMY_LABEL), PAL_XY(77, 115), 0, FALSE, FALSE);
    PAL_DrawText(PAL_GetWord(BATTLEWIN_DOLLAR_LABEL), PAL_XY(197, 115), 0, FALSE, FALSE);

    PAL_DrawNumber(g_Battle.iExpGained, 5, PAL_XY(182 + ww1, 74), kNumColorYellow, kNumAlignRight);
    PAL_DrawNumber(g_Battle.iCashGained, 5, PAL_XY(162, 119), kNumColorYellow, kNumAlignMid);

    VIDEO_UpdateScreen(&rect);
    PAL_WaitForAnyKey(g_Battle.fIsBoss ? 5500 : 3000);
  }

  // Add the cash value
  g.dwCash += g_Battle.iCashGained;

  // Add the experience points for each players
  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
    BOOL fLevelUp = FALSE;

    WORD w = PARTY_PLAYER(i);
    if (PLAYER.rgwHP[w] == 0)
      continue; // don't care about dead players

    DWORD dwExp = g.Exp.rgPrimaryExp[w].wExp + g_Battle.iExpGained;

    if (PLAYER.rgwLevel[w] > MAX_LEVELS)
      PLAYER.rgwLevel[w] = MAX_LEVELS;

    while (dwExp >= LEVELUP_EXP[PLAYER.rgwLevel[w]]) {
      dwExp -= LEVELUP_EXP[PLAYER.rgwLevel[w]];

      if (PLAYER.rgwLevel[w] < MAX_LEVELS) {
        fLevelUp = TRUE;
        PAL_PlayerLevelUp(w, 1);

        PLAYER.rgwHP[w] = PLAYER.rgwMaxHP[w];
        PLAYER.rgwMP[w] = PLAYER.rgwMaxMP[w];
      }
    }

    g.Exp.rgPrimaryExp[w].wExp = (WORD)dwExp;

    if (fLevelUp) {
      VIDEO_RestoreScreen(gpScreen);

      // Player has gained a level, show the message.
      PAL_DrawLevelUpMessage(w, &ORIG_PLAYER);

      // Update the screen and wait for key
      VIDEO_UpdateScreen(&rect1);
      PAL_WaitForAnyKey(3000);

      ORIG_PLAYER = PLAYER;
    }

    // Increasing of other hidden levels
    int iTotalCount = 0;

    iTotalCount += g.Exp.rgAttackExp[w].wCount;
    iTotalCount += g.Exp.rgDefenseExp[w].wCount;
    iTotalCount += g.Exp.rgDexterityExp[w].wCount;
    iTotalCount += g.Exp.rgFleeExp[w].wCount;
    iTotalCount += g.Exp.rgHealthExp[w].wCount;
    iTotalCount += g.Exp.rgMagicExp[w].wCount;
    iTotalCount += g.Exp.rgMagicPowerExp[w].wCount;

    if (iTotalCount > 0) {
#define CHECK_HIDDEN_EXP(expname, statname, label)                                                                     \
  {                                                                                                                    \
    int dwExp = g_Battle.iExpGained;                                                                                   \
    dwExp *= g.Exp.expname[w].wCount;                                                                                  \
    dwExp /= iTotalCount;                                                                                              \
    dwExp *= 2;                                                                                                        \
    dwExp += g.Exp.expname[w].wExp;                                                                                    \
                                                                                                                       \
    if (g.Exp.expname[w].wLevel > MAX_LEVELS)                                                                          \
      g.Exp.expname[w].wLevel = MAX_LEVELS;                                                                            \
                                                                                                                       \
    while (dwExp >= LEVELUP_EXP[g.Exp.expname[w].wLevel]) {                                                            \
      dwExp -= LEVELUP_EXP[g.Exp.expname[w].wLevel];                                                                   \
      PLAYER.statname[w] += RandomLong(1, 2);                                                                          \
      if (g.Exp.expname[w].wLevel < MAX_LEVELS)                                                                        \
        g.Exp.expname[w].wLevel++;                                                                                     \
    }                                                                                                                  \
                                                                                                                       \
    g.Exp.expname[w].wExp = (WORD)dwExp;                                                                               \
                                                                                                                       \
    if (PLAYER.statname[w] != ORIG_PLAYER.statname[w]) {                                                               \
      WCHAR buffer[256] = L"";                                                                                         \
      PAL_swprintf(buffer, 256, L"%ls%ls%ls", PAL_GetWord(PLAYER.rgwName[w]), PAL_GetWord(label),                      \
                   PAL_GetWord(BATTLEWIN_LEVELUP_LABEL));                                                              \
      PAL_CreateSingleLineBox(PAL_XY(78, 60), 9, FALSE);                                                               \
      PAL_DrawText(buffer, PAL_XY(90, 70), 0, FALSE, FALSE);                                                           \
      PAL_DrawNumber(PLAYER.statname[w] - ORIG_PLAYER.statname[w], 5, PAL_XY(191, 74), kNumColorYellow,                \
                     kNumAlignRight);                                                                                  \
      VIDEO_UpdateScreen(&rect);                                                                                       \
      PAL_WaitForAnyKey(3000);                                                                                         \
    }                                                                                                                  \
  }

      CHECK_HIDDEN_EXP(rgHealthExp, rgwMaxHP, STATUS_LABEL_HP);
      CHECK_HIDDEN_EXP(rgMagicExp, rgwMaxMP, STATUS_LABEL_MP);
      CHECK_HIDDEN_EXP(rgAttackExp, rgwAttackStrength, STATUS_LABEL_ATTACKPOWER);
      CHECK_HIDDEN_EXP(rgMagicPowerExp, rgwMagicStrength, STATUS_LABEL_MAGICPOWER);
      CHECK_HIDDEN_EXP(rgDefenseExp, rgwDefense, STATUS_LABEL_RESISTANCE);
      CHECK_HIDDEN_EXP(rgDexterityExp, rgwDexterity, STATUS_LABEL_DEXTERITY);
      CHECK_HIDDEN_EXP(rgFleeExp, rgwFleeRate, STATUS_LABEL_FLEERATE);

#undef CHECK_HIDDEN_EXP

      // Avoid HP/MP out of sync with upgraded maxHP/MP
      if (fLevelUp) {
        PLAYER.rgwHP[w] = PLAYER.rgwMaxHP[w];
        PLAYER.rgwMP[w] = PLAYER.rgwMaxMP[w];
      }
    }

    // Learn all magics at the current level
    for (int j = 0; j < N_LEVELUP_MAGIC; j++) {
      if (LEVELUP_MAGIC[j].m[w].wMagic == 0 || LEVELUP_MAGIC[j].m[w].wLevel > PLAYER.rgwLevel[w])
        continue;
      if (PAL_AddMagic(w, LEVELUP_MAGIC[j].m[w].wMagic)) {
        PAL_CreateSingleLineBox(PAL_XY(65, 105), 10, FALSE);
        PAL_DrawText(PAL_GetWord(PLAYER.rgwName[w]), PAL_XY(75, 115), 0, FALSE, FALSE);
        PAL_DrawText(PAL_GetWord(BATTLEWIN_ADDMAGIC_LABEL), PAL_XY(75 + 16 * 3, 115), 0, FALSE, FALSE);
        PAL_DrawText(PAL_GetWord(LEVELUP_MAGIC[j].m[w].wMagic), PAL_XY(75 + 16 * 5, 115), 0x1B, FALSE, FALSE);
        VIDEO_UpdateScreen(&rect);
        PAL_WaitForAnyKey(3000);
      }
    }
  }

  // Run the post-battle scripts
  for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++)
    PAL_RunTriggerScript(BATTLE_ENEMY[i].wScriptOnBattleEnd, i);

  // Recover automatically after each battle
  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
    WORD w = PARTY_PLAYER(i);
    PLAYER.rgwHP[w] += (PLAYER.rgwMaxHP[w] - PLAYER.rgwHP[w]) / 2;
    PLAYER.rgwMP[w] += (PLAYER.rgwMaxMP[w] - PLAYER.rgwMP[w]) / 2;
  }
}

// The main battle routine.
static BATTLERESULT PAL_BattleMain(VOID) {
  // Generate the scene and switch to it with effect
  VIDEO_BackupScreen(gpScreen);
  PAL_BattleMakeScene();
  VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);
  AUDIO_PlayMusic(0, FALSE, 1);
  UTIL_Delay(200);
  VIDEO_SwitchScreen(5);

  // Play the battle music
  AUDIO_PlayMusic(g.wCurBattleMusic, TRUE, 0);

  // Fade in the screen when needed
  if (gNeedToFadeIn) {
    PAL_FadeIn(gCurPalette, gNightPalette, 1);
    gNeedToFadeIn = FALSE;
  }

  PAL_ClearKeyState();
  uint64_t dwTime = PAL_GetTicks();

  // Run the main battle loop.
  g_Battle.BattleResult = kBattleResultOnGoing;
  while (g_Battle.BattleResult == kBattleResultOnGoing) {
    PAL_DelayUntil(dwTime);
    dwTime = PAL_GetTicks() + BATTLE_FRAME_TIME;

    PAL_BattleUpdateFighters();
    PAL_BattleRenderStart();

    if (g_Battle.Phase == kBattlePhaseSelectAction) {
      PAL_ProcessActionSelection();
      PAL_BattleUIUpdate();
    } else {
      PAL_ProcessActionInQueue();
    }

    if (PAL_CheckBattleEnd())
      break;

    PAL_BattleRenderEnd();
  }

  // Return the battle result
  return g_Battle.BattleResult;
}

BATTLERESULT
PAL_StartBattle(WORD wEnemyTeam, BOOL fIsBoss) {

  // Make sure everyone in the party is alive, also clear all hidden EXP count records.
  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
    WORD w = PARTY_PLAYER(i);

    if (PLAYER.rgwHP[w] == 0) {
      PLAYER.rgwHP[w] = 1;
      gPlayerStatus[w][kStatusPuppet] = 0;
    }

    g.Exp.rgHealthExp[w].wCount = 0;
    g.Exp.rgMagicExp[w].wCount = 0;
    g.Exp.rgAttackExp[w].wCount = 0;
    g.Exp.rgMagicPowerExp[w].wCount = 0;
    g.Exp.rgDefenseExp[w].wCount = 0;
    g.Exp.rgDexterityExp[w].wCount = 0;
    g.Exp.rgFleeExp[w].wCount = 0;
  }

  // Store all enemies
  memset(BATTLE_ENEMY, 0, sizeof(BATTLE_ENEMY));
  g_Battle.wMaxEnemyIndex = 0;
  for (int i = 0, j = 0; j < MAX_ENEMIES_IN_TEAM; j++) {
    WORD w = ENEMY_TEAM[wEnemyTeam].id[j];
    if (w == 0xFFFF)
      continue;

    OBJECT_ENEMY *enemy = &OBJECT[w].enemy;
    if (w != 0) {
      BATTLE_ENEMY[i].e = ENEMY[enemy->wEnemyID];
      BATTLE_ENEMY[i].wScriptOnTurnStart = enemy->wScriptOnTurnStart;
      BATTLE_ENEMY[i].wScriptOnBattleEnd = enemy->wScriptOnBattleEnd;
      BATTLE_ENEMY[i].wScriptOnReady = enemy->wScriptOnReady;
    }

    BATTLE_ENEMY[i++].wObjectID = w;
    g_Battle.wMaxEnemyIndex = i - 1;
  }

  // Store all players
  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
    BATTLE_PLAYER[i].state = kFighterWait;
    BATTLE_PLAYER[i].fDefending = FALSE;
    BATTLE_PLAYER[i].wCurrentFrame = 0;
    BATTLE_PLAYER[i].iColorShift = 0;
  }

  PAL_LoadBattleRenderResources();

  PAL_UpdateEquipmentEffect();

  g_Battle.iExpGained = 0;
  g_Battle.iCashGained = 0;

  g_Battle.fIsBoss = fIsBoss;
  g_Battle.fEnemyCleared = FALSE;
  g_Battle.fEnemyMoving = FALSE;
  g_Battle.iHidingTime = 0;

  g_Battle.UI.state = kBattleUIWait;
  g_Battle.UI.fAutoAttack = FALSE;
  g_Battle.UI.iSelectedIndex = 0;
  g_Battle.UI.iPrevEnemyTarget = -1;

  memset(g_Battle.UI.rgShowNum, 0, sizeof(g_Battle.UI.rgShowNum));

  g_Battle.lpSummonSprite = NULL;
  g_Battle.sBackgroundColorShift = 0;

  g_Battle.fSpriteAddLock = TRUE;

  // To run enemy's script on turn start
  g_Battle.Phase = kBattlePhasePerformAction;
  g_Battle.iCurAction = MAX_ACTIONQUEUE_ITEMS;

  g_Battle.fRepeat = FALSE;
  g_Battle.fForce = FALSE;
  g_Battle.fFlee = FALSE;
  g_Battle.fPrevAutoAtk = FALSE;
  g_Battle.fPrevPlayerAutoAtk = FALSE;

  // Run the main battle routine.
  gInBattle = TRUE;
  int result = PAL_BattleMain();
  // Player won the battle. Add the Experience points.
  if (result == kBattleResultWon)
    PAL_BattleWon();
  gInBattle = FALSE;

  // Clear all player status, poisons and temporary effects
  PAL_ClearAllPlayerStatus();
  for (WORD w = 0; w < MAX_PLAYER_ROLES; w++) {
    PAL_CurePoisonByLevel(w, 3);
    // 移除梦蛇/愤怒/虚弱的效果
    PAL_RemoveEquipmentEffect(w, kBodyPartExtra);
  }

  PAL_FreeBattleRenderResources();

  AUDIO_PlayMusic(g.wCurMusic, TRUE, 1);

  return result;
}
