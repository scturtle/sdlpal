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
// Based on PALx Project by palxex.
// Copyright (c) 2006-2008, Pal Lockheart <palxex@gmail.com>.
//

#include "main.h"

BOOL g_fScriptSuccess = TRUE;
static int g_iCurEquipPart = -1;
static int g_iCurPlayingRNG; // 当前正在播放的 RNG 动画编号

static WORD PAL_InterpretInstruction(WORD wScriptEntry, WORD wEventObjectID)
/*++
  Purpose:
    Interpret and execute one instruction in the script.

  Parameters:
    [IN]  wScriptEntry - The script entry to execute.
    [IN]  wEventObjectID - The event object ID which invoked the script.

  Return value:
    The address of the next script instruction to execute.
--*/
{
  LPSCRIPTENTRY pScript = &(SCRIPT[wScriptEntry]);

  LPEVENTOBJECT pEvtObj = wEventObjectID != 0 ? &g.events[wEventObjectID - 1] : NULL;

  // If opr0 is ObjectID, should just use pCurrent.
  LPEVENTOBJECT pCurrent;
  if (pScript->rgwOperand[0] == 0 || pScript->rgwOperand[0] == 0xFFFF) {
    pCurrent = pEvtObj;
  } else {
    pCurrent = &(g.events[pScript->rgwOperand[0] - 1]);
  }

  switch (pScript->wOperation) {
  case 0x000B:
  case 0x000C:
  case 0x000D:
  case 0x000E:
    // walk one step
    pEvtObj->wDirection = pScript->wOperation - 0x000B;
    PAL_NPCWalkOneStep(wEventObjectID, 2);
    break;

  case 0x000F:
    // Set the direction and/or gesture for event object
    if (pScript->rgwOperand[0] != 0xFFFF)
      pEvtObj->wDirection = pScript->rgwOperand[0];
    if (pScript->rgwOperand[1] != 0xFFFF)
      pEvtObj->wCurrentFrameNum = pScript->rgwOperand[1];
    break;

  case 0x0010:
    // Walk straight to the specified position
    if (!PAL_NPCWalkTo(wEventObjectID, pScript->rgwOperand[0], pScript->rgwOperand[1], pScript->rgwOperand[2], 3))
      wScriptEntry--;
    break;

  case 0x0011:
    // Walk straight to the specified position, at a lower speed
    if ((wEventObjectID & 1) ^ (gFrameNum & 1)) {
      if (!PAL_NPCWalkTo(wEventObjectID, pScript->rgwOperand[0], pScript->rgwOperand[1], pScript->rgwOperand[2], 2))
        wScriptEntry--;
    } else {
      wScriptEntry--;
    }
    break;

  case 0x0012:
    // Set the position of the event object, relative to the party
    pCurrent->x = pScript->rgwOperand[1] + PAL_X(g.viewport) + PAL_X(gPartyOffset);
    pCurrent->y = pScript->rgwOperand[2] + PAL_Y(g.viewport) + PAL_Y(gPartyOffset);
    break;

  case 0x0013:
    // Set the position of the event object
    pCurrent->x = pScript->rgwOperand[1];
    pCurrent->y = pScript->rgwOperand[2];
    break;

  case 0x0014:
    // Set the gesture of the event object
    pEvtObj->wCurrentFrameNum = pScript->rgwOperand[0];
    pEvtObj->wDirection = kDirSouth;
    break;

  case 0x0015:
    // Set the direction and gesture for a party member
    g.wPartyDirection = pScript->rgwOperand[0];
    PARTY[pScript->rgwOperand[2]].wFrame = g.wPartyDirection * 3 + pScript->rgwOperand[1];
    break;

  case 0x0016:
    // Set the direction and gesture for an event object
    if (pScript->rgwOperand[0] != 0) {
      pCurrent->wDirection = pScript->rgwOperand[1];
      pCurrent->wCurrentFrameNum = pScript->rgwOperand[2];
    }
    break;

  case 0x0017: {
    // set the player's extra attribute
    int part = pScript->rgwOperand[0] - 0xB;
    WORD *p = (WORD *)(&gEquipmentEffect[part]); // HACKHACK
    p[pScript->rgwOperand[1] * MAX_PLAYER_ROLES + wEventObjectID] = (SHORT)pScript->rgwOperand[2];
  } break;

  case 0x0018: {
    // Equip the selected item
    int part = pScript->rgwOperand[0] - 0x0B;
    g_iCurEquipPart = part;

    // The wEventObjectID parameter here should indicate the player role
    PAL_RemoveEquipmentEffect(wEventObjectID, part);

    WORD prevObjID = PLAYER.rgwEquipment[part][wEventObjectID];
    if (prevObjID != pScript->rgwOperand[1]) {
      PLAYER.rgwEquipment[part][wEventObjectID] = pScript->rgwOperand[1];
      gLastUnequippedItem = prevObjID;

      int i, j;
      if (PAL_GetItemIndexToInventory(pScript->rgwOperand[1], &i) && i < MAX_INVENTORY &&
          g.rgInventory[i].nAmount == 1 && prevObjID != 0 && !PAL_GetItemIndexToInventory(prevObjID, &j)) {
        // When the number of items you want to wear is 1 and the number of items you are wearing is also 1,
        // replace them directly, instead of removing items and adding them at the end of the item menu
        g.rgInventory[i].wItem = prevObjID;
      } else {
        PAL_AddItemToInventory(pScript->rgwOperand[1], -1);
        if (prevObjID != 0)
          PAL_AddItemToInventory(prevObjID, 1);
      }
    }
  } break;

  case 0x0019: {
    // Increase/decrease the player's attribute
    WORD *p = (WORD *)(&PLAYER); // HACKHACK

    int iPlayerRole;
    if (pScript->rgwOperand[2] == 0)
      iPlayerRole = wEventObjectID;
    else
      iPlayerRole = pScript->rgwOperand[2] - 1;

    p[pScript->rgwOperand[0] * MAX_PLAYER_ROLES + iPlayerRole] += (SHORT)pScript->rgwOperand[1];
  } break;

  case 0x001A: {
    // Set player's stat
    WORD *p;
    // 装备过程中, 修改装备附加属性, 否则直接修改玩家属性
    if (g_iCurEquipPart != -1)
      p = (WORD *)&(gEquipmentEffect[g_iCurEquipPart]);
    else
      p = (WORD *)(&PLAYER); // HACKHACK

    int iPlayerRole;
    if (pScript->rgwOperand[2] == 0)
      iPlayerRole = wEventObjectID;
    else
      iPlayerRole = pScript->rgwOperand[2] - 1;

    p[pScript->rgwOperand[0] * MAX_PLAYER_ROLES + iPlayerRole] = (SHORT)pScript->rgwOperand[1];
  } break;

  case 0x001B:
    // 0x001B: Increase/decrease player's HP
  case 0x001C:
    // 0x001C: Increase/decrease player's MP
  case 0x001D: {
    // 0x001D: Increase/decrease player's HP and MP
    SHORT val = (SHORT)pScript->rgwOperand[1];
    SHORT sHP = (pScript->wOperation != 0x001C) ? val : 0;
    SHORT sMP = (pScript->wOperation != 0x001B) ? val : 0;
    if (pScript->rgwOperand[0]) {
      // Apply to everyone
      g_fScriptSuccess = FALSE;
      for (int i = 0; i <= g.wMaxPartyMemberIndex; i++)
        if (PAL_IncreaseHPMP(PARTY_PLAYER(i), sHP, sMP))
          g_fScriptSuccess = TRUE;
    } else {
      // Apply to one player. The wEventObjectID parameter should indicate the player role.
      g_fScriptSuccess = FALSE;
      if (PAL_IncreaseHPMP(wEventObjectID, sHP, sMP))
        g_fScriptSuccess = TRUE;
    }
  } break;

  case 0x001E: {
    // Increase or decrease cash by the specified amount
    SHORT amount = (SHORT)(pScript->rgwOperand[0]);
    if (amount < 0 && g.dwCash < -amount) // not enough cash
      wScriptEntry = pScript->rgwOperand[1] - 1;
    else
      g.dwCash += amount;
  } break;

  case 0x001F:
    // Add item to inventory
    PAL_AddItemToInventory(pScript->rgwOperand[0], (SHORT)(pScript->rgwOperand[1]));
    break;

  case 0x0020: {
    // Remove item from inventory
    int objID = pScript->rgwOperand[0];
    int amount = pScript->rgwOperand[1];
    if (amount == 0)
      amount = 1;
    if (amount <= PAL_CountItem(objID) || pScript->rgwOperand[2] == 0) {
      int owe = PAL_AddItemToInventory(objID, -amount);
      if (owe <= 0) {
        // Check if it's a partial removal and set the remaining amount
        if (owe < 0)
          amount = -owe;
        // Try removing equipped item
        for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
          WORD w = PARTY_PLAYER(i);
          for (int j = 0; j < MAX_PLAYER_EQUIPMENTS; j++) {
            if (PLAYER.rgwEquipment[j][w] == objID) {
              PAL_RemoveEquipmentEffect(w, j);
              PLAYER.rgwEquipment[j][w] = 0;
              if (--amount == 0) {
                i = 9999;
                break;
              }
            }
          }
        }
      }
    } else
      wScriptEntry = pScript->rgwOperand[2] - 1;
  } break;

  case 0x0021:
    // Inflict damage to the enemy
    if (pScript->rgwOperand[0]) {
      // Inflict damage to all enemies
      for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++)
        if (BATTLE_ENEMY[i].wObjectID != 0)
          BATTLE_ENEMY[i].e.wHealth -= pScript->rgwOperand[1];
    } else {
      // Inflict damage to one enemy
      BATTLE_ENEMY[wEventObjectID].e.wHealth -= pScript->rgwOperand[1];
    }
    break;

  case 0x0022:
    // Revive player
    if (pScript->rgwOperand[0]) {
      // Apply to everyone
      g_fScriptSuccess = FALSE;
      for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
        WORD w = PARTY_PLAYER(i);
        if (PLAYER.rgwHP[w] == 0) {
          PLAYER.rgwHP[w] = PLAYER.rgwMaxHP[w] * pScript->rgwOperand[1] / 10;
          PAL_CurePoisonByLevel(w, 3);
          for (int x = 0; x < kStatusAll; x++)
            PAL_RemovePlayerStatus(w, x);
          g_fScriptSuccess = TRUE;
        }
      }
    } else {
      // Apply to one player
      g_fScriptSuccess = FALSE;
      WORD w = wEventObjectID;
      if (PLAYER.rgwHP[w] == 0) {
        PLAYER.rgwHP[w] = PLAYER.rgwMaxHP[w] * pScript->rgwOperand[1] / 10;
        PAL_CurePoisonByLevel(w, 3);
        for (int x = 0; x < kStatusAll; x++)
          PAL_RemovePlayerStatus(w, x);
        g_fScriptSuccess = TRUE;
      }
    }
    break;

  case 0x0023: {
    // Remove equipment from the specified player
    int iPlayerRole = pScript->rgwOperand[0];
    if (pScript->rgwOperand[1] == 0) {
      // Remove all equipments
      for (int i = 0; i < MAX_PLAYER_EQUIPMENTS; i++) {
        WORD w = PLAYER.rgwEquipment[i][iPlayerRole];
        if (w != 0) {
          PAL_AddItemToInventory(w, 1);
          PLAYER.rgwEquipment[i][iPlayerRole] = 0;
        }
        PAL_RemoveEquipmentEffect(iPlayerRole, i);
      }
    } else {
      WORD w = PLAYER.rgwEquipment[pScript->rgwOperand[1] - 1][iPlayerRole];
      if (w != 0) {
        PAL_RemoveEquipmentEffect(iPlayerRole, pScript->rgwOperand[1] - 1);
        PAL_AddItemToInventory(w, 1);
        PLAYER.rgwEquipment[pScript->rgwOperand[1] - 1][iPlayerRole] = 0;
      }
    }
  } break;

  case 0x0024:
    // Set the autoscript entry address for an event object
    if (pScript->rgwOperand[0] != 0)
      pCurrent->wAutoScript = pScript->rgwOperand[1];
    break;

  case 0x0025:
    // Set the trigger script entry address for an event object
    if (pScript->rgwOperand[0] != 0)
      pCurrent->wTriggerScript = pScript->rgwOperand[1];
    break;

  case 0x0026:
    // Show the buy item menu
    PAL_MakeScene();
    VIDEO_UpdateScreen(NULL);
    PAL_BuyMenu(pScript->rgwOperand[0]);
    break;

  case 0x0027:
    // Show the sell item menu
    PAL_MakeScene();
    VIDEO_UpdateScreen(NULL);
    PAL_SellMenu();
    break;

  case 0x0028: {
    // Apply poison to enemy
    BOOL applyToAll = pScript->rgwOperand[0];
    for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
      WORD w = BATTLE_ENEMY[i].wObjectID;
      if (w == 0)
        continue;
      if (!applyToAll && i != wEventObjectID)
        continue;
      if (RandomLong(0, 9) >= OBJECT[w].enemy.wResistanceToSorcery) {
        int j;
        for (j = 0; j < MAX_POISONS; j++)
          if (BATTLE_ENEMY[i].rgPoisons[j].wPoisonID == pScript->rgwOperand[1])
            break;
        if (j >= MAX_POISONS) {
          for (j = 0; j < MAX_POISONS; j++) {
            if (BATTLE_ENEMY[i].rgPoisons[j].wPoisonID == 0) {
              BATTLE_ENEMY[i].rgPoisons[j].wPoisonID = pScript->rgwOperand[1];
              BATTLE_ENEMY[i].rgPoisons[j].wPoisonScript =
                  PAL_RunTriggerScript(OBJECT[pScript->rgwOperand[1]].poison.wEnemyScript, wEventObjectID);
              break;
            }
          }
        }
      }
    }
  } break;

  case 0x0029: {
    // Apply poison to player
    BOOL applyToAll = pScript->rgwOperand[0];
    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
      WORD w = PARTY_PLAYER(i);
      if (!applyToAll && w != wEventObjectID)
        continue;
      if (RandomLong(1, 100) > PAL_GetPlayerPoisonResistance(w))
        PAL_AddPoisonForPlayer(w, pScript->rgwOperand[1]);
    }
  } break;

  case 0x002A: {
    // Cure poison by object ID for enemy
    BOOL applyToAll = pScript->rgwOperand[0];
    for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
      if (BATTLE_ENEMY[i].wObjectID == 0)
        continue;
      if (!applyToAll && i != wEventObjectID)
        continue;
      for (int j = 0; j < MAX_POISONS; j++) {
        if (BATTLE_ENEMY[i].rgPoisons[j].wPoisonID == pScript->rgwOperand[1]) {
          BATTLE_ENEMY[i].rgPoisons[j].wPoisonID = 0;
          BATTLE_ENEMY[i].rgPoisons[j].wPoisonScript = 0;
          break;
        }
      }
    }
  } break;

  case 0x002B: {
    // Cure poison by object ID for player
    BOOL applyToAll = pScript->rgwOperand[0];
    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
      WORD w = PARTY_PLAYER(i);
      if (!applyToAll && w != wEventObjectID)
        continue;
      PAL_CurePoisonByKind(w, pScript->rgwOperand[1]);
    }
  } break;

  case 0x002C: {
    // Cure poisons by level
    BOOL applyToAll = pScript->rgwOperand[0];
    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
      WORD w = PARTY_PLAYER(i);
      if (!applyToAll && w != wEventObjectID)
        continue;
      PAL_CurePoisonByLevel(w, pScript->rgwOperand[1]);
    }
  } break;

  case 0x002D:
    // Set the status for player
    if (!PAL_SetPlayerStatus(wEventObjectID, pScript->rgwOperand[0], pScript->rgwOperand[1])) {
      g_fScriptSuccess = FALSE;
    }
    break;

  case 0x002E:
    // Set the status for enemy
    if (RandomLong(0, 9) > OBJECT[BATTLE_ENEMY[wEventObjectID].wObjectID].enemy.wResistanceToSorcery) {
      BATTLE_ENEMY[wEventObjectID].rgwStatus[pScript->rgwOperand[0]] = pScript->rgwOperand[1];
    } else {
      wScriptEntry = pScript->rgwOperand[2] - 1;
    }
    break;

  case 0x002F:
    // Remove player's status
    PAL_RemovePlayerStatus(wEventObjectID, pScript->rgwOperand[0]);
    break;

  case 0x0030: {
    // Increase player's stat temporarily by percent
    WORD *ptmp = (WORD *)(&gEquipmentEffect[kBodyPartExtra]); // HACKHACK
    WORD *p = (WORD *)(&PLAYER);                              // HACKHACK

    int iPlayerRole;
    if (pScript->rgwOperand[2] == 0)
      iPlayerRole = wEventObjectID;
    else
      iPlayerRole = pScript->rgwOperand[2] - 1;

    ptmp[pScript->rgwOperand[0] * MAX_PLAYER_ROLES + iPlayerRole] =
        p[pScript->rgwOperand[0] * MAX_PLAYER_ROLES + iPlayerRole] * (SHORT)pScript->rgwOperand[1] / 100;
  } break;

  case 0x0031:
    // Change battle sprite temporarily for player (变蛇)
    gEquipmentEffect[kBodyPartExtra].rgwSpriteNumInBattle[wEventObjectID] = pScript->rgwOperand[0];
    break;

  case 0x0033:
    // collect the enemy for items
    if (BATTLE_ENEMY[wEventObjectID].e.wCollectValue != 0) {
      g.wCollectValue += BATTLE_ENEMY[wEventObjectID].e.wCollectValue;
    } else {
      wScriptEntry = pScript->rgwOperand[0] - 1;
    }
    break;

  case 0x0034:
    // Transform collected enemies into items
    if (!PAL_collectItem()) {
      wScriptEntry = pScript->rgwOperand[0] - 1;
    }
    break;

  case 0x0035:
    // Shake the screen
    VIDEO_ShakeScreen(pScript->rgwOperand[0], pScript->rgwOperand[1] ? pScript->rgwOperand[1] : 4);
    if (!pScript->rgwOperand[0])
      VIDEO_UpdateScreen(NULL);
    break;

  case 0x0036:
    // Set the current playing RNG animation
    g_iCurPlayingRNG = pScript->rgwOperand[0];
    break;

  case 0x0037:
    // Play RNG animation
    PAL_RNGPlay(g_iCurPlayingRNG, pScript->rgwOperand[0], pScript->rgwOperand[1] > 0 ? pScript->rgwOperand[1] : -1,
                pScript->rgwOperand[2] > 0 ? pScript->rgwOperand[2] : 16);
    break;

  case 0x0038:
    // Teleport the party out of the scene
    if (!gInBattle && g.scenes[g.wCurScene - 1].wScriptOnLeave != 0) {
      PAL_RunTriggerScript(g.scenes[g.wCurScene - 1].wScriptOnLeave, 0xFFFF);
    } else {
      // failed
      g_fScriptSuccess = FALSE;
      wScriptEntry = pScript->rgwOperand[0] - 1;
    }
    break;

  case 0x0039: {
    // Drain HP from enemy
    BATTLE_ENEMY[wEventObjectID].e.wHealth -= pScript->rgwOperand[0];
    WORD w = PARTY_PLAYER(g_Battle.wMovingPlayerIndex);
    PLAYER.rgwHP[w] += pScript->rgwOperand[0];
    if (PLAYER.rgwHP[w] > PLAYER.rgwMaxHP[w])
      PLAYER.rgwHP[w] = PLAYER.rgwMaxHP[w];
  } break;

  case 0x003A:
    // Player flee from the battle
    if (g_Battle.fIsBoss) {
      // Cannot flee from bosses
      wScriptEntry = pScript->rgwOperand[0] - 1;
    } else {
      PAL_BattlePlayerEscape();
    }
    break;

  case 0x003F:
    // Ride the event object to the specified position, at a low speed
    PAL_PartyRideEventObject(wEventObjectID, pScript->rgwOperand[0], pScript->rgwOperand[1], pScript->rgwOperand[2], 2);
    break;

  case 0x0040:
    // set the trigger method for a event object
    if (pScript->rgwOperand[0] != 0)
      pCurrent->wTriggerMode = pScript->rgwOperand[1];
    break;

  case 0x0041:
    // Mark the script as failed
    g_fScriptSuccess = FALSE;
    break;

  case 0x0042:
    // Simulate a magic for player
    assert(pScript->rgwOperand[2] == 0);
    PAL_BattleSimulateMagic(wEventObjectID, pScript->rgwOperand[0], pScript->rgwOperand[1]);
    break;

  case 0x0043:
    // Set background music
    g.wCurMusic = pScript->rgwOperand[0];
    AUDIO_PlayMusic(pScript->rgwOperand[0],
                    /*loop=*/pScript->rgwOperand[1] != 1,
                    /*fade_time=*/pScript->rgwOperand[1] == 3 ? 3.0f : 0.0f);
    break;

  case 0x0044:
    // Ride the event object to the specified position, at the normal speed
    PAL_PartyRideEventObject(wEventObjectID, pScript->rgwOperand[0], pScript->rgwOperand[1], pScript->rgwOperand[2], 4);
    break;

  case 0x0045:
    // Set battle music
    g.wCurBattleMusic = pScript->rgwOperand[0];
    break;

  case 0x0046: {
    // Set the party position on the map
    int xOffset = ((g.wPartyDirection == kDirWest || g.wPartyDirection == kDirSouth) ? 16 : -16);
    int yOffset = ((g.wPartyDirection == kDirWest || g.wPartyDirection == kDirNorth) ? 8 : -8);

    g.viewport = PAL_XY(pScript->rgwOperand[0] * 32 + pScript->rgwOperand[2] * 16 - PAL_X(gPartyOffset),
                        pScript->rgwOperand[1] * 16 + pScript->rgwOperand[2] * 8 - PAL_Y(gPartyOffset));

    int x = PAL_X(gPartyOffset);
    int y = PAL_Y(gPartyOffset);

    for (int i = 0; i < MAX_PLAYABLE_PLAYER_ROLES; i++) {
      PARTY[i].x = x;
      PARTY[i].y = y;
      TRAIL[i].x = x + PAL_X(g.viewport);
      TRAIL[i].y = y + PAL_Y(g.viewport);
      TRAIL[i].wDirection = g.wPartyDirection;

      x += xOffset;
      y += yOffset;
    }
  } break;

  case 0x0047:
    // Play sound effect
    AUDIO_PlaySound(pScript->rgwOperand[0]);
    break;

  case 0x0049:
    // Set the state of event object
    if (pScript->rgwOperand[0] != 0)
      pCurrent->sState = pScript->rgwOperand[1];
    break;

  case 0x004A:
    // Set the current battlefield
    g.wCurBattleField = pScript->rgwOperand[0];
    break;

  case 0x004B:
    // Nullify the event object for a short while
    pEvtObj->sVanishTime = -15;
    break;

  case 0x004C: {
    // chase the player
    int chaseRange = pScript->rgwOperand[0];
    if (chaseRange == 0)
      chaseRange = 8;
    int speed = pScript->rgwOperand[1];
    if (speed == 0)
      speed = 4;
    BOOL floating = pScript->rgwOperand[2];
    PAL_MonsterChasePlayer(wEventObjectID, speed, chaseRange, floating);
  } break;

  case 0x004E:
    // Load the last saved game
    PAL_FadeOut(1);
    PAL_ReloadInNextTick(g.saveSlot);
    return 0; // don't go further

  case 0x004F:
    // Fade the screen to red color (game over)
    PAL_FadeToRed();
    break;

  case 0x0050:
    // screen fade out
    VIDEO_UpdateScreen(NULL);
    PAL_FadeOut(pScript->rgwOperand[0] ? pScript->rgwOperand[0] : 1);
    gNeedToFadeIn = TRUE;
    break;

  case 0x0051:
    // screen fade in
    VIDEO_UpdateScreen(NULL);
    PAL_FadeIn(gCurPalette, gNightPalette, ((SHORT)(pScript->rgwOperand[0]) > 0) ? pScript->rgwOperand[0] : 1);
    gNeedToFadeIn = FALSE;
    break;

  case 0x0052:
    // hide the event object for a while, default 800 frames
    pEvtObj->sState *= -1;
    pEvtObj->sVanishTime = (pScript->rgwOperand[0] ? pScript->rgwOperand[0] : 800);
    break;

  case 0x0053:
    // use the day palette
    gNightPalette = FALSE;
    break;

  case 0x0054:
    // use the night palette
    gNightPalette = TRUE;
    break;

  case 0x0055: {
    // Add magic to a player
    WORD role = pScript->rgwOperand[1] ? pScript->rgwOperand[1] - 1 : wEventObjectID;
    PAL_AddMagic(role, pScript->rgwOperand[0]);
  } break;

  case 0x0056: {
    // Remove magic from a player
    WORD role = pScript->rgwOperand[1] ? pScript->rgwOperand[1] - 1 : wEventObjectID;
    PAL_RemoveMagic(role, pScript->rgwOperand[0]);
  } break;

  case 0x0057: {
    // Set the base damage of magic according to MP value
    int multiplier = (pScript->rgwOperand[1] == 0) ? 8 : pScript->rgwOperand[1];
    WORD magicNumber = OBJECT[pScript->rgwOperand[0]].magic.wMagicNumber;
    MAGIC[magicNumber].wBaseDamage = PLAYER.rgwMP[wEventObjectID] * multiplier;
    PLAYER.rgwMP[wEventObjectID] = 0;
  } break;

  case 0x0058:
    // Jump if there is less than the specified number of the specified items in the inventory
    if (PAL_GetItemAmount(pScript->rgwOperand[0]) < (SHORT)(pScript->rgwOperand[1]))
      wScriptEntry = pScript->rgwOperand[2] - 1;
    break;

  case 0x0059: {
    // Change to the specified scene
    int scene = pScript->rgwOperand[0];
    if (scene > 0 && scene <= MAX_SCENES && g.wCurScene != scene) {
      // Set data to load the scene in the next frame
      g.wCurScene = scene;
      PAL_SetLoadFlags(kLoadScene);
      gEnteringScene = TRUE;
      g.wLayer = 0;
    }
  } break;

  case 0x005A:
    // Halve the player's HP
    // The wEventObjectID parameter here should indicate the player role
    PLAYER.rgwHP[wEventObjectID] /= 2;
    break;

  case 0x005B: {
    // Halve the enemy's HP
    WORD damage = BATTLE_ENEMY[wEventObjectID].e.wHealth / 2 + 1;
    if (damage > pScript->rgwOperand[0])
      damage = pScript->rgwOperand[0];
    BATTLE_ENEMY[wEventObjectID].e.wHealth -= damage;
  } break;

  case 0x005C:
    // Hide for a while
    g_Battle.iHidingTime = -(INT)(pScript->rgwOperand[0]);
    break;

  case 0x005D:
    // Jump if player doesn't have the specified poison
    if (!PAL_IsPlayerPoisonedByKind(wEventObjectID, pScript->rgwOperand[0]))
      wScriptEntry = pScript->rgwOperand[1] - 1;
    break;

  case 0x005E: {
    // Jump if enemy doesn't have the specified poison
    int i;
    for (i = 0; i < MAX_POISONS; i++)
      if (BATTLE_ENEMY[wEventObjectID].rgPoisons[i].wPoisonID == pScript->rgwOperand[0])
        break;
    if (i >= MAX_POISONS)
      wScriptEntry = pScript->rgwOperand[1] - 1;
  } break;

  case 0x005F:
    // Kill the player immediately
    // The wEventObjectID parameter here should indicate the player role
    PLAYER.rgwHP[wEventObjectID] = 0;
    break;

  case 0x0060:
    // Immediate KO of the enemy
    BATTLE_ENEMY[wEventObjectID].e.wHealth = 0;
    break;

  case 0x0061:
    // Jump if player is not poisoned
    if (!PAL_IsPlayerPoisonedByLevel(wEventObjectID, 0))
      wScriptEntry = pScript->rgwOperand[0] - 1;
    break;

  case 0x0062:
    // Pause enemy chasing for a while
    g.wChasespeedChangeCycles = pScript->rgwOperand[0];
    g.wChaseRange = 0;
    break;

  case 0x0063:
    // Speed up enemy chasing for a while
    g.wChasespeedChangeCycles = pScript->rgwOperand[0];
    g.wChaseRange = 3;
    break;

  case 0x0064: {
    // Jump if enemy's HP is more than the specified percentage
    BATTLEENEMY *enemy = &BATTLE_ENEMY[wEventObjectID];
    int enemyID = OBJECT[enemy->wObjectID].enemy.wEnemyID;
    if (enemy->e.wHealth * 100 > ENEMY[enemyID].wHealth * pScript->rgwOperand[0])
      wScriptEntry = pScript->rgwOperand[1] - 1;
  } break;

  case 0x0065:
    // Set the player's sprite
    PLAYER.rgwSpriteNum[pScript->rgwOperand[0]] = pScript->rgwOperand[1];
    if (!gInBattle && pScript->rgwOperand[2]) {
      PAL_SetLoadFlags(kLoadPlayerSprite);
      PAL_LoadResources();
    }
    break;

  case 0x0066: {
    // Throw weapon to enemy
    int baseDamage = pScript->rgwOperand[1] * 5;
    baseDamage += PLAYER.rgwAttackStrength[PARTY_PLAYER(g_Battle.wMovingPlayerIndex)] * RandomLong(0, 3);
    PAL_BattleSimulateMagic((SHORT)wEventObjectID, pScript->rgwOperand[0], baseDamage);
  } break;

  case 0x0067:
    // Enemy use magic
    BATTLE_ENEMY[wEventObjectID].e.wMagic = pScript->rgwOperand[0];
    BATTLE_ENEMY[wEventObjectID].e.wMagicRate = (pScript->rgwOperand[1] == 0) ? 10 : pScript->rgwOperand[1];
    break;

  case 0x0068:
    // Jump if it's enemy's turn
    if (g_Battle.fEnemyMoving)
      wScriptEntry = pScript->rgwOperand[0] - 1;
    break;

  case 0x0069:
    // Enemy escape in battle
    PAL_BattleEnemyEscape();
    break;

  case 0x006A:
    // Steal from the enemy
    PAL_BattleStealFromEnemy(wEventObjectID, pScript->rgwOperand[0]);
    break;

  case 0x006B:
    // Blow away enemies
    g_Battle.iBlow = (SHORT)(pScript->rgwOperand[0]);
    break;

  case 0x006C:
    // Walk the NPC in one step
    pCurrent->x += (SHORT)(pScript->rgwOperand[1]);
    pCurrent->y += (SHORT)(pScript->rgwOperand[2]);
    PAL_NPCWalkOneStep(pScript->rgwOperand[0], 0);
    break;

  case 0x006D: {
    // Set the enter script and leave script for a scene
    int scene = pScript->rgwOperand[0];
    if (scene) {
      if (pScript->rgwOperand[1]) {
        g.scenes[scene - 1].wScriptOnEnter = pScript->rgwOperand[1];
      }
      if (pScript->rgwOperand[2]) {
        g.scenes[scene - 1].wScriptOnLeave = pScript->rgwOperand[2];
      }
      if (pScript->rgwOperand[1] == 0 && pScript->rgwOperand[2] == 0) {
        g.scenes[scene - 1].wScriptOnEnter = 0;
        g.scenes[scene - 1].wScriptOnLeave = 0;
      }
    }
  } break;

  case 0x006E:
    // Move the player to the specified position in one step
    for (int i = 3; i >= 0; i--)
      TRAIL[i + 1] = TRAIL[i];
    TRAIL[0].wDirection = g.wPartyDirection;
    TRAIL[0].x = PAL_X(g.viewport) + PAL_X(gPartyOffset);
    TRAIL[0].y = PAL_Y(g.viewport) + PAL_Y(gPartyOffset);

    g.viewport = PAL_XY(PAL_X(g.viewport) + (SHORT)(pScript->rgwOperand[0]),
                        PAL_Y(g.viewport) + (SHORT)(pScript->rgwOperand[1]));

    g.wLayer = pScript->rgwOperand[2] * 8;

    if (pScript->rgwOperand[0] != 0 || pScript->rgwOperand[1] != 0)
      PAL_UpdatePartyGestures(TRUE);
    break;

  case 0x006F:
    // Sync the state of current event object with another event object
    if (pCurrent->sState == (SHORT)(pScript->rgwOperand[1]))
      pEvtObj->sState = (SHORT)(pScript->rgwOperand[1]);
    break;

  case 0x0070:
    // Walk the party to the specified position
    PAL_PartyWalkTo(pScript->rgwOperand[0], pScript->rgwOperand[1], pScript->rgwOperand[2], 2);
    break;

  case 0x0071:
    // Wave the screen
    g.wScreenWave = pScript->rgwOperand[0];
    gWaveProgression = (SHORT)(pScript->rgwOperand[1]);
    break;

  case 0x0073:
    // Fade the screen to scene
    VIDEO_BackupScreen(gpScreen);
    PAL_MakeScene();
    VIDEO_FadeScreen(pScript->rgwOperand[0]);
    break;

  case 0x0074:
    // Jump if not all players are full HP
    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
      WORD w = PARTY_PLAYER(i);
      if (PLAYER.rgwHP[w] < PLAYER.rgwMaxHP[w]) {
        wScriptEntry = pScript->rgwOperand[0] - 1;
        break;
      }
    }
    break;

  case 0x0075:
    // Set the player party
    g.wMaxPartyMemberIndex = 0;
    for (int i = 0; i < 3; i++) {
      if (pScript->rgwOperand[i] != 0) {
        PARTY_PLAYER(g.wMaxPartyMemberIndex) = pScript->rgwOperand[i] - 1;
        g.wMaxPartyMemberIndex++;
      }
    }
    g.wMaxPartyMemberIndex--;

    // Reload the player sprites
    PAL_SetLoadFlags(kLoadPlayerSprite);
    PAL_LoadResources();

    memset(g.rgPoisonStatus, 0, sizeof(g.rgPoisonStatus));
    PAL_UpdateEquipmentEffect();
    break;

  case 0x0076:
    // Show FBP picture
    VIDEO_FillScreenBlack();
    VIDEO_UpdateScreen(NULL);
    break;

  case 0x0077:
    // Stop current playing music
    AUDIO_PlayMusic(0, FALSE, (pScript->rgwOperand[0] == 0) ? 2.0f : (FLOAT)(pScript->rgwOperand[0]) * 3);
    g.wCurMusic = 0;
    break;

  case 0x0078:
    // 战后返回地图
    break;

  case 0x0079:
    // Jump if the specified player is in the party
    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
      if (PLAYER.rgwName[PARTY_PLAYER(i)] == pScript->rgwOperand[0]) {
        wScriptEntry = pScript->rgwOperand[1] - 1;
        break;
      }
    }
    break;

  case 0x007A:
    // Walk the party to the specified position, at a higher speed
    PAL_PartyWalkTo(pScript->rgwOperand[0], pScript->rgwOperand[1], pScript->rgwOperand[2], 4);
    break;

  case 0x007B:
    // Walk the party to the specified position, at the highest speed
    PAL_PartyWalkTo(pScript->rgwOperand[0], pScript->rgwOperand[1], pScript->rgwOperand[2], 8);
    break;

  case 0x007C:
    // Walk straight to the specified position
    if ((wEventObjectID & 1) ^ (gFrameNum & 1)) {
      if (!PAL_NPCWalkTo(wEventObjectID, pScript->rgwOperand[0], pScript->rgwOperand[1], pScript->rgwOperand[2], 4))
        wScriptEntry--;
    } else {
      wScriptEntry--;
    }
    break;

  case 0x007D:
    // Move the event object
    pCurrent->x += (SHORT)(pScript->rgwOperand[1]);
    pCurrent->y += (SHORT)(pScript->rgwOperand[2]);
    break;

  case 0x007E:
    // Set the layer of event object
    pCurrent->sLayer = (SHORT)(pScript->rgwOperand[1]);
    break;

  case 0x007F:
    // Move the viewport
    PAL_MoveViewport(pScript->rgwOperand[0], pScript->rgwOperand[1], pScript->rgwOperand[2]);
    break;

  case 0x0080:
    // Toggle day/night palette
    gNightPalette = !(gNightPalette);
    PAL_PaletteFade(gCurPalette, gNightPalette, !(pScript->rgwOperand[0]));
    break;

  case 0x0081: {
    // Jump if the player is not facing the specified event object
    WORD evtObjID = pScript->rgwOperand[0];
    WORD dist = pScript->rgwOperand[1];
    if (evtObjID <= SCENE_EVENT_OBJ_IDX_ST || evtObjID > SCENE_EVENT_OBJ_IDX_ED) {
      // The event object is not in the current scene
      wScriptEntry = pScript->rgwOperand[2] - 1;
      g_fScriptSuccess = FALSE;
      break;
    }

    int x = pCurrent->x;
    int y = pCurrent->y;
    x += ((g.wPartyDirection == kDirWest || g.wPartyDirection == kDirSouth) ? 16 : -16);
    y += ((g.wPartyDirection == kDirWest || g.wPartyDirection == kDirNorth) ? 8 : -8);
    x -= PAL_X(g.viewport) + PAL_X(gPartyOffset);
    y -= PAL_Y(g.viewport) + PAL_Y(gPartyOffset);

    if (abs(x) + abs(y * 2) < dist * 32 + 16 && pCurrent->sState > 0) {
      if (dist > 0) {
        // Change the trigger mode so that the object can be triggered in next frame
        pCurrent->wTriggerMode = kTriggerTouchNormal + dist;
      }
    } else {
      wScriptEntry = pScript->rgwOperand[2] - 1;
      g_fScriptSuccess = FALSE;
    }
  } break;

  case 0x0082:
    // Walk straight to the specified position, at a high speed
    if (!PAL_NPCWalkTo(wEventObjectID, pScript->rgwOperand[0], pScript->rgwOperand[1], pScript->rgwOperand[2], 8)) {
      wScriptEntry--;
    }
    break;

  case 0x0083: {
    // Jump if event object is not in the specified zone of the current event object
    WORD evtObjID = pScript->rgwOperand[0];
    WORD dist = pScript->rgwOperand[1];
    if (evtObjID <= SCENE_EVENT_OBJ_IDX_ST || evtObjID > SCENE_EVENT_OBJ_IDX_ED) {
      // The event object is not in the current scene
      wScriptEntry = pScript->rgwOperand[2] - 1;
      g_fScriptSuccess = FALSE;
      break;
    }
    int x = pEvtObj->x - pCurrent->x;
    int y = pEvtObj->y - pCurrent->y;
    if (abs(x) + abs(y * 2) >= dist * 32 + 16) {
      wScriptEntry = pScript->rgwOperand[2] - 1;
      g_fScriptSuccess = FALSE;
    }
  } break;

  case 0x0084: {
    // Place the item which player used as an event object to the scene
    WORD evtObjID = pScript->rgwOperand[0];
    if (evtObjID <= SCENE_EVENT_OBJ_IDX_ST || evtObjID > SCENE_EVENT_OBJ_IDX_ED) {
      // The event object is not in the current scene
      wScriptEntry = pScript->rgwOperand[2] - 1;
      g_fScriptSuccess = FALSE;
      break;
    }

    int x = PAL_X(g.viewport) + PAL_X(gPartyOffset);
    int y = PAL_Y(g.viewport) + PAL_Y(gPartyOffset);
    x += ((g.wPartyDirection == kDirWest || g.wPartyDirection == kDirSouth) ? -16 : 16);
    y += ((g.wPartyDirection == kDirWest || g.wPartyDirection == kDirNorth) ? -8 : 8);

    if (PAL_CheckObstacle(PAL_XY(x, y), FALSE, 0)) {
      wScriptEntry = pScript->rgwOperand[2] - 1;
      g_fScriptSuccess = FALSE;
    } else {
      // place item here
      pCurrent->x = x;
      pCurrent->y = y;
      pCurrent->sState = (SHORT)(pScript->rgwOperand[1]);
    }
  } break;

  case 0x0085:
    // Delay for a period
    UTIL_Delay(pScript->rgwOperand[0] * 80);
    break;

  case 0x0086: {
    // Jump if the specified item is not equipped
    int count = 0;
    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
      WORD w = PARTY_PLAYER(i);
      for (int x = 0; x < MAX_PLAYER_EQUIPMENTS; x++)
        if (PLAYER.rgwEquipment[x][w] == pScript->rgwOperand[0])
          count++;
    }
    if (count < pScript->rgwOperand[1])
      wScriptEntry = pScript->rgwOperand[2] - 1;
    break;
  }

  case 0x0087:
    // Animate the event object
    PAL_NPCWalkOneStep(wEventObjectID, 0);
    break;

  case 0x0088: {
    // Set the base damage of magic according to amount of money
    int i = ((g.dwCash > 5000) ? 5000 : g.dwCash);
    g.dwCash -= i;
    int j = OBJECT[pScript->rgwOperand[0]].magic.wMagicNumber;
    MAGIC[j].wBaseDamage = i * 2 / 5;
    break;
  }

  case 0x0089:
    // Set the battle result
    g_Battle.BattleResult = pScript->rgwOperand[0];
    break;

  case 0x008A:
    // Enable Auto-Battle for next battle
    gAutoBattle = TRUE;
    break;

  case 0x008B:
    // change the current palette
    gCurPalette = pScript->rgwOperand[0];
    if (!gNeedToFadeIn)
      PAL_SetPalette(gCurPalette, FALSE);
    break;

  case 0x008C:
    // Fade from/to color
    PAL_ColorFade(pScript->rgwOperand[1], (BYTE)(pScript->rgwOperand[0]), pScript->rgwOperand[2]);
    gNeedToFadeIn = FALSE;
    break;

  case 0x008D:
    // Increase player's level
    PAL_PlayerLevelUp(wEventObjectID, pScript->rgwOperand[0]);
    break;

  case 0x008F:
    // Halve the cash amount
    g.dwCash /= 2;
    break;

  case 0x0090:
    // Set the object script (没什么用?)
    break;

  case 0x0091: {
    // Jump if the enemy is not first of same kind
    int pos = 0;
    if (gInBattle) {
      for (int i = 0, count = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
        if (BATTLE_ENEMY[i].wObjectID == BATTLE_ENEMY[wEventObjectID].wObjectID) {
          count++;
          if (i == wEventObjectID)
            pos = count;
        }
      }
    }
    if (pos > 1)
      wScriptEntry = pScript->rgwOperand[0] - 1;
  } break;

  case 0x0092:
    // Show a magic-casting animation for a player in battle
    if (gInBattle) {
      int playerIndex = pScript->rgwOperand[0] - 1;
      PAL_BattleShowPlayerPreMagicAnim(playerIndex, FALSE);
      BATTLE_PLAYER[playerIndex].wCurrentFrame = 6;

      for (int i = 0; i < 5; i++) {
        for (int j = 0; j <= g.wMaxPartyMemberIndex; j++)
          BATTLE_PLAYER[j].iColorShift = i * 2;
        PAL_BattleDelay(1, 0, TRUE);
      }

      VIDEO_BackupScreen(g_Battle.lpSceneBuf);
      PAL_BattleUpdateFighters();
      PAL_BattleMakeScene();
      PAL_BattleFadeScene();
    }
    break;

  case 0x0093:
    // Fade the screen. Update scene in the process.
    PAL_SceneFade(gCurPalette, gNightPalette, (SHORT)(pScript->rgwOperand[0]));
    gNeedToFadeIn = ((SHORT)(pScript->rgwOperand[0]) < 0);
    break;

  case 0x0094:
    // Jump if the state of event object is the specified one
    if (pCurrent->sState == (SHORT)(pScript->rgwOperand[1]))
      wScriptEntry = pScript->rgwOperand[2] - 1;
    break;

  case 0x0095:
    // Jump if the current scene is the specified one
    if (g.wCurScene == pScript->rgwOperand[0])
      wScriptEntry = pScript->rgwOperand[1] - 1;
    break;

  case 0x0097:
    // Ride the event object to the specified position, at a higher speed
    PAL_PartyRideEventObject(wEventObjectID, pScript->rgwOperand[0], pScript->rgwOperand[1], pScript->rgwOperand[2], 8);
    break;

  case 0x0098: {
    // Set follower of the party
    g.nFollower = 0;
    for (int i = 0; i < 2; i++) {
      if (pScript->rgwOperand[i] > 0) {
        int curFollower = i + 1;
        g.nFollower = curFollower;
        PARTY_PLAYER(g.wMaxPartyMemberIndex + curFollower) = pScript->rgwOperand[i];

        PAL_SetLoadFlags(kLoadPlayerSprite);
        PAL_LoadResources();

        // Update the position and gesture for the follower
        PARTY[g.wMaxPartyMemberIndex + curFollower].x = TRAIL[3 + i].x - PAL_X(g.viewport);
        PARTY[g.wMaxPartyMemberIndex + curFollower].y = TRAIL[3 + i].y - PAL_Y(g.viewport);
        PARTY[g.wMaxPartyMemberIndex + curFollower].wFrame = TRAIL[3 + i].wDirection * 3;
      }
    }
  } break;

  case 0x0099:
    // Change the map for the specified scene
    assert(pScript->rgwOperand[0] == 0xFFFF);
    g.scenes[g.wCurScene - 1].wMapNum = pScript->rgwOperand[1];
    PAL_SetLoadFlags(kLoadScene);
    PAL_LoadResources();
    break;

  case 0x009A:
    // Set the state for multiple event objects
    for (int i = pScript->rgwOperand[0]; i <= pScript->rgwOperand[1]; i++)
      g.events[i - 1].sState = pScript->rgwOperand[2];
    break;

  case 0x009B:
    // Fade to the current scene
    assert(pScript->rgwOperand[0] == 2);
    assert(pScript->rgwOperand[1] == 0xFFFF);
    VIDEO_BackupScreen(gpScreen);
    PAL_MakeScene();
    VIDEO_FadeScreen(2);
    break;

  case 0x009C:
    // Enemy division itself
    if (!PAL_EnemyDivisionItself(pScript->rgwOperand[0], wEventObjectID)) {
      if (pScript->rgwOperand[1])
        wScriptEntry = pScript->rgwOperand[1] - 1;
    }
    break;

  case 0x009E:
    // Enemy summons another monster
    if (!PAL_EnemySummonMonster(pScript->rgwOperand[0], pScript->rgwOperand[1], wEventObjectID)) {
      if (pScript->rgwOperand[2])
        wScriptEntry = pScript->rgwOperand[2] - 1;
    }
    break;

  case 0x009F:
    // Enemy transforms into something else
    if (g_Battle.iHidingTime <= 0 && BATTLE_ENEMY[wEventObjectID].rgwStatus[kStatusSleep] == 0 &&
        BATTLE_ENEMY[wEventObjectID].rgwStatus[kStatusParalyzed] == 0 &&
        BATTLE_ENEMY[wEventObjectID].rgwStatus[kStatusConfused] == 0) {
      WORD prevHp = BATTLE_ENEMY[wEventObjectID].e.wHealth;

      BATTLE_ENEMY[wEventObjectID].wObjectID = pScript->rgwOperand[0];
      BATTLE_ENEMY[wEventObjectID].e = ENEMY[OBJECT[pScript->rgwOperand[0]].enemy.wEnemyID];

      BATTLE_ENEMY[wEventObjectID].e.wHealth = prevHp;
      BATTLE_ENEMY[wEventObjectID].wCurrentFrame = 0;

      for (int i = 0; i < 6; i++) {
        BATTLE_ENEMY[wEventObjectID].iColorShift = i;
        PAL_BattleDelay(1, 0, FALSE);
      }

      BATTLE_ENEMY[wEventObjectID].iColorShift = 0;

      AUDIO_PlaySound(47);
      VIDEO_BackupScreen(g_Battle.lpSceneBuf);
      PAL_LoadBattleSprites();
      PAL_BattleMakeScene();
      PAL_BattleFadeScene();
    }
    break;

  case 0x00A0:
    // end game
    PAL_PlayAVI("4.avi");
    PAL_PlayAVI("5.avi");
    PAL_PlayAVI("6.avi");
    PAL_Shutdown(0);
    break;

  case 0x00A1:
    // Set the positions of all party members to the same as the first one
    for (int i = 0; i < MAX_PLAYABLE_PLAYER_ROLES; i++) {
      TRAIL[i].wDirection = g.wPartyDirection;
      TRAIL[i].x = PARTY[0].x + PAL_X(g.viewport);
      TRAIL[i].y = PARTY[0].y + PAL_Y(g.viewport);
    }
    for (int i = 1; i <= g.wMaxPartyMemberIndex; i++) {
      PARTY[i].x = PARTY[0].x;
      PARTY[i].y = PARTY[0].y - 1;
    }
    PAL_UpdatePartyGestures(FALSE);
    break;

  case 0x00A2:
    // Jump to one of the following instructions randomly
    wScriptEntry += RandomLong(0, pScript->rgwOperand[0] - 1);
    break;

  case 0x00A3:
    // Play CD music. Use the RIX music for fallback.
    g.wCurMusic = pScript->rgwOperand[1];
    AUDIO_PlayMusic(pScript->rgwOperand[1], TRUE, 0);
    break;

  default:
    TerminateOnError("SCRIPT: Invalid Instruction at %4x: (%4x - %4x, %4x, %4x)", wScriptEntry, pScript->wOperation,
                     pScript->rgwOperand[0], pScript->rgwOperand[1], pScript->rgwOperand[2]);
    break;
  }

  return wScriptEntry + 1;
}

WORD PAL_RunTriggerScript(WORD wScriptEntry, WORD wEventObjectID)
/*++
  Purpose:
    Runs a trigger script.

  Parameters:
    [IN]  wScriptEntry - The script entry to execute.
    [IN]  wEventObjectID - The event object ID which invoked the script.

  Return value:
    The entry point of the script.
--*/
{
  static WORD wLastEventObject = 0;

  extern BOOL g_fUpdatedInBattle; // HACKHACK
  g_fUpdatedInBattle = FALSE;

  if (wEventObjectID == 0xFFFF)
    wEventObjectID = wLastEventObject;
  wLastEventObject = wEventObjectID;

  LPEVENTOBJECT pEvtObj = NULL;
  if (wEventObjectID != 0)
    pEvtObj = &(g.events[wEventObjectID - 1]);

  BOOL fEnded = FALSE;
  WORD wNextScriptEntry = wScriptEntry;
  g_fScriptSuccess = TRUE;

  while (wScriptEntry != 0 && !fEnded) {
    LPSCRIPTENTRY pScript = &(SCRIPT[wScriptEntry]);

    switch (pScript->wOperation) {
    case 0x0000:
      // Stop running
      fEnded = TRUE;
      break;

    case 0x0001:
      // Stop running and replace the entry with the next line
      fEnded = TRUE;
      wNextScriptEntry = wScriptEntry + 1;
      break;

    case 0x0002:
      // Stop running and replace the entry with the specified one
      if (pScript->rgwOperand[1] == 0 || ++(pEvtObj->nScriptIdleFrame) < pScript->rgwOperand[1]) {
        fEnded = TRUE;
        wNextScriptEntry = pScript->rgwOperand[0];
      } else {
        // failed
        pEvtObj->nScriptIdleFrame = 0;
        wScriptEntry++;
      }
      break;

    case 0x0003:
      // unconditional jump
      if (pScript->rgwOperand[1] == 0 || ++(pEvtObj->nScriptIdleFrame) < pScript->rgwOperand[1]) {
        wScriptEntry = pScript->rgwOperand[0];
      } else {
        // failed
        pEvtObj->nScriptIdleFrame = 0;
        wScriptEntry++;
      }
      break;

    case 0x0004:
      // Call script
      PAL_RunTriggerScript(pScript->rgwOperand[0],
                           ((pScript->rgwOperand[1] == 0) ? wEventObjectID : pScript->rgwOperand[1]));
      wScriptEntry++;
      break;

    case 0x0005:
      // Redraw screen
      PAL_ClearDialog(TRUE);

      if (PAL_DialogIsPlayingRNG()) {
        VIDEO_RestoreScreen(gpScreen);
      } else if (gInBattle) {
        PAL_BattleMakeScene();
        VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);
        VIDEO_UpdateScreen(NULL);
      } else {
        if (pScript->rgwOperand[2])
          PAL_UpdatePartyGestures(FALSE);

        PAL_MakeScene();
        VIDEO_UpdateScreen(NULL);
        UTIL_Delay((pScript->rgwOperand[1] == 0) ? 60 : (pScript->rgwOperand[1] * 60));
      }

      wScriptEntry++;
      break;

    case 0x0006:
      // Jump to the specified address by the specified rate
      if (RandomLong(1, 100) >= pScript->rgwOperand[0]) {
        wScriptEntry = pScript->rgwOperand[1];
        continue;
      } else {
        wScriptEntry++;
      }
      break;

    case 0x0007: {
      // Start battle
      int result = PAL_StartBattle(pScript->rgwOperand[0], !pScript->rgwOperand[2]);
      if (result == kBattleResultLost && pScript->rgwOperand[1] != 0) {
        wScriptEntry = pScript->rgwOperand[1];
      } else if (result == kBattleResultFleed && pScript->rgwOperand[2] != 0) {
        wScriptEntry = pScript->rgwOperand[2];
      } else {
        wScriptEntry++;
      }
      gAutoBattle = FALSE;
    } break;

    case 0x0008:
      // Replace the entry with the next instruction
      wScriptEntry++;
      wNextScriptEntry = wScriptEntry;
      break;

    case 0x0009:
      // wait for the specified number of frames
      {
        PAL_ClearDialog(TRUE);
        DWORD time = PAL_GetTicks() + FRAME_TIME;
        for (int i = 0; i < (pScript->rgwOperand[0] ? pScript->rgwOperand[0] : 1); i++) {
          PAL_DelayUntil(time);
          time = PAL_GetTicks() + FRAME_TIME;

          if (pScript->rgwOperand[2])
            PAL_UpdatePartyGestures(FALSE);

          PAL_GameUpdate(pScript->rgwOperand[1] ? TRUE : FALSE);
          PAL_MakeScene();
          VIDEO_UpdateScreen(NULL);
        }
      }
      wScriptEntry++;
      break;

    case 0x000A:
      // Goto the specified address if player selected no
      PAL_ClearDialog(FALSE);

      if (!PAL_ConfirmMenu())
        wScriptEntry = pScript->rgwOperand[0];
      else
        wScriptEntry++;
      break;

    case 0x003B:
      // Show dialog in the middle part of the screen
      PAL_ClearDialog(TRUE);
      PAL_StartDialog(kDialogCenter, 0, 0, FALSE);
      wScriptEntry++;
      break;

    case 0x003C:
      // Show dialog in the upper part of the screen
      PAL_ClearDialog(TRUE);
      PAL_StartDialog(kDialogUpper, 0, pScript->rgwOperand[0], pScript->rgwOperand[2] ? TRUE : FALSE);
      wScriptEntry++;
      break;

    case 0x003D:
      // Show dialog in the lower part of the screen
      PAL_ClearDialog(TRUE);
      PAL_StartDialog(kDialogLower, 0, pScript->rgwOperand[0], pScript->rgwOperand[2] ? TRUE : FALSE);
      wScriptEntry++;
      break;

    case 0x003E:
      //
      // Show text in a window at the center of the screen
      //
      PAL_ClearDialog(TRUE);
      PAL_StartDialog(kDialogCenterWindow, 0, 0, FALSE);
      wScriptEntry++;
      break;

    case 0x008E:
      // Restore the screen
      PAL_ClearDialog(TRUE);
      VIDEO_RestoreScreen(gpScreen);
      VIDEO_UpdateScreen(NULL);
      wScriptEntry++;
      break;

    case 0xFFFF:
      // Print dialog text
      PAL_ShowDialogText(PAL_GetMsg(pScript->rgwOperand[0]));
      wScriptEntry++;
      break;

    default:
      PAL_ClearDialog(TRUE);
      wScriptEntry = PAL_InterpretInstruction(wScriptEntry, wEventObjectID);
      break;
    }
  }

  PAL_EndDialog();
  g_iCurEquipPart = -1;

  return wNextScriptEntry;
}

WORD PAL_RunAutoScript(WORD wScriptEntry, WORD wEventObjectID)
/*++
  Purpose:
    Runs the autoscript of the specified event object.

  Parameters:
    [IN]  wScriptEntry - The script entry to execute.
    [IN]  wEventObjectID - The event object ID which invoked the script.

  Return value:
    The address of the next script instruction to execute.
--*/
{
  while (true) {
    LPSCRIPTENTRY pScript = &(SCRIPT[wScriptEntry]);
    LPEVENTOBJECT pEvtObj = &(g.events[wEventObjectID - 1]);

    // For autoscript, we should interpret one instruction per frame (except jumping)
    // and save the address of next instruction.
    switch (pScript->wOperation) {
    case 0x0000:
      // Stop running
      break;

    case 0x0001:
      // Stop running and replace the entry with the next line
      wScriptEntry++;
      break;

    case 0x0002:
      // Stop running and replace the entry with the specified one
      if (pScript->rgwOperand[1] == 0 || ++(pEvtObj->wScriptIdleFrameCountAuto) < pScript->rgwOperand[1]) {
        wScriptEntry = pScript->rgwOperand[0];
      } else {
        pEvtObj->wScriptIdleFrameCountAuto = 0;
        wScriptEntry++;
      }
      break;

    case 0x0003:
      // unconditional jump
      if (pScript->rgwOperand[1] == 0 || ++(pEvtObj->wScriptIdleFrameCountAuto) < pScript->rgwOperand[1]) {
        wScriptEntry = pScript->rgwOperand[0];
        continue; // 继续解析
      } else {
        pEvtObj->wScriptIdleFrameCountAuto = 0;
        wScriptEntry++;
      }
      break;

    case 0x0004:
      // Call subroutine
      PAL_RunTriggerScript(pScript->rgwOperand[0], pScript->rgwOperand[1] ? pScript->rgwOperand[1] : wEventObjectID);
      wScriptEntry++;
      break;

    case 0x0006:
      // jump to the specified address by the specified rate
      if (RandomLong(1, 100) >= pScript->rgwOperand[0]) {
        if (pScript->rgwOperand[1] != 0) {
          wScriptEntry = pScript->rgwOperand[1];
          continue; // 继续解析
        } else {
          // 本轮停止执行，下次从当前行重新掷盅判断
        }
      } else {
        wScriptEntry++;
      }
      break;

    case 0x0009:
      // Wait for a certain number of frames
      if (++(pEvtObj->wScriptIdleFrameCountAuto) >= pScript->rgwOperand[0]) {
        // waiting ended; go further
        pEvtObj->wScriptIdleFrameCountAuto = 0;
        wScriptEntry++;
      }
      break;

    case 0xFFFF: {
      int line = wEventObjectID;
      int XBase = (line & PAL_ITEM_DESC_BOTTOM) ? 71 : 102;
      int YBase = (line & PAL_ITEM_DESC_BOTTOM) ? 151 : 3;
      int iDescLine = (line & ~PAL_ITEM_DESC_BOTTOM);
      LPCWSTR msg = PAL_GetMsg(pScript->rgwOperand[0]);
      PAL_DrawText(msg, PAL_XY(XBase, iDescLine * 16 + YBase), DESCTEXT_COLOR, TRUE, FALSE);
      wScriptEntry++;
    } break;

    case 0x00A7:
      wScriptEntry++;
      break;

    default:
      // Other operations
      wScriptEntry = PAL_InterpretInstruction(wScriptEntry, wEventObjectID);
      break;
    }

    return wScriptEntry;
  }
}
