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
// Portions based on PALx Project by palxex.
// Copyright (c) 2006, Pal Lockheart <palxex@gmail.com>.
//

#include "main.h"

typedef struct tagRESOURCES {
  BYTE bLoadFlags;

  LPPALMAP lpMap;                  // loaded map of current scene
  LPSPRITE *lppEventObjectSprites; // event object sprites of current scene
  int nEventObject;                // number of event objects in current scene

  LPSPRITE rglpPlayerSprite[MAX_PLAYABLE_PLAYER_ROLES]; // player sprites of current party
} RESOURCES;

static RESOURCES gpResources;

LPPALMAP
PAL_GetCurrentMap(VOID) { return gpResources.lpMap; }

LPSPRITE
PAL_GetPlayerSprite(BYTE bPlayerIndex) {
  if (bPlayerIndex > MAX_PLAYABLE_PLAYER_ROLES - 1)
    return NULL;
  return gpResources.rglpPlayerSprite[bPlayerIndex];
}

static VOID PAL_FreePlayerSprites(VOID) {
  for (int i = 0; i < MAX_PLAYABLE_PLAYER_ROLES; i++) {
    free(gpResources.rglpPlayerSprite[i]);
    gpResources.rglpPlayerSprite[i] = NULL;
  }
}

LPSPRITE
PAL_GetEventObjectSprite(WORD wEventObjectID) {
  wEventObjectID -= SCENE_EVENT_OBJ_IDX_ST;
  if (wEventObjectID >= gpResources.nEventObject)
    return NULL;
  return gpResources.lppEventObjectSprites[wEventObjectID];
}

static VOID PAL_FreeEventObjectSprites(VOID) {
  if (gpResources.lppEventObjectSprites != NULL) {
    for (int i = 0; i < gpResources.nEventObject; i++)
      free(gpResources.lppEventObjectSprites[i]);
    free(gpResources.lppEventObjectSprites);
    gpResources.lppEventObjectSprites = NULL;
    gpResources.nEventObject = 0;
  }
}

VOID PAL_SetLoadFlags(BYTE bFlags) { gpResources.bLoadFlags |= bFlags; }

VOID PAL_LoadResources(VOID) {
  // Load global data
  if (gpResources.bLoadFlags & kLoadGlobalData) {
    PAL_InitGameData(g.saveSlot);
    AUDIO_PlayMusic(g.wCurMusic, TRUE, 1);
  }

  // Load scene
  if (gpResources.bLoadFlags & kLoadScene) {

    if (gEnteringScene) {
      g.wScreenWave = 0;
      gWaveProgression = 0;
    }

    // Free previous loaded scene (sprites and map)
    PAL_FreeEventObjectSprites();
    PAL_FreeMap(gpResources.lpMap);

    // Load map
    gpResources.lpMap = PAL_LoadMap(g.scenes[g.wCurScene - 1].wMapNum);
    if (gpResources.lpMap == NULL) {
      TerminateOnError("fail to load map");
    }

    // Load sprites
    int index = SCENE_EVENT_OBJ_IDX_ST;
    gpResources.nEventObject = SCENE_EVENT_OBJ_IDX_ED - index;

    if (gpResources.nEventObject > 0) {
      gpResources.lppEventObjectSprites = (LPSPRITE *)UTIL_malloc(gpResources.nEventObject * sizeof(LPSPRITE));
    }

    for (int i = 0; i < gpResources.nEventObject; i++, index++) {
      int n = g.events[index].wSpriteNum;
      if (n == 0) {
        // this event object has no sprite
        gpResources.lppEventObjectSprites[i] = NULL;
        continue;
      }
      int size = PAL_MKFGetDecompressedSize(n, FP_MGO);
      gpResources.lppEventObjectSprites[i] = (LPSPRITE)UTIL_malloc(size);
      if (PAL_MKFDecompressChunk(gpResources.lppEventObjectSprites[i], size, n, FP_MGO) > 0) {
        g.events[index].nSpriteFramesAuto = PAL_SpriteGetNumFrames(gpResources.lppEventObjectSprites[i]);
      }
    }

    gPartyOffset = PAL_XY(160, 112);
  }

  // Load player sprites
  if (gpResources.bLoadFlags & kLoadPlayerSprite) {
    // Free previous loaded player sprites
    PAL_FreePlayerSprites();

    // Load player sprite
    for (int i = 0; i <= (short)g.wMaxPartyMemberIndex; i++) {
      WORD wSpriteNum = PLAYER.rgwSpriteNum[PARTY_PLAYER(i)];
      int size = PAL_MKFGetDecompressedSize(wSpriteNum, FP_MGO);
      LPSPRITE ptr = (LPSPRITE)UTIL_malloc(size);
      PAL_MKFDecompressChunk(ptr, size, wSpriteNum, FP_MGO);
      gpResources.rglpPlayerSprite[i] = ptr;
    }

    // Load the follower sprite
    for (int i = 1; i <= g.nFollower; i++) {
      WORD wSpriteNum = PARTY_PLAYER(g.wMaxPartyMemberIndex + i);
      int size = PAL_MKFGetDecompressedSize(wSpriteNum, FP_MGO);
      LPSPRITE ptr = (LPSPRITE)UTIL_malloc(size);
      PAL_MKFDecompressChunk(ptr, size, wSpriteNum, FP_MGO);
      gpResources.rglpPlayerSprite[(short)g.wMaxPartyMemberIndex + i] = ptr;
    }
  }

  // Clear all of the load flags
  gpResources.bLoadFlags = 0;
}

VOID PAL_FreeResources(VOID) {
  PAL_FreePlayerSprites();
  PAL_FreeEventObjectSprites();
  PAL_FreeMap(gpResources.lpMap);
}

#define MAX_SPRITE_TO_DRAW 512

typedef struct tagSPRITE_TO_DRAW {
  LPCBITMAPRLE lpBitmap; // pointer to the frame bitmap
  PAL_POS pos;           // position on the scene
  int ySort;
} SPRITE_TO_DRAW;

static SPRITE_TO_DRAW g_rgSpriteToDraw[MAX_SPRITE_TO_DRAW];
static int g_nSpriteToDraw;

static VOID PAL_AddSpriteToDraw(LPCBITMAPRLE lpSpriteFrame, int x, int y, int ySort) {
  if (g_nSpriteToDraw >= MAX_SPRITE_TO_DRAW)
    TerminateOnError("too many sprites");
  g_rgSpriteToDraw[g_nSpriteToDraw].lpBitmap = lpSpriteFrame;
  g_rgSpriteToDraw[g_nSpriteToDraw].pos = PAL_XY(x, y);
  g_rgSpriteToDraw[g_nSpriteToDraw].ySort = ySort;
  g_nSpriteToDraw++;
}

// Draw players and moving objects to screen. Re-draw map tiles if cover these sprites.
static VOID PAL_SceneDrawSprites(VOID) {
  g_nSpriteToDraw = 0;

  // Players
  for (int i = 0; i <= (short)g.wMaxPartyMemberIndex + g.nFollower; i++) {
    LPCBITMAPRLE lpBitmap = PAL_SpriteGetFrame(PAL_GetPlayerSprite((BYTE)i), PARTY[i].wFrame);
    if (lpBitmap == NULL)
      continue;
    // x 需要从居中转到左上角, y 本来就在底部
    int x = PARTY[i].x - PAL_RLEGetWidth(lpBitmap) / 2;
    int y = PARTY[i].y - PAL_RLEGetHeight(lpBitmap) + 4;
    int layer = g.wLayer + 6;
    int ySort = y + PAL_RLEGetHeight(lpBitmap) + layer;
    PAL_AddSpriteToDraw(lpBitmap, x, y, ySort);
  }

  // Event Objects (Monsters/NPCs/others)
  for (int i = SCENE_EVENT_OBJ_IDX_ST; i < SCENE_EVENT_OBJ_IDX_ED; i++) {
    LPEVENTOBJECT lpEvtObj = &(g.events[i]);
    if (lpEvtObj->sState == kObjStateHidden || lpEvtObj->sVanishTime > 0 || lpEvtObj->sState < 0)
      continue;

    // Get the sprite
    LPCSPRITE lpSprite = PAL_GetEventObjectSprite((WORD)i);
    if (lpSprite == NULL)
      continue;

    int iFrame = lpEvtObj->wCurrentFrameNum;
    if (lpEvtObj->nSpriteFrames == 3) {
      // walking character
      if (iFrame == 2)
        iFrame = 0;
      if (iFrame == 3)
        iFrame = 2;
    }

    LPCBITMAPRLE lpBitmap = PAL_SpriteGetFrame(lpSprite, lpEvtObj->wDirection * lpEvtObj->nSpriteFrames + iFrame);
    if (lpBitmap == NULL)
      continue;

    // Calculate the coordinate and check if outside the screen
    int x = (SHORT)lpEvtObj->x - PAL_X(g.viewport) - PAL_RLEGetWidth(lpBitmap) / 2;
    if (x >= 320 || x < -(int)PAL_RLEGetWidth(lpBitmap))
      continue;
    int y = (SHORT)lpEvtObj->y - PAL_Y(g.viewport) - PAL_RLEGetHeight(lpBitmap) + 7;
    if (y >= 200 || y < -(int)PAL_RLEGetHeight(lpBitmap))
      continue;

    int layer = lpEvtObj->sLayer * 8 + 2; // 比 map tile 优先级高
    int ySort = y + PAL_RLEGetHeight(lpBitmap) + layer;
    PAL_AddSpriteToDraw(lpBitmap, x, y, ySort);
  }

  // Add all map tiles with height that could cover above sprites
  for (int j = PAL_Y(g.viewport) / 16; j <= (PAL_Y(g.viewport) + 200) / 16; j++)
    for (int i = PAL_X(g.viewport) / 32; i <= (PAL_X(g.viewport) + 320) / 32; i++)
      for (int h = 0; h < 2; h++)
        for (int l = 0; l < 2; l++) {
          LPCBITMAPRLE lpTile = PAL_MapGetTileBitmap(i, j, h, l, PAL_GetCurrentMap());
          int height = PAL_MapGetTileHeight(i, j, h, l, PAL_GetCurrentMap());
          if (lpTile != NULL && height > 0) {
            int x = i * 32 + h * 16 - 16 - PAL_X(g.viewport);
            int y = j * 16 + h * 8 - 8 - PAL_Y(g.viewport);
            int layer = height * 8 + l;
            int ySort = y + PAL_RLEGetHeight(lpTile) + layer;
            PAL_AddSpriteToDraw(lpTile, x, y, ySort);
          }
        }

  // All sprites are now in our array; sort them by their vertical positions.
  for (int x = 0; x < g_nSpriteToDraw - 1; x++) {
    BOOL fSwap = FALSE;
    for (int y = 0; y < g_nSpriteToDraw - 1 - x; y++) {
      if (g_rgSpriteToDraw[y].ySort > g_rgSpriteToDraw[y + 1].ySort) {
        fSwap = TRUE;
        SPRITE_TO_DRAW tmp = g_rgSpriteToDraw[y];
        g_rgSpriteToDraw[y] = g_rgSpriteToDraw[y + 1];
        g_rgSpriteToDraw[y + 1] = tmp;
      }
    }
    if (!fSwap)
      break;
  }

  // Draw all the sprites to the screen.
  for (int i = 0; i < g_nSpriteToDraw; i++) {
    SPRITE_TO_DRAW *p = &g_rgSpriteToDraw[i];
    PAL_RLEBlitToSurface(p->lpBitmap, gpScreen, p->pos);
  }
}

VOID PAL_ApplyWave(SDL_Surface *lpSurface) {
  g.wScreenWave += gWaveProgression;
  if (g.wScreenWave == 0 || g.wScreenWave >= 256) {
    // No need to wave the screen
    g.wScreenWave = 0;
    gWaveProgression = 0;
    return;
  }

  // Calculate the waving offsets.
  int a = 0;
  int b = 60 + 8;

  int wave[32];
  for (int i = 0; i < 16; i++) {
    b -= 8;
    a += b;

    // WARNING: assuming the screen width is 320
    wave[i] = a * g.wScreenWave / 256;
    wave[i + 16] = 320 - wave[i];
  }

  // Apply the effect.
  // WARNING: only works with 320x200 8-bit surface.
  static int index = 0;
  a = index;
  LPBYTE p = (LPBYTE)(lpSurface->pixels);

  // Loop through all lines in the screen buffer.
  for (int i = 0; i < 200; i++) {
    b = wave[a];

    if (b > 0) {
      // Do a shift on the current line with the calculated offset.
      BYTE buf[320];
      memcpy(buf, p, b);
      // memmove(p, p + b, 320 - b);
      memmove(p, &p[b], 320 - b);
      // memcpy(p + 320 - b, buf, b);
      memcpy(&p[320 - b], buf, b);
    }

    a = (a + 1) % 32;
    p += lpSurface->pitch;
  }

  index = (index + 1) % 32;
}

// Draw the scene of the current frame to the screen. Both the map and the sprites are handled here.
VOID PAL_MakeScene(VOID) {
  SDL_Rect rect = {0, 0, 320, 200};

  // Step 1: Draw the complete map, for both of the layers.
  rect.x = PAL_X(g.viewport);
  rect.y = PAL_Y(g.viewport);

  PAL_MapBlitToSurface(PAL_GetCurrentMap(), gpScreen, &rect, 0);
  PAL_MapBlitToSurface(PAL_GetCurrentMap(), gpScreen, &rect, 1);

  // Step 2: Apply screen waving effects.
  PAL_ApplyWave(gpScreen);

  // Step 3: Draw all the sprites.
  PAL_SceneDrawSprites();

  // Check if we need to fade in.
  if (gNeedToFadeIn) {
    VIDEO_UpdateScreen(NULL);
    PAL_FadeIn(gCurPalette, gNightPalette, 1);
    gNeedToFadeIn = FALSE;
  }
}

BOOL PAL_CheckObstacle(PAL_POS pos, BOOL fCheckEventObjects, WORD wSelfObject) {
  return PAL_CheckObstacleWithRange(pos, fCheckEventObjects, wSelfObject, FALSE);
}

BOOL PAL_CheckObstacleWithRange(PAL_POS pos, BOOL fCheckEventObjects, WORD wSelfObject, BOOL fCheckRange) {
  // Check if the map tile at the specified position is blocking
  int x = PAL_X(pos) / 32;
  int y = PAL_Y(pos) / 16;
  int h = 0;

  // Avoid walk out of range, look out of map
  if (fCheckRange) {
    int blockX = PAL_X(gPartyOffset) / 32;
    int blockY = PAL_Y(gPartyOffset) / 16;
    if (x < blockX || x >= 2048 || y < blockY || y >= 2048) {
      return TRUE;
    }
  }

  int xr = PAL_X(pos) % 32;
  int yr = PAL_Y(pos) % 16;
  if (xr + yr * 2 >= 16) {
    if (xr + yr * 2 >= 48) {
      x++;
      y++;
    } else if (32 - xr + yr * 2 < 16) {
      x++;
    } else if (32 - xr + yr * 2 < 48) {
      h = 1;
    } else {
      y++;
    }
  }

  if (PAL_MapTileIsBlocked(x, y, h, PAL_GetCurrentMap())) {
    return TRUE;
  }

  if (fCheckEventObjects) {
    // Loop through all event objects in the current scene
    for (int i = SCENE_EVENT_OBJ_IDX_ST; i < SCENE_EVENT_OBJ_IDX_ED; i++) {
      LPEVENTOBJECT p = &(g.events[i]);
      if (i == wSelfObject - 1)
        continue; // Skip myself

      // Is this object a blocking one?
      if (p->sState >= kObjStateBlocker) {
        // Check for collision
        if (abs(p->x - PAL_X(pos)) + abs(p->y - PAL_Y(pos)) * 2 < 16)
          return TRUE;
      }
    }
  }
  return FALSE;
}

VOID PAL_UpdatePartyGestures(BOOL fWalking) {
  static int s_iThisStepFrame = 0;
  int iStepFrameFollower = 0, iStepFrameLeader = 0;

  if (fWalking) {
    // 3 frames, 0 is standing, 1 and 2 is stepping
    s_iThisStepFrame = (s_iThisStepFrame + 1) % 4;
    if (s_iThisStepFrame & 1) {
      iStepFrameLeader = (s_iThisStepFrame + 1) / 2; // 1 or 2
      iStepFrameFollower = 3 - iStepFrameLeader;     // 2 or 1
    } else {
      iStepFrameLeader = 0;
      iStepFrameFollower = 0;
    }

    // Update the gestures and positions for other party leader
    PARTY[0].x = PAL_X(gPartyOffset);
    PARTY[0].y = PAL_Y(gPartyOffset);
    if (PLAYER.rgwWalkFrames[PARTY_PLAYER(0)] == 4) // 灵儿蛇形
      PARTY[0].wFrame = g.wPartyDirection * 4 + s_iThisStepFrame;
    else
      PARTY[0].wFrame = g.wPartyDirection * 3 + iStepFrameLeader;

    // Update the gestures and positions for other party members
    for (int i = 1; i <= (short)g.wMaxPartyMemberIndex; i++) {
      PARTY[i].x = TRAIL[1].x - PAL_X(g.viewport);
      PARTY[i].y = TRAIL[1].y - PAL_Y(g.viewport);
      if (PLAYER.rgwWalkFrames[PARTY_PLAYER(i)] == 4) // 灵儿蛇形
        PARTY[i].wFrame = TRAIL[2].wDirection * 4 + s_iThisStepFrame;
      else
        PARTY[i].wFrame = TRAIL[2].wDirection * 3 + iStepFrameLeader;

      if (i == 2) {
        PARTY[i].x += (TRAIL[1].wDirection == kDirEast || TRAIL[1].wDirection == kDirWest) ? -16 : 16;
        PARTY[i].y += 8;
      } else {
        PARTY[i].x += ((TRAIL[1].wDirection == kDirWest || TRAIL[1].wDirection == kDirSouth) ? 16 : -16);
        PARTY[i].y += ((TRAIL[1].wDirection == kDirWest || TRAIL[1].wDirection == kDirNorth) ? 8 : -8);
      }

      // Adjust the position if there is obstacle
      if (PAL_CheckObstacleWithRange(PAL_XY(PARTY[i].x + PAL_X(g.viewport), PARTY[i].y + PAL_Y(g.viewport)), TRUE, 0,
                                     TRUE)) {
        PARTY[i].x = TRAIL[1].x - PAL_X(g.viewport);
        PARTY[i].y = TRAIL[1].y - PAL_Y(g.viewport);
      }
    }

    // Update the position and gesture for the follower
    for (int i = 1; i <= g.nFollower; i++) {
      PARTY[g.wMaxPartyMemberIndex + i].x = TRAIL[2 + i].x - PAL_X(g.viewport);
      PARTY[g.wMaxPartyMemberIndex + i].y = TRAIL[2 + i].y - PAL_Y(g.viewport);
      PARTY[g.wMaxPartyMemberIndex + i].wFrame = TRAIL[2 + i].wDirection * 3 + iStepFrameFollower;
    }
  } else {
    // Player is not moved. Use the "standing" gesture instead of "walking" one.
    int f = PLAYER.rgwWalkFrames[PARTY_PLAYER(0)]; // 灵儿蛇形
    if (f == 0)
      f = 3;
    PARTY[0].wFrame = g.wPartyDirection * f;

    for (int i = 1; i <= (short)g.wMaxPartyMemberIndex; i++) {
      int f = PLAYER.rgwWalkFrames[PARTY_PLAYER(i)]; // 灵儿蛇形
      if (f == 0)
        f = 3;
      PARTY[i].wFrame = TRAIL[2].wDirection * f;
    }

    for (int i = 1; i <= g.nFollower; i++)
      PARTY[g.wMaxPartyMemberIndex + i].wFrame = TRAIL[2 + i].wDirection * 3;

    // 0 -> 2, 2 -> 0
    s_iThisStepFrame &= 2;
    s_iThisStepFrame ^= 2;
  }
}

VOID PAL_UpdateParty(VOID) {
  // Has user pressed one of the arrow keys?
  if (g_InputState.dir != kDirUnknown) {
    int xOffset = ((g_InputState.dir == kDirWest || g_InputState.dir == kDirSouth) ? -16 : 16);
    int yOffset = ((g_InputState.dir == kDirWest || g_InputState.dir == kDirNorth) ? -8 : 8);

    int xSource = PAL_X(g.viewport) + PAL_X(gPartyOffset);
    int ySource = PAL_Y(g.viewport) + PAL_Y(gPartyOffset);

    int xTarget = xSource + xOffset;
    int yTarget = ySource + yOffset;

    g.wPartyDirection = g_InputState.dir;

    // Check for obstacles on the destination location
    if (!PAL_CheckObstacleWithRange(PAL_XY(xTarget, yTarget), TRUE, 0, TRUE)) {
      // Player will actually be moved. Store trail.
      for (int i = 3; i >= 0; i--)
        TRAIL[i + 1] = TRAIL[i];

      TRAIL[0].wDirection = g_InputState.dir;
      TRAIL[0].x = xSource;
      TRAIL[0].y = ySource;

      // Move the viewport
      g.viewport = PAL_XY(PAL_X(g.viewport) + xOffset, PAL_Y(g.viewport) + yOffset);

      // Update gestures
      PAL_UpdatePartyGestures(/*fWalking=*/TRUE);

      return; // don't go further
    }
  }

  PAL_UpdatePartyGestures(/*fWalking=*/FALSE);
}

VOID PAL_NPCWalkOneStep(WORD wEventObjectID, INT iSpeed) {

  // Check for invalid parameters
  if (wEventObjectID == 0 || wEventObjectID > MAX_EVENTS)
    return;

  LPEVENTOBJECT p = &(g.events[wEventObjectID - 1]);

  // Move the event object by the specified direction
  p->x += ((p->wDirection == kDirWest || p->wDirection == kDirSouth) ? -2 : 2) * iSpeed;
  p->y += ((p->wDirection == kDirWest || p->wDirection == kDirNorth) ? -1 : 1) * iSpeed;

  // Update the gesture
  if (p->nSpriteFrames > 0) {
    p->wCurrentFrameNum++;
    p->wCurrentFrameNum %= (p->nSpriteFrames == 3 ? 4 : p->nSpriteFrames);
  } else if (p->nSpriteFramesAuto > 0) {
    p->wCurrentFrameNum++;
    p->wCurrentFrameNum %= p->nSpriteFramesAuto;
  }
}

// Make the specified event object walk to the map position specified by (x, y, h) at the speed of iSpeed.
// Return TRUE if the event object has successfully moved to the specified position, FALSE if still need more moving.
BOOL PAL_NPCWalkTo(WORD wEventObjectID, INT x, INT y, INT h, INT iSpeed) {
  LPEVENTOBJECT pEvtObj = &(g.events[wEventObjectID - 1]);

  int xOffset = (x * 32 + h * 16) - pEvtObj->x;
  int yOffset = (y * 16 + h * 8) - pEvtObj->y;

  if (yOffset < 0)
    pEvtObj->wDirection = ((xOffset < 0) ? kDirWest : kDirNorth);
  else
    pEvtObj->wDirection = ((xOffset < 0) ? kDirSouth : kDirEast);

  if (abs(xOffset) < iSpeed * 2 || abs(yOffset) < iSpeed * 2) {
    pEvtObj->x = x * 32 + h * 16;
    pEvtObj->y = y * 16 + h * 8;
  } else {
    PAL_NPCWalkOneStep(wEventObjectID, iSpeed);
  }

  if (pEvtObj->x == x * 32 + h * 16 && pEvtObj->y == y * 16 + h * 8) {
    pEvtObj->wCurrentFrameNum = 0;
    return TRUE;
  }

  return FALSE;
}

// Make the party walk to the map position specified by (x, y, h) at the speed of iSpeed.
VOID PAL_PartyWalkTo(INT x, INT y, INT h, INT iSpeed) {
  int xOffset = x * 32 + h * 16 - PAL_X(g.viewport) - PAL_X(gPartyOffset);
  int yOffset = y * 16 + h * 8 - PAL_Y(g.viewport) - PAL_Y(gPartyOffset);

  int64_t t = 0;

  while (xOffset != 0 || yOffset != 0) {
    PAL_DelayUntil(t);
    t = PAL_GetTicks() + FRAME_TIME;

    // Store trail
    for (int i = 3; i >= 0; i--)
      TRAIL[i + 1] = TRAIL[i];

    TRAIL[0].wDirection = g.wPartyDirection;
    TRAIL[0].x = PAL_X(g.viewport) + PAL_X(gPartyOffset);
    TRAIL[0].y = PAL_Y(g.viewport) + PAL_Y(gPartyOffset);

    g.wPartyDirection = (yOffset < 0) ? ((xOffset < 0) ? kDirWest : kDirNorth) : ((xOffset < 0) ? kDirSouth : kDirEast);

    int dx = PAL_X(g.viewport) + ((abs(xOffset) <= iSpeed * 2) ? xOffset : iSpeed * (xOffset < 0 ? -2 : 2));
    int dy = PAL_Y(g.viewport) + ((abs(yOffset) <= iSpeed) ? yOffset : iSpeed * (yOffset < 0 ? -1 : 1));

    // Move the viewport
    g.viewport = PAL_XY(dx, dy);

    PAL_UpdatePartyGestures(TRUE);
    PAL_GameUpdate(FALSE);
    PAL_MakeScene();
    VIDEO_UpdateScreen(NULL);

    xOffset = x * 32 + h * 16 - PAL_X(g.viewport) - PAL_X(gPartyOffset);
    yOffset = y * 16 + h * 8 - PAL_Y(g.viewport) - PAL_Y(gPartyOffset);
  }

  PAL_UpdatePartyGestures(FALSE);
}

// Move the party to the specified position, riding the specified event object.
// Return TRUE if the party and event object has successfully moved to the specified position, FALSE if still need more
// moving.
VOID PAL_PartyRideEventObject(WORD wEventObjectID, INT x, INT y, INT h, INT iSpeed) {
  LPEVENTOBJECT p = &(g.events[wEventObjectID - 1]);

  int xOffset = x * 32 + h * 16 - PAL_X(g.viewport) - PAL_X(gPartyOffset);
  int yOffset = y * 16 + h * 8 - PAL_Y(g.viewport) - PAL_Y(gPartyOffset);

  uint64_t t = 0;
  while (xOffset != 0 || yOffset != 0) {
    PAL_DelayUntil(t);
    t = PAL_GetTicks() + FRAME_TIME;

    g.wPartyDirection = (yOffset < 0) ? ((xOffset < 0) ? kDirWest : kDirNorth) : ((xOffset < 0) ? kDirSouth : kDirEast);

    int dx = (abs(xOffset) > iSpeed * 2) ? iSpeed * (xOffset < 0 ? -2 : 2) : xOffset;
    int dy = (abs(yOffset) > iSpeed) ? iSpeed * (yOffset < 0 ? -1 : 1) : yOffset;

    // Store trail
    for (int i = 3; i >= 0; i--)
      TRAIL[i + 1] = TRAIL[i];

    TRAIL[0].wDirection = g.wPartyDirection;
    TRAIL[0].x = PAL_X(g.viewport) + dx + PAL_X(gPartyOffset);
    TRAIL[0].y = PAL_Y(g.viewport) + dy + PAL_Y(gPartyOffset);

    // Move the viewport
    g.viewport = PAL_XY_OFFSET(g.viewport, dx, dy);

    // Move the event object
    p->x += dx;
    p->y += dy;

    PAL_GameUpdate(FALSE);
    PAL_MakeScene();
    VIDEO_UpdateScreen(NULL);

    xOffset = x * 32 + h * 16 - PAL_X(g.viewport) - PAL_X(gPartyOffset);
    yOffset = y * 16 + h * 8 - PAL_Y(g.viewport) - PAL_Y(gPartyOffset);
  }
}

// Make the specified event object chase the players.
VOID PAL_MonsterChasePlayer(WORD wEventObjectID, WORD wSpeed, WORD wChaseRange, BOOL fFloating) {
  LPEVENTOBJECT pEvtObj = &g.events[wEventObjectID - 1];
  WORD wMonsterSpeed = 0;

  if (g.wChaseRange != 0) {
    int x = PAL_X(g.viewport) + PAL_X(gPartyOffset) - pEvtObj->x;
    int y = PAL_Y(g.viewport) + PAL_Y(gPartyOffset) - pEvtObj->y;

    if (x == 0)
      x = RandomLong(0, 1) ? -1 : 1;
    if (y == 0)
      y = RandomLong(0, 1) ? -1 : 1;

    // 计算当前怪物所在的图块中心点
    WORD prevx = pEvtObj->x;
    WORD prevy = pEvtObj->y;
    int i = prevx % 32;
    int j = prevy % 16;
    prevx /= 32;
    prevy /= 16;
    int l = 0;
    if (i + j * 2 >= 16) {
      if (i + j * 2 >= 48) {
        prevx++;
        prevy++;
      } else if (32 - i + j * 2 < 16) {
        prevx++;
      } else if (32 - i + j * 2 < 48) {
        l = 1;
      } else {
        prevy++;
      }
    }
    prevx = prevx * 32 + l * 16;
    prevy = prevy * 16 + l * 8;

    // 玩家是否在追踪范围内
    if (abs(x) + abs(y) * 2 < wChaseRange * 32 * g.wChaseRange) {
      pEvtObj->wDirection = (x < 0) ? ((y < 0) ? kDirWest : kDirSouth) : ((y < 0) ? kDirNorth : kDirEast);

      // 下一步位置
      x = pEvtObj->x + x / abs(x) * 16;
      y = pEvtObj->y + y / abs(y) * 8;

      if (fFloating) {
        wMonsterSpeed = wSpeed;
      } else {
        wMonsterSpeed = wSpeed;
        // 碰撞盒检测, 上下左右4个偏移点
        for (int i = 0; i < 4; i++) {
          int dx = 0, dy = 0;
          if (i == 0)
            dx = -4, dy = +2;
          else if (i == 1)
            dx = -4, dy = -2;
          else if (i == 2)
            dx = +4, dy = -2;
          else if (i == 3)
            dx = +4, dy = +2;

          // 边缘碰到障碍，强制回弹
          if (PAL_CheckObstacle(PAL_XY(x + dx, y + dy), FALSE, 0)) {
            pEvtObj->x = prevx;
            pEvtObj->y = prevy;
            wMonsterSpeed = 0;
            break;
          }
        }
      }
    }
  } else {
    // Exorcism-Fragrance (驱魔香) will cause monsters to spin in place
    // Switch direction every two frames
    if (gFrameNum & 1) {
      pEvtObj->wDirection++;
      if (pEvtObj->wDirection > 3)
        pEvtObj->wDirection = 0;
    }
  }

  PAL_NPCWalkOneStep(wEventObjectID, wMonsterSpeed);
}

VOID PAL_MoveViewport(WORD op0, WORD op1, WORD op2) {
  if (op0 == 0 && op1 == 0) {
    // 镜头复位

    // 计算主角当前位置与屏幕中心的偏差
    int x = PARTY[0].x - 160;
    int y = PARTY[0].y - 112;

    // 移动视口
    g.viewport = PAL_XY_OFFSET(g.viewport, x, y);
    // 更新队伍偏移
    gPartyOffset = PAL_XY(160, 112);
    // 修正队员坐标
    for (int i = 0; i <= (short)g.wMaxPartyMemberIndex + g.nFollower; i++) {
      PARTY[i].x -= x;
      PARTY[i].y -= y;
    }

    // 如果不是瞬间复位，则刷新画面
    if (op2 != 0xFFFF) {
      PAL_MakeScene();
      VIDEO_UpdateScreen(NULL);
    }
  } else if (op2 == 0xFFFF) {
    // 直接跳转到地图的特定 tile

    // 计算目标视口的绝对坐标
    int targetViewportX = op0 * 32 - 160;
    int targetViewportY = op1 * 16 - 112;

    // 计算视口移动的增量
    int dx = PAL_X(g.viewport) - targetViewportX;
    int dy = PAL_Y(g.viewport) - targetViewportY;
    // 移动视口
    g.viewport = PAL_XY(targetViewportX, targetViewportY);
    // 修正队员坐标
    for (int i = 0; i <= (short)g.wMaxPartyMemberIndex + g.nFollower; i++) {
      PARTY[i].x += dx;
      PARTY[i].y += dy;
    }

    PAL_MakeScene();
    VIDEO_UpdateScreen(NULL);
  } else {
    // 镜头平滑移动
    short speedX = (short)op0;
    short speedY = (short)op1;
    int frames = (short)op2;
    if (frames == 0)
      frames = 1;
    uint64_t nextTime = PAL_GetTicks() + FRAME_TIME;
    for (int i = 0; i < frames; i++) {
      // 移动视口
      g.viewport = PAL_XY_OFFSET(g.viewport, speedX, speedY);
      // 更新队伍偏移
      gPartyOffset = PAL_XY_OFFSET(gPartyOffset, -speedX, -speedY);
      // 修正队员坐标
      for (int j = 0; j <= (short)g.wMaxPartyMemberIndex + g.nFollower; j++) {
        PARTY[j].x -= speedX;
        PARTY[j].y -= speedY;
      }

      PAL_GameUpdate(FALSE);
      PAL_MakeScene();
      VIDEO_UpdateScreen(NULL);

      PAL_DelayUntil(nextTime);
      nextTime = PAL_GetTicks() + FRAME_TIME;
    }
  }
}
