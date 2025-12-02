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

// The main game logic routine. Update the status of everything.
// fTrigger: whether to process trigger events or not.
VOID PAL_GameUpdate(BOOL fTrigger) {
  // Check for trigger events
  if (fTrigger) {
    // Check if we are entering a new scene
    if (gEnteringScene) {
      // Run the script for entering the scene
      gEnteringScene = FALSE;

      int i = g.wCurScene - 1;
      g.scenes[i].wScriptOnEnter = PAL_RunTriggerScript(g.scenes[i].wScriptOnEnter, 0xFFFF);

      // Don't go further as we're switching to another scene
      if (gEnteringScene)
        return;

      PAL_ClearKeyState();
      PAL_MakeScene();
    }

    // 处理刷新或近距离自动触发事件
    for (WORD wEventObjectID = SCENE_EVENT_OBJ_IDX_ST + 1; wEventObjectID <= SCENE_EVENT_OBJ_IDX_ED; wEventObjectID++) {
      LPEVENTOBJECT p = &g.events[wEventObjectID - 1];

      // 更新刷新时间
      if (p->sVanishTime != 0) {
        p->sVanishTime += ((p->sVanishTime < 0) ? 1 : -1);
        continue;
      }

      if (p->sState < 0) {
        // 离开屏幕后刷新
        if (p->x < PAL_X(g.viewport) || p->x > PAL_X(g.viewport) + 320 || //
            p->y < PAL_Y(g.viewport) || p->y > PAL_Y(g.viewport) + 200) {
          p->sState = abs(p->sState);
          p->wCurrentFrameNum = 0;
        }
      } else if (p->sState > 0 && p->wTriggerMode >= kTriggerTouchNear) {
        // 自动触发事件
        int xDist = PAL_X(g.viewport) + PAL_X(gPartyOffset) - p->x;
        int yDist = PAL_Y(g.viewport) + PAL_Y(gPartyOffset) - p->y;
        if (abs(xDist) + abs(yDist) * 2 < (p->wTriggerMode - kTriggerTouchNear) * 32 + 16) {
          // Player is in the trigger zone.

          if (p->nSpriteFrames) {
            // The sprite has multiple frames. Try to adjust the direction.
            p->wCurrentFrameNum = 0;
            if (xDist > 0) {
              p->wDirection = ((yDist > 0) ? kDirEast : kDirNorth);
            } else {
              p->wDirection = ((yDist > 0) ? kDirSouth : kDirWest);
            }

            // Redraw the scene
            PAL_UpdatePartyGestures(FALSE);

            PAL_MakeScene();
            VIDEO_UpdateScreen(NULL);
          }

          // Execute the script.
          p->wTriggerScript = PAL_RunTriggerScript(p->wTriggerScript, wEventObjectID);

          PAL_ClearKeyState();

          // Don't go further on scene switching
          if (gEnteringScene)
            return;
        }
      }
    }
  }

  // Run autoscript for each event objects
  for (WORD wEventObjectID = SCENE_EVENT_OBJ_IDX_ST + 1; wEventObjectID <= SCENE_EVENT_OBJ_IDX_ED; wEventObjectID++) {
    LPEVENTOBJECT p = &g.events[wEventObjectID - 1];

    if (p->sState > 0 && p->sVanishTime == 0) {
      WORD wScriptEntry = p->wAutoScript;
      if (wScriptEntry != 0) {
        p->wAutoScript = PAL_RunAutoScript(wScriptEntry, wEventObjectID);
        // Don't go further on scene switching
        if (gEnteringScene)
          return;
      }
    }

    // Check if the player is in the way
    int xDist = PAL_X(g.viewport) + PAL_X(gPartyOffset) - p->x;
    int yDist = PAL_Y(g.viewport) + PAL_Y(gPartyOffset) - p->y;
    if (fTrigger && p->sState >= kObjStateBlocker && p->wSpriteNum != 0 && abs(xDist) + abs(yDist) * 2 <= 12) {
      // Player is in the way, try to move player a step
      for (int i = 0; i < 4; i++) {
        WORD wDir = (p->wDirection + i + 1) % 4;
        int x = PAL_X(g.viewport) + PAL_X(gPartyOffset);
        int y = PAL_Y(g.viewport) + PAL_Y(gPartyOffset);
        x += ((wDir == kDirWest || wDir == kDirSouth) ? -16 : 16);
        y += ((wDir == kDirWest || wDir == kDirNorth) ? -8 : 8);
        PAL_POS pos = PAL_XY(x, y);
        if (!PAL_CheckObstacleWithRange(pos, TRUE, 0, TRUE)) {
          g.viewport = PAL_XY(PAL_X(pos) - PAL_X(gPartyOffset), PAL_Y(pos) - PAL_Y(gPartyOffset));
          break;
        }
      }
    }
  }

  // 更新驱魔香/十里香效果时间
  if (--g.wChasespeedChangeCycles == 0)
    g.wChaseRange = 1;

  gFrameNum++;
}

VOID PAL_GameUseItem(VOID) {
  while (TRUE) {
    WORD wObject = PAL_ItemSelectMenu(NULL, kItemFlagUsable);
    if (wObject == 0)
      return;

    if (!(OBJECT[wObject].item.wFlags & kItemFlagApplyToAll)) {
      // Select the player to use the item on
      WORD wPlayer = 0;

      while (TRUE) {
        wPlayer = PAL_ItemUseMenu(wObject);
        if (wPlayer == MENUITEM_VALUE_CANCELLED)
          break;

        // Run the script
        OBJECT[wObject].item.wScriptOnUse = PAL_RunTriggerScript(OBJECT[wObject].item.wScriptOnUse, wPlayer);

        // Remove the item if the item is consuming and the script succeeded
        if ((OBJECT[wObject].item.wFlags & kItemFlagConsuming) && g_fScriptSuccess)
          PAL_AddItemToInventory(wObject, -1);
      }
    } else {
      // Run the script
      OBJECT[wObject].item.wScriptOnUse = PAL_RunTriggerScript(OBJECT[wObject].item.wScriptOnUse, 0xFFFF);

      // Remove the item if the item is consuming and the script succeeded
      if ((OBJECT[wObject].item.wFlags & kItemFlagConsuming) && g_fScriptSuccess)
        PAL_AddItemToInventory(wObject, -1);

      return;
    }
  }
}

VOID PAL_GameEquipItem(VOID) {
  while (TRUE) {
    WORD wObject = PAL_ItemSelectMenu(NULL, kItemFlagEquipable);
    if (wObject == 0)
      return;
    PAL_EquipItemMenu(wObject);
  }
}

// Process searching trigger events.
VOID PAL_Search(VOID) {

  // Calculate 13 checkpoint coordinates for manual event search.
  int x = PAL_X(g.viewport) + PAL_X(gPartyOffset);
  int y = PAL_Y(g.viewport) + PAL_Y(gPartyOffset);

  int xOffset = (g.wPartyDirection == kDirNorth || g.wPartyDirection == kDirEast) ? 16 : -16;
  int yOffset = (g.wPartyDirection == kDirEast || g.wPartyDirection == kDirSouth) ? 8 : -8;

  PAL_POS rgPos[13];
  rgPos[0] = PAL_XY(x, y);

  for (int i = 0; i < 4; i++) {
    rgPos[i * 3 + 1] = PAL_XY(x + xOffset, y + yOffset);
    rgPos[i * 3 + 2] = PAL_XY(x, y + yOffset * 2);
    rgPos[i * 3 + 3] = PAL_XY(x + 2 * xOffset, y);
    x += xOffset;
    y += yOffset;
  }

  for (int i = 0; i < 13; i++) {
    // Convert to map location
    int dx = PAL_X(rgPos[i]) / 32;
    int dy = PAL_Y(rgPos[i]) / 16;
    int dh = ((PAL_X(rgPos[i]) % 32) ? 1 : 0);

    // Loop through all event objects
    for (int k = SCENE_EVENT_OBJ_IDX_ST; k < SCENE_EVENT_OBJ_IDX_ED; k++) {
      LPEVENTOBJECT p = &(g.events[k]);
      int ex = p->x / 32;
      int ey = p->y / 16;
      int eh = ((p->x % 32) ? 1 : 0);

      // kTriggerSearchNear  : i > 2
      // kTriggerSearchNormal: i > 8
      // kTriggerSearchFar   : i > 14
      if (p->sState <= 0 || p->wTriggerMode >= kTriggerTouchNear || p->wTriggerMode * 6 - 4 < i || dx != ex ||
          dy != ey || dh != eh)
        continue;

      // Adjust direction/gesture for party members and the event object
      if (p->nSpriteFrames * 4 > p->wCurrentFrameNum) {
        p->wCurrentFrameNum = 0;                     // use standing gesture
        p->wDirection = (g.wPartyDirection + 2) % 4; // face the party

        // All party members should face the event object
        for (int l = 0; l <= g.wMaxPartyMemberIndex; l++)
          PARTY[l].wFrame = g.wPartyDirection * 3;

        // Redraw everything
        PAL_MakeScene();
        VIDEO_UpdateScreen(NULL);
      }

      // Execute the script
      p->wTriggerScript = PAL_RunTriggerScript(p->wTriggerScript, k + 1);

      // Clear inputs and delay for a short time
      UTIL_Delay(50);
      PAL_ClearKeyState();

      return; // don't go further
    }
  }

  // If nothing found, flash the event object in view to draw attention. // by scturtle
  BYTE bufDialogIcons[282];
  PAL_MKFReadChunk(bufDialogIcons, 282, 12, FP_DATA);
  for (int k = SCENE_EVENT_OBJ_IDX_ST; k < SCENE_EVENT_OBJ_IDX_ED; k++) {
    LPEVENTOBJECT p = &(g.events[k]);
    if (p->sState > 0 && p->wTriggerMode > 0 && p->wTriggerMode <= 3) {
      int xDist = p->x - PAL_X(g.viewport);
      int yDist = p->y - PAL_Y(g.viewport);
      if (xDist >= 0 && xDist < 320 && yDist >= 0 && yDist < 200) {
        LPSPRITE lpSprite = PAL_GetEventObjectSprite(k);
        if (lpSprite) {
          LPCBITMAPRLE lpBitmap = PAL_SpriteGetFrame(lpSprite, p->wDirection * p->nSpriteFrames + p->wCurrentFrameNum);
          PAL_POS pos = PAL_XY(xDist - PAL_RLEGetWidth(lpBitmap) / 2, yDist - PAL_RLEGetHeight(lpBitmap) + 7);
          PAL_RLEBlitWithColorShift(lpBitmap, gpScreen, pos, 7);
        } else {
          PAL_RLEBlitToSurface(PAL_SpriteGetFrame(bufDialogIcons, 2), gpScreen, PAL_XY(xDist - 8, yDist - 16));
        }
      }
    }
  }
  VIDEO_UpdateScreen(NULL);
}

VOID PAL_QuitGame(VOID) {
  WORD wReturnValue = PAL_ConfirmMenu();
  if (wReturnValue == 1) {
    AUDIO_PlayMusic(0, FALSE, 2);
    PAL_FadeOut(2);
    PAL_Shutdown(0);
  }
}

// Starts a video frame. Called once per video frame.
VOID PAL_StartFrame(VOID) {
  // Run the game logic of one frame
  PAL_GameUpdate(TRUE);
  if (gEnteringScene)
    return;

  // Update the positions and gestures of party members
  PAL_UpdateParty();

  // Update the scene
  PAL_MakeScene();
  VIDEO_UpdateScreen(NULL);

  if (g_InputState.dwKeyPress & kKeyMenu)
    PAL_InGameMenu();
  else if (g_InputState.dwKeyPress & kKeyUseItem)
    PAL_GameUseItem();
  else if (g_InputState.dwKeyPress & kKeyThrowItem)
    PAL_GameEquipItem();
  else if (g_InputState.dwKeyPress & kKeyForce)
    PAL_InGameMagicMenu();
  else if (g_InputState.dwKeyPress & kKeyStatus)
    PAL_PlayerStatus();
  else if (g_InputState.dwKeyPress & kKeySearch)
    PAL_Search();
  else if (g_InputState.dwKeyPress & kKeyFlee)
    PAL_QuitGame();
}

VOID PAL_WaitForAnyKey(WORD wTimeOut) {
  PAL_ClearKeyState();
  DWORD dwTimeOut = PAL_GetTicks() + wTimeOut;
  while (wTimeOut == 0 || !SDL_TICKS_PASSED(PAL_GetTicks(), dwTimeOut)) {
    UTIL_Delay(5);
    if (g_InputState.dwKeyPress)
      break;
  }
}
