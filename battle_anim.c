#include "main.h"

WORD g_rgPlayerPos[3][3][2] = {
    {{240, 170}},                        // one player
    {{200, 176}, {256, 152}},            // two players
    {{180, 180}, {234, 170}, {270, 146}} // three players
};

VOID PAL_BattleShowPlayerAttackAnim(WORD wPlayerIndex, BOOL fCritical, BOOL fSecondAttack) {
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
    player->pos = fSecondAttack ? PAL_XY(x - 12, y - 8) : PAL_XY(x, y);
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
    player->pos = fSecondAttack ? PAL_XY(x - 12, y - 8) : PAL_XY(x, y);
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

    PAL_BattleUpdateAllEnemyFrame();
    PAL_BattleRenderStart();

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

    PAL_BattleRenderEnd();

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

VOID PAL_BattleShowPlayerThrowItemAnim(WORD wPlayerIndex, WORD wObject, SHORT sTarget) {
  WORD wPlayerRole = PARTY_PLAYER(wPlayerIndex);
  BATTLEPLAYER *player = &BATTLE_PLAYER[wPlayerIndex];

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
}

VOID PAL_BattleShowPlayerUseItemAnim(WORD wPlayerIndex, WORD wObjectID, SHORT sTarget) {
  BATTLEPLAYER *player = &BATTLE_PLAYER[wPlayerIndex];

  PAL_BattleDelay(4, 0, TRUE);
  player->pos = PAL_XY_OFFSET(player->pos, -15, -7);
  player->wCurrentFrame = 5;
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
      PAL_BattleRenderStart();

      PAL_RLEBlitToSurface(b, gpScreen, PAL_XY_OFFSET(pos, -width / 2, -height));

      PAL_BattleRenderEnd();
    }
  }
  PAL_BattleDelay(1, 0, TRUE);
}

VOID PAL_BattleShowPlayerDefMagicAnim(WORD wPlayerIndex, WORD wObjectID, SHORT sTarget) {
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

    PAL_BattleRenderStart();
    PAL_BattleRenderEnd();
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

VOID PAL_BattleShowPlayerOffMagicAnim(WORD wPlayerIndex, WORD wObjectID, SHORT sTarget, BOOL fSummon) {
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

    PAL_BattleRenderStart();
    PAL_BattleRenderEnd();
  }

  g.wScreenWave = wave;
  VIDEO_ShakeScreen(0, 0);

  free(lpSpriteEffect);

  // 恢复敌人坐标
  for (int i = 0; i <= g_Battle.wMaxEnemyIndex; i++)
    BATTLE_ENEMY[i].pos = BATTLE_ENEMY[i].posOriginal;
}

VOID PAL_BattleShowPlayerSummonMagicAnim(WORD wObjectID) {
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

      PAL_BattleRenderStart();
      PAL_BattleRenderEnd();

      g_Battle.iSummonFrame++;
    }
  }

  // Show the actual magic effect
  PAL_BattleShowPlayerOffMagicAnim((WORD)-1, wEffectMagicID, -1, TRUE);
}

VOID PAL_BattleShowPostMagicAnim(VOID) {
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

VOID PAL_BattleShowEnemyMagicAnim(WORD wEnemyIndex, WORD wObjectID, SHORT sTarget) {
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

    PAL_BattleRenderStart();
    PAL_BattleRenderEnd();
  }

  g.wScreenWave = wave;
  VIDEO_ShakeScreen(0, 0);

  free(lpSpriteEffect);

  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++)
    BATTLE_PLAYER[i].pos = BATTLE_PLAYER[i].posOriginal;
}

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

    PAL_BattleRenderStart();
    PAL_BattleRenderEnd();

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
