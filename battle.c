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

BATTLE g_Battle;

WORD g_rgPlayerPos[3][3][2] = {
    {{240, 170}},                        // one player
    {{200, 176}, {256, 152}},            // two players
    {{180, 180}, {234, 170}, {270, 146}} // three players
};

// 从 g_Battle.lpBackground 绘制到 g_Battle.lpSceneBuf
VOID PAL_BattleDrawBackground(VOID) {
  LPBYTE pSrc = g_Battle.lpBackground->pixels;
  LPBYTE pDst = g_Battle.lpSceneBuf->pixels;
  for (int i = 0; i < g_Battle.lpSceneBuf->pitch * g_Battle.lpSceneBuf->h; i++) {
    BYTE b = (*pSrc & 0x0F); // 亮度
    b += g_Battle.sBackgroundColorShift;
    if (b & 0x80) // 下溢
      b = 0;
    else if (b & 0x70) // 上溢
      b = 0x0F;
    *pDst = (b | (*pSrc & 0xF0)); // 组合
    ++pSrc;
    ++pDst;
  }
  PAL_ApplyWave(g_Battle.lpSceneBuf);
}

VOID PAL_BattleDrawEnemySprites(WORD wEnemyIndex, SDL_Surface *lpDstSurface) {
  BATTLEENEMY *enemy = &BATTLE_ENEMY[wEnemyIndex];
  LPCBITMAPRLE bitmap = PAL_SpriteGetFrame(enemy->lpSprite, enemy->wCurrentFrame);

  PAL_POS pos = enemy->pos;

  // Enemy is confused
  if (enemy->rgwStatus[kStatusConfused] > 0 && enemy->rgwStatus[kStatusSleep] == 0 &&
      enemy->rgwStatus[kStatusParalyzed] == 0) {
    pos = PAL_XY_OFFSET(pos, RandomLong(-1, 1), 0);
  }

  // bottom center => top left
  pos = PAL_XY_OFFSET(pos, -PAL_RLEGetWidth(bitmap) / 2, -PAL_RLEGetHeight(bitmap));

  // enemy is alive
  if (enemy->wObjectID != 0) {
    if (enemy->iColorShift)
      PAL_RLEBlitWithColorShift(bitmap, lpDstSurface, pos, enemy->iColorShift);
    else
      PAL_RLEBlitToSurface(bitmap, lpDstSurface, pos);
  }
}

VOID PAL_BattleDrawPlayerSprites(WORD wPlayerIndex, SDL_Surface *lpDstSurface) {
  if (wPlayerIndex == 0xFFFF) {
    // Draw the summoned god
    if (g_Battle.lpSummonSprite != NULL) {
      LPCBITMAPRLE bitmap = PAL_SpriteGetFrame(g_Battle.lpSummonSprite, g_Battle.iSummonFrame);
      PAL_POS pos = PAL_XY_OFFSET(g_Battle.posSummon, -PAL_RLEGetWidth(bitmap) / 2, -PAL_RLEGetHeight(bitmap));
      PAL_RLEBlitToSurface(bitmap, lpDstSurface, pos);
    }
  } else {
    // Draw the players
    BATTLEPLAYER *player = &BATTLE_PLAYER[wPlayerIndex];
    PAL_POS pos = player->pos;

    WORD *status = gPlayerStatus[PARTY_PLAYER(wPlayerIndex)];
    if (status[kStatusConfused] != 0 && status[kStatusSleep] == 0 && status[kStatusParalyzed] == 0 &&
        PLAYER.rgwHP[PARTY_PLAYER(wPlayerIndex)] > 0 && !PAL_IsPlayerDying(PARTY_PLAYER(wPlayerIndex))) {
      // Player is confused and not dead
      pos = PAL_XY_OFFSET(pos, 0, RandomLong(-1, 1));
    }

    LPCBITMAPRLE bitmap = PAL_SpriteGetFrame(player->lpSprite, player->wCurrentFrame);
    pos = PAL_XY_OFFSET(pos, -PAL_RLEGetWidth(bitmap) / 2, -PAL_RLEGetHeight(bitmap));

    if (player->iColorShift != 0)
      PAL_RLEBlitWithColorShift(bitmap, lpDstSurface, pos, player->iColorShift);
    else if (g_Battle.iHidingTime == 0)
      PAL_RLEBlitToSurface(bitmap, lpDstSurface, pos);
  }
}

VOID PAL_BattleDrawMagicSprites(INT iMagicNum, SDL_Surface *lpDstSurface, PAL_POS pos) {
  LPCBITMAPRLE lpBitmap = g_Battle.lpMagicBitmap;
  PAL_RLEBlitToSurface(lpBitmap, lpDstSurface,
                       PAL_XY_OFFSET(pos, -PAL_RLEGetWidth(lpBitmap) / 2, -PAL_RLEGetHeight(lpBitmap)));
}

VOID PAL_BattleClearSpriteObject(VOID) {
  memset(&g_Battle.SpriteDrawSeq, 0, sizeof(g_Battle.SpriteDrawSeq));
  g_Battle.spriteDrawSeqSize = 0;
}

VOID PAL_BattleSpriteAddUnlock(VOID) {
  g_Battle.fSpriteAddLock = FALSE;
  PAL_BattleClearSpriteObject();
}

VOID PAL_BattleAddSpriteObject(WORD wType, WORD wObjectIndex, PAL_POS pos, SHORT sLayerOffset, BOOL fHaveColorShift) {
  if (g_Battle.spriteDrawSeqSize < MAX_BATTLESPRITE_ITEMS) {
    BATTLESPRITE *SpriteObject = &g_Battle.SpriteDrawSeq[g_Battle.spriteDrawSeqSize];
    SpriteObject->wType = wType;
    SpriteObject->wObjectIndex = wObjectIndex;
    SpriteObject->pos = pos;
    SpriteObject->sLayerOffset = sLayerOffset;
    SpriteObject->fHaveColorShift = fHaveColorShift;
    g_Battle.spriteDrawSeqSize++;
  }
}

VOID PAL_BattleAddFighterSpriteObject(VOID) {
  // Place enemies in the drawing sequence.
  for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++)
    PAL_BattleAddSpriteObject(kBattleSpriteTypeEnemy, i, BATTLE_ENEMY[i].pos, 0, BATTLE_ENEMY[i].iColorShift);

  if (g_Battle.lpSummonSprite != NULL) {
    // Place summon god in the drawing sequence.
    PAL_BattleAddSpriteObject(kBattleSpriteTypePlayer, -1, g_Battle.posSummon, 0, g_Battle.fSummonColorShift);
  } else {
    // Place players in the drawing sequence.
    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++)
      PAL_BattleAddSpriteObject(kBattleSpriteTypePlayer, i, BATTLE_PLAYER[i].pos, 0, BATTLE_PLAYER[i].iColorShift);
  }
}

VOID PAL_BattleSortSpriteObjecByPos(VOID) {
  // Sort the players drawing order by Y coordinate
  for (int i = 0; i < g_Battle.spriteDrawSeqSize - 1; i++) {
    for (int j = i + 1; j < g_Battle.spriteDrawSeqSize; j++) {
      BATTLESPRITE *curr = &g_Battle.SpriteDrawSeq[i];
      BATTLESPRITE *next = &g_Battle.SpriteDrawSeq[j];
      SHORT currPosY = PAL_Y(curr->pos) + curr->sLayerOffset;
      SHORT nextPosY = PAL_Y(next->pos) + next->sLayerOffset;
      if (currPosY > nextPosY) {
        // Pos Y compare successfully, directly swap variable values
        BATTLESPRITE tmp = *curr;
        *curr = *next;
        *next = tmp;
      } else if (currPosY == nextPosY) {
        // The pos Y of the two are equal. Compare their X coordinates
        if (PAL_X(curr->pos) < PAL_X(next->pos)) {
          BATTLESPRITE tmp = *curr;
          *curr = *next;
          *next = tmp;
        }
      }
    }
  }
}

VOID PAL_BattleDrawAllSprites(VOID) {
  // Draw all sprites to the battle screen buffer.
  PAL_BattleDrawAllSpritesWithColorShift(FALSE);

  // Only draw sprites with color shift into the battle screen buffer.
  // Because in the original game,
  // sprites with color shift are directly overlaid on the original sprites.
  PAL_BattleDrawAllSpritesWithColorShift(TRUE);
}

VOID PAL_BattleDrawAllSpritesWithColorShift(BOOL fColorShift) {
  // Sort the sprite that need to be drawn
  PAL_BattleSortSpriteObjecByPos();

  for (int i = 0; i < g_Battle.spriteDrawSeqSize; i++) {
    BATTLESPRITE *SpriteObject = &g_Battle.SpriteDrawSeq[i];

    // Draw only sprites with color shift
    if (fColorShift && !SpriteObject->fHaveColorShift)
      continue;

    switch (SpriteObject->wType) {
    case kBattleSpriteTypeEnemy:
      PAL_BattleDrawEnemySprites(SpriteObject->wObjectIndex, g_Battle.lpSceneBuf);
      break;
    case kBattleSpriteTypePlayer:
      PAL_BattleDrawPlayerSprites(SpriteObject->wObjectIndex, g_Battle.lpSceneBuf);
      break;
    case kBattleSpriteTypeMagic:
      PAL_BattleDrawMagicSprites(SpriteObject->wObjectIndex, g_Battle.lpSceneBuf, SpriteObject->pos);
      break;
    }
  }
}

// 将战斗背景、敌人贴图等绘制到 g_Battle.lpSceneBuf 中
VOID PAL_BattleMakeScene(VOID) {
  PAL_BattleDrawBackground();

  if (g_Battle.fSpriteAddLock) {
    // Initialize the drawing sequence
    PAL_BattleClearSpriteObject();
  } else {
    g_Battle.fSpriteAddLock = TRUE;
  }

  // Place enemies and players in the drawing sequence
  PAL_BattleAddFighterSpriteObject();

  // Draw all sprite
  PAL_BattleDrawAllSprites();
}

VOID PAL_BattleFadeScene(VOID) {
  const int rgIndex[6] = {0, 3, 1, 5, 2, 4};

  LPBYTE buf_a = (LPBYTE)(g_Battle.lpSceneBuf->pixels);
  LPBYTE buf_b = (LPBYTE)(gpScreenBak->pixels);

  DWORD time = PAL_GetTicks();
  for (int i = 0; i < 12; i++) {
    for (int j = 0; j < 6; j++) {
      PAL_DelayUntil(time);
      time = PAL_GetTicks() + 16;

      for (int k = rgIndex[j]; k < gpScreen->pitch * gpScreen->h; k += 6) {
        BYTE a = buf_a[k];
        BYTE b = buf_b[k];
        if (i > 0) {
          if ((a & 0x0F) > (b & 0x0F))
            b++;
          else if ((a & 0x0F) < (b & 0x0F))
            b--;
        }
        buf_b[k] = ((a & 0xF0) | (b & 0x0F));
      }

      // Draw the backup buffer to the screen
      VIDEO_RestoreScreen(gpScreen);
      PAL_BattleUIUpdate();
      VIDEO_UpdateScreen(NULL);
    }
  }

  // Draw the result buffer to the screen as the final step
  VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);
  PAL_BattleUIUpdate();
  VIDEO_UpdateScreen(NULL);
}

// The main battle routine.
static BATTLERESULT PAL_BattleMain(VOID) {
  VIDEO_BackupScreen(gpScreen);
  // Generate the scene and draw the scene to the screen buffer
  PAL_BattleMakeScene();
  VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);

  // Fade out the music and delay for a while
  AUDIO_PlayMusic(0, FALSE, 1);
  UTIL_Delay(200);

  // Switch the screen
  VIDEO_SwitchScreen(5);

  // Play the battle music
  AUDIO_PlayMusic(g.wCurBattleMusic, TRUE, 0);

  // Fade in the screen when needed
  if (gNeedToFadeIn) {
    PAL_FadeIn(gCurPalette, gNightPalette, 1);
    gNeedToFadeIn = FALSE;
  }

  // Run the pre-battle scripts for each enemies
  for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
    BATTLE_ENEMY[i].wScriptOnTurnStart = PAL_RunTriggerScript(BATTLE_ENEMY[i].wScriptOnTurnStart, i);
    if (g_Battle.BattleResult != kBattleResultPreBattle)
      break;
  }

  if (g_Battle.BattleResult == kBattleResultPreBattle)
    g_Battle.BattleResult = kBattleResultOnGoing;

  PAL_ClearKeyState();
  uint64_t dwTime = PAL_GetTicks();

  // Run the main battle loop.
  while (g_Battle.BattleResult == kBattleResultOnGoing) {
    PAL_DelayUntil(dwTime);
    dwTime = PAL_GetTicks() + BATTLE_FRAME_TIME;

    // Run the main frame routine.
    PAL_BattleStartFrame();

    // Update the screen.
    VIDEO_UpdateScreen(NULL);
  }

  // Return the battle result
  return g_Battle.BattleResult;
}

static VOID PAL_FreeBattleSprites(VOID) {
  // Free all the loaded sprites
  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
    if (BATTLE_PLAYER[i].lpSprite != NULL)
      free(BATTLE_PLAYER[i].lpSprite);
    BATTLE_PLAYER[i].lpSprite = NULL;
  }

  for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++) {
    if (BATTLE_ENEMY[i].lpSprite != NULL)
      free(BATTLE_ENEMY[i].lpSprite);
    BATTLE_ENEMY[i].lpSprite = NULL;
  }

  if (g_Battle.lpSummonSprite != NULL)
    free(g_Battle.lpSummonSprite);
  g_Battle.lpSummonSprite = NULL;
}

VOID PAL_LoadBattleSprites(VOID) {
  PAL_FreeBattleSprites();

  // Load battle sprites for players
  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
    int s = PAL_GetPlayerBattleSprite(PARTY_PLAYER(i));
    int l = PAL_MKFGetDecompressedSize(s, FP_F);
    if (l <= 0)
      continue;
    BATTLEPLAYER *player = &BATTLE_PLAYER[i];
    player->lpSprite = UTIL_malloc(l);
    PAL_MKFDecompressChunk(player->lpSprite, l, s, FP_F);
    // Set the default position for this player
    int x = g_rgPlayerPos[g.wMaxPartyMemberIndex][i][0];
    int y = g_rgPlayerPos[g.wMaxPartyMemberIndex][i][1];
    player->posOriginal = PAL_XY(x, y);
    player->pos = PAL_XY(x, y);
  }

  // Load battle sprites for enemies
  FILE *fp = UTIL_OpenRequiredFile("abc.mkf");
  for (int i = 0; i < MAX_ENEMIES_IN_TEAM; i++) {
    BATTLEENEMY *enemy = &BATTLE_ENEMY[i];
    if (BATTLE_ENEMY[i].wObjectID == 0)
      continue;
    int l = PAL_MKFGetDecompressedSize(OBJECT[enemy->wObjectID].enemy.wEnemyID, fp);
    if (l <= 0)
      continue;
    enemy->lpSprite = UTIL_malloc(l);
    PAL_MKFDecompressChunk(enemy->lpSprite, l, OBJECT[enemy->wObjectID].enemy.wEnemyID, fp);
    // Set the default position for this enemy
    int x = ENEMY_POS.pos[i][g_Battle.wMaxEnemyIndex].x;
    int y = ENEMY_POS.pos[i][g_Battle.wMaxEnemyIndex].y;
    y += enemy->e.wYPosOffset;
    enemy->posOriginal = PAL_XY(x, y);
    enemy->pos = PAL_XY(x, y);
  }

  fclose(fp);
}

static VOID PAL_LoadBattleBackground(VOID) {
  g_Battle.lpBackground = VIDEO_CreateCompatibleSurface(gpScreen);
  if (g_Battle.lpBackground == NULL)
    TerminateOnError("PAL_LoadBattleBackground(): failed to create surface!");
  BYTE buf[320 * 200];
  PAL_MKFDecompressChunk(buf, 320 * 200, g.wCurBattleField, FP_FBP);
  VIDEO_FBPBlitToSurface(buf, g_Battle.lpBackground);
}

static VOID PAL_DrawLevelUpMessage(int w, PLAYERROLES *ORIG_PLAYER) {
  PAL_CreateSingleLineBox(PAL_XY(80, 0), 10, FALSE);
  PAL_CreateBox(PAL_XY(82, 32), 7, 8, 1, FALSE);

  WCHAR buffer[256] = L"";
  PAL_swprintf(buffer, sizeof(buffer) / sizeof(WCHAR), L"%ls%ls%ls", PAL_GetWord(PLAYER.rgwName[w]),
               PAL_GetWord(STATUS_LABEL_LEVEL), PAL_GetWord(BATTLEWIN_LEVELUP_LABEL));
  PAL_DrawText(buffer, PAL_XY(110, 10), 0, FALSE, FALSE);

  for (int j = 0; j < 8; j++) {
    PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_ARROW), gpScreen, PAL_XY(180, 48 + 18 * j));
  }

  PAL_DrawText(PAL_GetWord(STATUS_LABEL_LEVEL), PAL_XY(100, 44), BATTLEWIN_LEVELUP_LABEL_COLOR, TRUE, FALSE);
  PAL_DrawText(PAL_GetWord(STATUS_LABEL_HP), PAL_XY(100, 62), BATTLEWIN_LEVELUP_LABEL_COLOR, TRUE, FALSE);
  PAL_DrawText(PAL_GetWord(STATUS_LABEL_MP), PAL_XY(100, 80), BATTLEWIN_LEVELUP_LABEL_COLOR, TRUE, FALSE);
  PAL_DrawText(PAL_GetWord(STATUS_LABEL_ATTACKPOWER), PAL_XY(100, 98), BATTLEWIN_LEVELUP_LABEL_COLOR, TRUE, FALSE);
  PAL_DrawText(PAL_GetWord(STATUS_LABEL_MAGICPOWER), PAL_XY(100, 116), BATTLEWIN_LEVELUP_LABEL_COLOR, TRUE, FALSE);
  PAL_DrawText(PAL_GetWord(STATUS_LABEL_RESISTANCE), PAL_XY(100, 134), BATTLEWIN_LEVELUP_LABEL_COLOR, TRUE, FALSE);
  PAL_DrawText(PAL_GetWord(STATUS_LABEL_DEXTERITY), PAL_XY(100, 152), BATTLEWIN_LEVELUP_LABEL_COLOR, TRUE, FALSE);
  PAL_DrawText(PAL_GetWord(STATUS_LABEL_FLEERATE), PAL_XY(100, 170), BATTLEWIN_LEVELUP_LABEL_COLOR, TRUE, FALSE);

  // Draw the original stats and stats after level up
  PAL_DrawNumber(ORIG_PLAYER->rgwLevel[w], 4, PAL_XY(133, 47), kNumColorYellow, kNumAlignRight);
  PAL_DrawNumber(PLAYER.rgwLevel[w], 4, PAL_XY(195, 47), kNumColorYellow, kNumAlignRight);

  PAL_DrawNumber(ORIG_PLAYER->rgwHP[w], 4, PAL_XY(133, 64), kNumColorYellow, kNumAlignRight);
  PAL_DrawNumber(ORIG_PLAYER->rgwMaxHP[w], 4, PAL_XY(154, 68), kNumColorBlue, kNumAlignRight);
  PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_SLASH), gpScreen, PAL_XY(156, 66));
  PAL_DrawNumber(PLAYER.rgwHP[w], 4, PAL_XY(195, 64), kNumColorYellow, kNumAlignRight);
  PAL_DrawNumber(PLAYER.rgwMaxHP[w], 4, PAL_XY(216, 68), kNumColorBlue, kNumAlignRight);
  PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_SLASH), gpScreen, PAL_XY(218, 66));

  PAL_DrawNumber(ORIG_PLAYER->rgwMP[w], 4, PAL_XY(133, 82), kNumColorYellow, kNumAlignRight);
  PAL_DrawNumber(ORIG_PLAYER->rgwMaxMP[w], 4, PAL_XY(154, 86), kNumColorBlue, kNumAlignRight);
  PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_SLASH), gpScreen, PAL_XY(156, 84));
  PAL_DrawNumber(PLAYER.rgwMP[w], 4, PAL_XY(195, 82), kNumColorYellow, kNumAlignRight);
  PAL_DrawNumber(PLAYER.rgwMaxMP[w], 4, PAL_XY(216, 86), kNumColorBlue, kNumAlignRight);
  PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_SLASH), gpScreen, PAL_XY(218, 84));

  PAL_DrawNumber(ORIG_PLAYER->rgwAttackStrength[w] + PAL_GetPlayerAttackStrength(w) - PLAYER.rgwAttackStrength[w], 4,
                 PAL_XY(133, 101), kNumColorYellow, kNumAlignRight);
  PAL_DrawNumber(PAL_GetPlayerAttackStrength(w), 4, PAL_XY(195, 101), kNumColorYellow, kNumAlignRight);

  PAL_DrawNumber(ORIG_PLAYER->rgwMagicStrength[w] + PAL_GetPlayerMagicStrength(w) - PLAYER.rgwMagicStrength[w], 4,
                 PAL_XY(133, 119), kNumColorYellow, kNumAlignRight);
  PAL_DrawNumber(PAL_GetPlayerMagicStrength(w), 4, PAL_XY(195, 119), kNumColorYellow, kNumAlignRight);

  PAL_DrawNumber(ORIG_PLAYER->rgwDefense[w] + PAL_GetPlayerDefense(w) - PLAYER.rgwDefense[w], 4, PAL_XY(133, 137),
                 kNumColorYellow, kNumAlignRight);
  PAL_DrawNumber(PAL_GetPlayerDefense(w), 4, PAL_XY(195, 137), kNumColorYellow, kNumAlignRight);

  PAL_DrawNumber(ORIG_PLAYER->rgwDexterity[w] + PAL_GetPlayerDexterity(w) - PLAYER.rgwDexterity[w], 4, PAL_XY(133, 155),
                 kNumColorYellow, kNumAlignRight);
  PAL_DrawNumber(PAL_GetPlayerDexterity(w), 4, PAL_XY(195, 155), kNumColorYellow, kNumAlignRight);

  PAL_DrawNumber(ORIG_PLAYER->rgwFleeRate[w] + PAL_GetPlayerFleeRate(w) - PLAYER.rgwFleeRate[w], 4, PAL_XY(133, 173),
                 kNumColorYellow, kNumAlignRight);
  PAL_DrawNumber(PAL_GetPlayerFleeRate(w), 4, PAL_XY(195, 173), kNumColorYellow, kNumAlignRight);
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

// Enemy flee the battle.
VOID PAL_BattleEnemyEscape(VOID) {
  AUDIO_PlaySound(45);

  // Show the animation
  BOOL f = TRUE;
  while (f) {
    f = FALSE;
    for (int j = 0; j <= g_Battle.wMaxEnemyIndex; j++) {
      if (BATTLE_ENEMY[j].wObjectID == 0)
        continue;
      BATTLE_ENEMY[j].pos = PAL_XY_OFFSET(BATTLE_ENEMY[j].pos, -5, 0);
      int w = PAL_RLEGetWidth(PAL_SpriteGetFrame(BATTLE_ENEMY[j].lpSprite, 0));
      if (PAL_X(BATTLE_ENEMY[j].pos) + w > 0)
        f = TRUE;
    }

    PAL_BattleMakeScene();
    VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);
    VIDEO_UpdateScreen(NULL);

    UTIL_Delay(10);
  }

  UTIL_Delay(500);
  g_Battle.BattleResult = kBattleResultTerminated;
}

VOID PAL_BattlePlayerEscape(VOID) {
  AUDIO_PlaySound(45);

  PAL_BattleUpdateFighters();

  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
    WORD wPlayerRole = PARTY_PLAYER(i);
    if (PLAYER.rgwHP[wPlayerRole] > 0)
      BATTLE_PLAYER[i].wCurrentFrame = 0;
  }

  for (int i = 0; i < 16; i++) {
    for (int j = 0; j <= g.wMaxPartyMemberIndex; j++) {
      WORD wPlayerRole = PARTY_PLAYER(j);
      if (PLAYER.rgwHP[wPlayerRole] > 0) {
        // TODO: This is still not the same as the original game
        switch (j) {
        case 0:
          if (g.wMaxPartyMemberIndex > 0) {
            BATTLE_PLAYER[j].pos = PAL_XY_OFFSET(BATTLE_PLAYER[j].pos, 4, 6);
            break;
          }
        case 1:
          BATTLE_PLAYER[j].pos = PAL_XY_OFFSET(BATTLE_PLAYER[j].pos, 4, 4);
          break;
        case 2:
          BATTLE_PLAYER[j].pos = PAL_XY_OFFSET(BATTLE_PLAYER[j].pos, 6, 3);
          break;
        }
      }
    }
    PAL_BattleDelay(1, 0, FALSE);
  }

  // Remove all players from the screen
  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++)
    BATTLE_PLAYER[i].pos = PAL_XY(9999, 9999);
  PAL_BattleDelay(1, 0, FALSE);

  g_Battle.BattleResult = kBattleResultFleed;
}

BATTLERESULT
PAL_StartBattle(WORD wEnemyTeam, BOOL fIsBoss) {
  // Set the screen waving effects
  WORD wPrevWaveLevel = g.wScreenWave;
  SHORT sPrevWaveProgression = gWaveProgression;

  gWaveProgression = 0;
  g.wScreenWave = BATTLEFIELD[g.wCurBattleField].wScreenWave;

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

  // Clear all item-using records
  for (int i = 0; i < MAX_INVENTORY; i++) {
    g.rgInventory[i].nAmountInUse = 0;
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

  // Load sprites and background
  PAL_LoadBattleSprites();
  PAL_LoadBattleBackground();

  // Create the surface for scene buffer
  g_Battle.lpSceneBuf = VIDEO_CreateCompatibleSurface(gpScreen);
  if (g_Battle.lpSceneBuf == NULL)
    TerminateOnError("PAL_StartBattle(): creating surface for scene buffer failed!");

  PAL_UpdateEquipmentEffect();

  g_Battle.iExpGained = 0;
  g_Battle.iCashGained = 0;

  g_Battle.fIsBoss = fIsBoss;
  g_Battle.fEnemyCleared = FALSE;
  g_Battle.fEnemyMoving = FALSE;
  g_Battle.iHidingTime = 0;
  g_Battle.wMovingPlayerIndex = 0;

  g_Battle.UI.state = kBattleUIWait;
  g_Battle.UI.fAutoAttack = FALSE;
  g_Battle.UI.iSelectedIndex = 0;
  g_Battle.UI.iPrevEnemyTarget = -1;

  memset(g_Battle.UI.rgShowNum, 0, sizeof(g_Battle.UI.rgShowNum));

  g_Battle.lpSummonSprite = NULL;
  g_Battle.sBackgroundColorShift = 0;

  gInBattle = TRUE;
  g_Battle.BattleResult = kBattleResultPreBattle;
  g_Battle.fSpriteAddLock = TRUE;

  PAL_BattleUpdateFighters();

  // Load the battle effect sprite.
  int l = PAL_MKFGetChunkSize(10, FP_DATA);
  g_Battle.lpEffectSprite = UTIL_malloc(l);
  PAL_MKFReadChunk(g_Battle.lpEffectSprite, l, 10, FP_DATA);

  g_Battle.Phase = kBattlePhaseSelectAction;
  g_Battle.fRepeat = FALSE;
  g_Battle.fForce = FALSE;
  g_Battle.fFlee = FALSE;
  g_Battle.fPrevAutoAtk = FALSE;
  g_Battle.fThisTurnCoop = FALSE;

  // Run the main battle routine.
  int result = PAL_BattleMain();
  // Player won the battle. Add the Experience points.
  if (result == kBattleResultWon)
    PAL_BattleWon();

  // Clear all item-using records
  for (WORD w = 0; w < MAX_INVENTORY; w++)
    g.rgInventory[w].nAmountInUse = 0;

  // Clear all player status, poisons and temporary effects
  PAL_ClearAllPlayerStatus();
  for (WORD w = 0; w < MAX_PLAYER_ROLES; w++) {
    PAL_CurePoisonByLevel(w, 3);
    // 移除梦蛇/愤怒/虚弱的效果
    PAL_RemoveEquipmentEffect(w, kBodyPartExtra);
  }

  // Free all the battle sprites
  PAL_FreeBattleSprites();
  free(g_Battle.lpEffectSprite);
  g_Battle.lpEffectSprite = NULL;

  // Free the surfaces for the background picture and scene buffer
  VIDEO_FreeSurface(g_Battle.lpBackground);
  VIDEO_FreeSurface(g_Battle.lpSceneBuf);

  g_Battle.lpBackground = NULL;
  g_Battle.lpSceneBuf = NULL;

  gInBattle = FALSE;

  AUDIO_PlayMusic(g.wCurMusic, TRUE, 1);

  // Restore the screen waving effects
  gWaveProgression = sPrevWaveProgression;
  g.wScreenWave = wPrevWaveLevel;

  return result;
}
