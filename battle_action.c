#include "main.h"

// Validate player's action, fallback to other action when needed.
// Validate player's target, re-select if needed.
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
        if (PAL_IsPlayerHealthy(PARTY_PLAYER(i)))
          iTotalHealthy++;
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

static VOID HandlePlayerAttack(WORD wPlayerIndex, SHORT sTarget) {
  WORD wPlayerRole = PARTY_PLAYER(wPlayerIndex);
  BATTLEPLAYER *player = &BATTLE_PLAYER[wPlayerIndex];

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

      BOOL fSecondAttack = (t > 0);
      PAL_BattleShowPlayerAttackAnim(wPlayerIndex, fCritical, fSecondAttack);
    }
  } else {
    // Attack all enemies
    for (int t = 0; t < (gPlayerStatus[wPlayerRole][kStatusDualAttack] ? 2 : 1); t++) {
      int division = 1;
      const int index[MAX_ENEMIES_IN_TEAM] = {2, 1, 0, 4, 3};

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

      BOOL fSecondAttack = (t > 0);
      PAL_BattleShowPlayerAttackAnim(wPlayerIndex, fCritical, fSecondAttack);
      PAL_BattleDelay(4, 0, TRUE);
    }
  }

  PAL_BattleUpdateFighters();
  PAL_BattleMakeScene();
  PAL_BattleDelay(3, 0, TRUE);

  g.Exp.rgAttackExp[wPlayerRole].wCount++;
  g.Exp.rgHealthExp[wPlayerRole].wCount += RandomLong(2, 3);
}

static VOID HandlePlayerAttackMate(WORD wPlayerIndex, SHORT sTarget) {
  WORD wPlayerRole = PARTY_PLAYER(wPlayerIndex);
  BATTLEPLAYER *player = &BATTLE_PLAYER[wPlayerIndex];

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
}

static VOID HandlePlayerCoopMagic(WORD wPlayerIndex, SHORT sTarget) {
  BATTLEPLAYER *player = &BATTLE_PLAYER[wPlayerIndex];

  WORD rgwCoopPos[3][2] = {{208, 157}, {234, 170}, {260, 183}};

  WORD wObject = player->action.wActionID;
  WORD wMagicNum = OBJECT[wObject].magic.wMagicNumber;

  BOOL coopContributors[MAX_PLAYERS_IN_PARTY];
  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
    if (PAL_IsPlayerHealthy(PARTY_PLAYER(i)))
      coopContributors[i] = TRUE;
    else
      coopContributors[i] = FALSE;
  }

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
        if (coopContributors[j] == FALSE)
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
      if (coopContributors[i] == FALSE)
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
    if (coopContributors[i] == FALSE)
      continue;

    PLAYER.rgwHP[PARTY_PLAYER(i)] -= MAGIC[wMagicNum].wCostMP;
    if ((SHORT)(PLAYER.rgwHP[PARTY_PLAYER(i)]) <= 0)
      PLAYER.rgwHP[PARTY_PLAYER(i)] = 1;
  }

  PAL_BattleBackupStat(); // so that "damages" to players won't be shown

  WORD str = 0;
  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
    if (coopContributors[i] == FALSE)
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
    // Move all players back to the original position
    for (int i = 1; i <= 6; i++) {
      // Update the position for the player who invoked the action
      int x = (PAL_X(player->posOriginal) * i + rgwCoopPos[0][0] * (6 - i)) / 6;
      int y = (PAL_Y(player->posOriginal) * i + rgwCoopPos[0][1] * (6 - i)) / 6;
      player->pos = PAL_XY(x, y);

      // Update the position for other players
      for (int j = 0, t = 0; j <= g.wMaxPartyMemberIndex; j++) {
        if (coopContributors[j] == FALSE)
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
}

static VOID HandlePlayerMagic(WORD wPlayerIndex, SHORT sTarget) {
  WORD wPlayerRole = PARTY_PLAYER(wPlayerIndex);
  BATTLEPLAYER *player = &BATTLE_PLAYER[wPlayerIndex];

  WORD wObject = player->action.wActionID;
  WORD wMagicNum = OBJECT[wObject].magic.wMagicNumber;
  MAGIC_T *magic = &MAGIC[wMagicNum];

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
}

static VOID HandlePlayerFlee(WORD wPlayerIndex, SHORT sTarget) {
  WORD wPlayerRole = PARTY_PLAYER(wPlayerIndex);
  BATTLEPLAYER *player = &BATTLE_PLAYER[wPlayerIndex];

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
}

static VOID HandlePlayerThrowItem(WORD wPlayerIndex, SHORT sTarget) {
  BATTLEPLAYER *player = &BATTLE_PLAYER[wPlayerIndex];
  WORD wObject = player->action.wActionID;

  PAL_BattleShowPlayerThrowItemAnim(wPlayerIndex, wObject, sTarget);

  // Run the script
  OBJECT[wObject].item.wScriptOnThrow = PAL_RunTriggerScript(OBJECT[wObject].item.wScriptOnThrow, (WORD)sTarget);

  // Remove the thrown item from inventory
  PAL_AddItemToInventory(wObject, -1);

  PAL_BattleDisplayStatChange();
  PAL_BattleDelay(4, 0, TRUE);
  PAL_BattleUpdateFighters();
  PAL_BattleDelay(4, 0, TRUE);

  PAL_BattleCheckHidingEffect();
}

static VOID HandlePlayerUseItem(WORD wPlayerIndex, SHORT sTarget) {
  BATTLEPLAYER *player = &BATTLE_PLAYER[wPlayerIndex];
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
}

// Perform the selected action for a player.
VOID PAL_BattlePlayerPerformAction(WORD wPlayerIndex) {
  WORD wPlayerRole = PARTY_PLAYER(wPlayerIndex);
  BATTLEPLAYER *player = &BATTLE_PLAYER[wPlayerIndex];

  // for script 0x0039 (steal) and 0x0066 (throw weapon)
  g_Battle.wMovingPlayerIndex = wPlayerIndex;

  g_Battle.iBlow = 0;

  SHORT origTarget = player->action.sTarget;
  PAL_BattlePlayerValidateAction(wPlayerIndex);
  // orig target may be dead, and target is re-selected in above function
  SHORT sTarget = player->action.sTarget;
  PAL_BattleBackupStat();

  // process different action types
  BOOL fThisTurnCoop = FALSE;
  if (!fThisTurnCoop) {
    switch (player->action.ActionType) {
    case kBattleActionAttack:
      HandlePlayerAttack(wPlayerIndex, sTarget);
      break;
    case kBattleActionAttackMate:
      HandlePlayerAttackMate(wPlayerIndex, sTarget);
      break;
    case kBattleActionCoopMagic:
      fThisTurnCoop = TRUE;
      HandlePlayerCoopMagic(wPlayerIndex, sTarget);
      break;
    case kBattleActionDefend:
      player->fDefending = TRUE;
      g.Exp.rgDefenseExp[wPlayerRole].wCount += 2;
      break;
    case kBattleActionFlee:
      HandlePlayerFlee(wPlayerIndex, sTarget);
      break;
    case kBattleActionMagic:
      HandlePlayerMagic(wPlayerIndex, sTarget);
      break;
    case kBattleActionThrowItem:
      HandlePlayerThrowItem(wPlayerIndex, sTarget);
      break;
    case kBattleActionUseItem:
      HandlePlayerUseItem(wPlayerIndex, sTarget);
      break;
    case kBattleActionPass:
      break;
    }
  }

  // Revert this player back to waiting state.
  player->state = kFighterWait;

  PAL_BattlePostActionCheck(FALSE);

  // Revert target slot of this player
  player->action.sTarget = origTarget;
}

VOID PAL_BattleEnemyPerformAction(WORD wEnemyIndex) {
  PAL_BattleBackupStat();
  g_Battle.iBlow = 0;

  BATTLEENEMY *enemy = &BATTLE_ENEMY[wEnemyIndex];
  WORD wMagic = enemy->e.wMagic;

  int sTarget;
  do {
    sTarget = RandomLong(0, g.wMaxPartyMemberIndex);
  } while (PLAYER.rgwHP[PARTY_PLAYER(sTarget)] == 0);
  WORD wPlayerRole = PARTY_PLAYER(sTarget);

  // 如果敌人睡眠、定身
  if (enemy->rgwStatus[kStatusSleep] > 0 || enemy->rgwStatus[kStatusParalyzed] > 0)
    return;

  if (enemy->rgwStatus[kStatusConfused] > 0) {
    // 敌人混乱攻击敌人
    int alive = 0;
    for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++)
      if (BATTLE_ENEMY[i].wObjectID != 0)
        alive++;
    if (alive == 1)
      return;

    int iTarget;
    do {
      iTarget = RandomLong(0, g_Battle.wMaxEnemyIndex);
    } while (iTarget == wEnemyIndex || BATTLE_ENEMY[iTarget].wObjectID == 0);

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

      PAL_BattleRenderStart();
      PAL_RLEBlitToSurface(b, gpScreen, PAL_XY(x - PAL_RLEGetWidth(b) / 2, y - PAL_RLEGetHeight(b)));
      PAL_BattleRenderEnd();
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

  } else if (wMagic != 0 && RandomLong(0, 9) < enemy->e.wMagicRate && enemy->rgwStatus[kStatusSilence] == 0) {
    if (wMagic == 0xFFFF)
      return;

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
          continue;

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
          continue;

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
    PAL_BattleDelay(8, 0, TRUE);

  } else {
    // Physical attack
    BATTLEPLAYER *target = &BATTLE_PLAYER[sTarget];

    WORD wFrameBak = target->wCurrentFrame;

    if (g_Battle.ActionQueue[g_Battle.iCurAction].fIsSecond && enemy->e.wMagic == 0)
      AUDIO_PlaySound(enemy->e.wMagicSound);
    else
      AUDIO_PlaySound(enemy->e.wAttackSound);

    BOOL fAutoDefend = (RandomLong(0, 16) >= 10); // 随机完美防御

    // 如果目标处于濒死或糟糕状态，完美防御触发掩护
    int iCoverIndex = -1;
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
      // 完美防御
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
    PAL_BattleDelay(5, 0, TRUE);

    PAL_BattleUpdateFighters();

    // 运行怪物自带毒攻击的脚本
    if (iCoverIndex == -1 && !fAutoDefend && enemy->e.wAttackEquivItemRate >= RandomLong(1, 10) &&
        PAL_GetPlayerPoisonResistance(wPlayerRole) < RandomLong(1, 100)) {
      int weapon = enemy->e.wAttackEquivItem;
      OBJECT[weapon].item.wScriptOnUse = PAL_RunTriggerScript(OBJECT[weapon].item.wScriptOnUse, wPlayerRole);
    }
  }

  PAL_BattlePostActionCheck(TRUE);
}

VOID PAL_BattlePostActionCheck(BOOL fCheckPlayers) {
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
      fFade = TRUE;
      continue;
    }

    fEnemyRemaining = TRUE;
  }

  if (!fEnemyRemaining) {
    g_Battle.fEnemyCleared = TRUE;
  }

  if (fCheckPlayers && !gAutoBattle) {
    BOOL found = FALSE;
    for (int i = 0; i <= g.wMaxPartyMemberIndex && !found; i++) {
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
          g_Battle.BattleResult = kBattleResultPause;

          // 触发友方愤怒
          OBJECT[wName].player.wScriptOnFriendDeath =
              PAL_RunTriggerScript(OBJECT[wName].player.wScriptOnFriendDeath, wCover);

          g_Battle.BattleResult = kBattleResultOnGoing;
          found = TRUE;
        }
      }
    }

    for (int i = 0; i <= g.wMaxPartyMemberIndex && !found; i++) {
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
          g_Battle.BattleResult = kBattleResultPause;

          // 触发自己虚弱
          OBJECT[wName].player.wScriptOnDying = PAL_RunTriggerScript(OBJECT[wName].player.wScriptOnDying, w);

          g_Battle.BattleResult = kBattleResultOnGoing;
          found = TRUE;
        }
      }
    }
  }

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

VOID PAL_BattleCommitAction(BOOL fRepeat) {
  BATTLEPLAYER *player = &BATTLE_PLAYER[g_Battle.UI.wCurPlayerIndex];

  if (!fRepeat) {
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

  // 资源预扣除
  if ((player->action.ActionType == kBattleActionUseItem &&
       (OBJECT[player->action.wActionID].item.wFlags & kItemFlagConsuming)) ||
      player->action.ActionType == kBattleActionThrowItem) {
    for (WORD w = 0; w < MAX_INVENTORY; w++) {
      if (g.rgInventory[w].wItem == player->action.wActionID) {
        g.rgInventory[w].nAmountInUse++; // 标记物品被占用
        break;
      }
    }
  }

  // 逃跑应用到所有角色
  if (g_Battle.UI.wActionType == kBattleActionFlee)
    g_Battle.fFlee = TRUE;

  player->state = kFighterAct;
  g_Battle.UI.state = kBattleUIWait;
}
