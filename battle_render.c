#include "main.h"

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

VOID PAL_FreeBattleSprites(VOID) {
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

static VOID PAL_LoadBattleBackground(VOID) {
  g_Battle.lpBackground = VIDEO_CreateCompatibleSurface(gpScreen);
  if (g_Battle.lpBackground == NULL)
    TerminateOnError("PAL_LoadBattleBackground(): failed to create surface!");
  BYTE buf[320 * 200];
  PAL_MKFDecompressChunk(buf, 320 * 200, g.wCurBattleField, FP_FBP);
  VIDEO_FBPBlitToSurface(buf, g_Battle.lpBackground);
}

static VOID PAL_FreeBattleBackground(VOID) {
  VIDEO_FreeSurface(g_Battle.lpBackground);
  g_Battle.lpBackground = NULL;
}

static WORD wPrevWaveLevel;
static SHORT sPrevWaveProgression;

VOID PAL_LoadBattleRenderResources(VOID) {
  PAL_LoadBattleSprites();
  PAL_LoadBattleBackground();

  g_Battle.lpSceneBuf = VIDEO_CreateCompatibleSurface(gpScreen);

  int l = PAL_MKFGetChunkSize(10, FP_DATA);
  g_Battle.lpEffectSprite = UTIL_malloc(l);
  PAL_MKFReadChunk(g_Battle.lpEffectSprite, l, 10, FP_DATA);

  // Backup and set the screen waving effects
  wPrevWaveLevel = g.wScreenWave;
  g.wScreenWave = BATTLEFIELD[g.wCurBattleField].wScreenWave;
  sPrevWaveProgression = gWaveProgression;
  gWaveProgression = 0;

  PAL_BattleUpdateFighters();
}

VOID PAL_FreeBattleRenderResources(VOID) {
  PAL_FreeBattleSprites();
  PAL_FreeBattleBackground();

  VIDEO_FreeSurface(g_Battle.lpSceneBuf);
  g_Battle.lpSceneBuf = NULL;

  free(g_Battle.lpEffectSprite);
  g_Battle.lpEffectSprite = NULL;

  // Restore the screen waving effects
  gWaveProgression = sPrevWaveProgression;
  g.wScreenWave = wPrevWaveLevel;
}

// 从 g_Battle.lpBackground 绘制到 g_Battle.lpSceneBuf
static VOID PAL_BattleDrawBackground(VOID) {
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
      else if (player->fDefending)
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

VOID PAL_BattleUpdateAllEnemyFrame() {
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

static VOID PAL_BattleDrawEnemySprites(WORD wEnemyIndex, SDL_Surface *lpDstSurface) {
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

static VOID PAL_BattleDrawPlayerSprites(WORD wPlayerIndex, SDL_Surface *lpDstSurface) {
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

static VOID PAL_BattleDrawMagicSprites(INT iMagicNum, SDL_Surface *lpDstSurface, PAL_POS pos) {
  LPCBITMAPRLE lpBitmap = g_Battle.lpMagicBitmap;
  PAL_RLEBlitToSurface(lpBitmap, lpDstSurface,
                       PAL_XY_OFFSET(pos, -PAL_RLEGetWidth(lpBitmap) / 2, -PAL_RLEGetHeight(lpBitmap)));
}

static VOID PAL_BattleClearSpriteObject(VOID) {
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

static VOID PAL_BattleSortSpriteObjecByPos(VOID) {
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

static VOID PAL_BattleDrawAllSpritesWithColorShift(BOOL fColorShift) {
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

  // Sort the sprite that need to be drawn
  PAL_BattleSortSpriteObjecByPos();

  // Draw all sprites (non-color-shifted first, then color-shifted)
  PAL_BattleDrawAllSpritesWithColorShift(FALSE);

  // Only draw sprites with color shift into the battle screen buffer.
  // Because in the original game,
  // sprites with color shift are directly overlaid on the original sprites.
  PAL_BattleDrawAllSpritesWithColorShift(TRUE);
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
      PAL_BattleRenderEnd();
    }
  }

  // Draw the result buffer to the screen as the final step
  VIDEO_CopyEntireSurface(g_Battle.lpSceneBuf, gpScreen);
  PAL_BattleRenderEnd();
}

VOID PAL_DrawLevelUpMessage(int w, PLAYERROLES *ORIG_PLAYER) {
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
