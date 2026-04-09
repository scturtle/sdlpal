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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "main.h"

#define WIDTH 320
#define HEIGHT 200

/* ════════════════════════════════════════════════════════════════════════
 * 全局变量 & SDL Surface 状态 (移除了 Window, Renderer 和 Texture)
 * ════════════════════════════════════════════════════════════════════════ */
SDL_Surface *gpScreen = NULL;     // 8-bit 原始游戏画布 (320x200)
SDL_Surface *gpScreenBak = NULL;  // 备份画布
SDL_Surface *gpScreenReal = NULL; // 32-bit 转换后画布 (320x200)
static SDL_Palette *gpPalette = NULL;

volatile BOOL g_bRenderPaused = FALSE;

// 震动参数
static WORD g_wShakeTime = 0;
static WORD g_wShakeLevel = 0;

/* ════════════════════════════════════════════════════════════════════════
 * Base64 & Kitty Graphics Protocol 核心渲染器
 * ════════════════════════════════════════════════════════════════════════ */
static size_t base64_encode(const uint8_t *in, size_t len, char *out) {
  static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  char *p = out;
  for (size_t i = 0; i < len; i += 3) {
    uint32_t w = (in[i] << 16) | ((i + 1 < len ? in[i + 1] : 0) << 8) | ((i + 2 < len ? in[i + 2] : 0));
    *p++ = b64[(w >> 18) & 0x3f];
    *p++ = b64[(w >> 12) & 0x3f];
    *p++ = (i + 1 < len) ? b64[(w >> 6) & 0x3f] : '=';
    *p++ = (i + 2 < len) ? b64[w & 0x3f] : '=';
  }
  return p - out;
}

static struct {
  long id;
  int frame;
  char b64[256005];
  char proto[300000];
} ctx;

static void render_init() {
  printf("\033[?25l\033[2J\033[H");
  fflush(stdout);
}

static void render_done(long id) {
  printf("\033_Ga=d,q=2,i=%ld;\033\\\033[H\033[2J\033[?25h", id);
  fflush(stdout);
}

static void render_frame(const uint8_t *rgb) {
  size_t b64_len = base64_encode(rgb, WIDTH * HEIGHT * 3, ctx.b64);
  char *buf = ctx.proto;
  size_t off = 0;

  for (size_t i = 0; i < b64_len; i += 4096) {
    int more = (i + 4096 < b64_len) ? 1 : 0;
    size_t chunk = more ? 4096 : (b64_len - i);

    if (i == 0) {
      if (ctx.frame == 0)
        // c=80 让图像占据终端的 80 列宽度进行缩放
        off += sprintf(buf + off, "\033_Ga=T,i=%ld,f=24,s=%d,v=%d,q=2,c=80,m=%d;", ctx.id, WIDTH, HEIGHT, more);
      else
        off += sprintf(buf + off, "\033_Ga=f,r=1,i=%ld,f=24,q=2,s=%d,v=%d,m=%d;", ctx.id, WIDTH, HEIGHT, more);
    } else {
      if (ctx.frame == 0)
        off += sprintf(buf + off, "\033_Gm=%d;", more);
      else
        off += sprintf(buf + off, "\033_Ga=f,r=1,q=2,m=%d;", more);
    }

    memcpy(buf + off, ctx.b64 + i, chunk);
    off += chunk;
    memcpy(buf + off, "\033\\", 2);
    off += 2;
  }

  if (ctx.frame > 0)
    off += sprintf(buf + off, "\033_Ga=a,q=2,c=1,i=%ld;\033\\", ctx.id);
  else {
    memcpy(buf + off, "\r\n", 2);
    off += 2;
  }

  // 重置光标到左上角，防止画面滚动错位
  off += sprintf(buf + off, "\033[H");

  fwrite(ctx.proto, 1, off, stdout);
  fflush(stdout);
  ctx.frame++;
}

/* ════════════════════════════════════════════════════════════════════════
 * 视频子系统接口重写
 * ════════════════════════════════════════════════════════════════════════ */

VOID VIDEO_CopySurface(SDL_Surface *s, const SDL_Rect *sr, SDL_Surface *t, const SDL_Rect *tr) {
  SDL_BlitSurface((s), (sr), (t), (tr));
}
VOID VIDEO_CopyEntireSurface(SDL_Surface *s, SDL_Surface *t) { SDL_BlitSurface((s), NULL, (t), NULL); }
VOID VIDEO_BackupScreen(SDL_Surface *s) { SDL_BlitSurface((s), NULL, gpScreenBak, NULL); }
VOID VIDEO_RestoreScreen(SDL_Surface *t) { SDL_BlitSurface(gpScreenBak, NULL, (t), NULL); }
VOID VIDEO_FreeSurface(SDL_Surface *s) { SDL_DestroySurface(s); }
VOID VIDEO_FillScreenBlack() { SDL_FillSurfaceRect(gpScreen, NULL, 0); }

INT VIDEO_FBPBlitToSurface(LPBYTE lpBitmapFBP, SDL_Surface *lpDstSurface) {
  if (lpBitmapFBP == NULL || lpDstSurface == NULL)
    return -1;
  SDL_Surface *temp = SDL_CreateSurfaceFrom(320, 200, SDL_PIXELFORMAT_INDEX8, (void *)lpBitmapFBP, 320);
  if (!temp)
    return -1;
  int result = SDL_BlitSurface(temp, NULL, lpDstSurface, NULL);
  SDL_DestroySurface(temp);
  return result == 0 ? 0 : -1;
}

INT VIDEO_Startup(VOID) {
  // 1. 初始化终端显示环境
  render_init();
  srand((unsigned)time(NULL));
  ctx.id = (long)rand() | 1;
  ctx.frame = 0;

  // 2. 初始化 SDL 面板 (用于内存渲染和特效处理)
  gpScreen = SDL_CreateSurface(WIDTH, HEIGHT, SDL_PIXELFORMAT_INDEX8);
  gpScreenBak = SDL_CreateSurface(WIDTH, HEIGHT, SDL_PIXELFORMAT_INDEX8);
  gpScreenReal = SDL_CreateSurface(WIDTH, HEIGHT, SDL_PIXELFORMAT_XRGB8888);
  gpPalette = SDL_CreatePalette(256);

  if (!gpScreen || !gpScreenBak || !gpScreenReal || !gpPalette) {
    VIDEO_Shutdown();
    return -2;
  }
  return 0;
}

VOID VIDEO_Shutdown(VOID) {
  // 清理终端显示
  render_done(ctx.id);

  // 清理 SDL 内存
  if (gpScreen)
    SDL_DestroySurface(gpScreen);
  if (gpScreenBak)
    SDL_DestroySurface(gpScreenBak);
  if (gpScreenReal)
    SDL_DestroySurface(gpScreenReal);
  if (gpPalette)
    SDL_DestroyPalette(gpPalette);

  gpScreen = NULL;
  gpScreenBak = NULL;
  gpScreenReal = NULL;
  gpPalette = NULL;
}

VOID VIDEO_RenderCopy(VOID) {
  if (g_bRenderPaused)
    return;

  // 锁定画布以安全读取像素
  if (SDL_MUSTLOCK(gpScreenReal))
    SDL_LockSurface(gpScreenReal);

  static uint8_t rgb24[WIDTH * HEIGHT * 3];

  // 将 SDL_PIXELFORMAT_XRGB8888 转换为 紧凑的 24-bit RGB
  for (int y = 0; y < HEIGHT; y++) {
    uint32_t *row = (uint32_t *)((uint8_t *)gpScreenReal->pixels + y * gpScreenReal->pitch);
    for (int x = 0; x < WIDTH; x++) {
      uint32_t pixel = row[x];
      int idx = (y * WIDTH + x) * 3;
      rgb24[idx + 0] = (pixel >> 16) & 0xFF; // R
      rgb24[idx + 1] = (pixel >> 8) & 0xFF;  // G
      rgb24[idx + 2] = pixel & 0xFF;         // B
    }
  }

  if (SDL_MUSTLOCK(gpScreenReal))
    SDL_UnlockSurface(gpScreenReal);

  // 刷新到终端
  render_frame(rgb24);
}

VOID VIDEO_UpdateScreen(const SDL_Rect *lpRect) {
  if (g_bRenderPaused)
    return;

  if (g_wShakeTime != 0) {
    SDL_Rect src = {0, 0, 320, 200 - g_wShakeLevel};
    SDL_Rect dst = {0, 0, 320, 200 - g_wShakeLevel};

    if (g_wShakeTime & 1)
      src.y = g_wShakeLevel;
    else
      dst.y = g_wShakeLevel;

    SDL_FillSurfaceRect(gpScreenReal, NULL, 0); // 清空背景
    SDL_BlitSurface(gpScreen, &src, gpScreenReal, &dst);
    g_wShakeTime--;
  } else {
    SDL_BlitSurface(gpScreen, lpRect, gpScreenReal, lpRect);
  }

  VIDEO_RenderCopy();
}

VOID VIDEO_SetPalette(SDL_Color rgPalette[256]) {
  SDL_SetPaletteColors(gpPalette, rgPalette, 0, 256);
  SDL_SetSurfacePalette(gpScreen, gpPalette);
  SDL_SetSurfacePalette(gpScreenBak, gpPalette);
  // 刷新全屏
  VIDEO_UpdateScreen(NULL);
}

SDL_Color *VIDEO_GetPalette(VOID) { return gpPalette->colors; }

VOID VIDEO_ToggleFullscreen(VOID) { gConfig.fFullScreen = !gConfig.fFullScreen; }

VOID VIDEO_ShakeScreen(WORD wShakeTime, WORD wShakeLevel) {
  g_wShakeTime = wShakeTime;
  g_wShakeLevel = wShakeLevel;
}

VOID VIDEO_SwitchScreen(WORD wSpeed) {
  const int rgIndex[6] = {0, 3, 1, 5, 2, 4};
  wSpeed = (wSpeed + 1) * 10;

  uint8_t *src = (uint8_t *)gpScreen->pixels;
  uint8_t *dst = (uint8_t *)gpScreenBak->pixels;

  for (int i = 0; i < 6; i++) {
    for (int j = rgIndex[i]; j < gpScreen->pitch * gpScreen->h; j += 6)
      dst[j] = src[j];

    VIDEO_CopyEntireSurface(gpScreenBak, gpScreenReal);
    VIDEO_RenderCopy();
    PAL_Delay(wSpeed);
  }
}

VOID VIDEO_FadeScreen(WORD wSpeed) {
  const int rgIndex[6] = {0, 3, 1, 5, 2, 4};
  SDL_Rect dstrect;
  short screenRealHeight = gpScreenReal->h;
  short screenRealY = 0;

  DWORD time = PAL_GetTicks();

  wSpeed++;
  wSpeed *= 10;

  for (int i = 0; i < 12; i++) {
    for (int j = 0; j < 6; j++) {
      PAL_ProcessEvent();
      while (!SDL_TICKS_PASSED(PAL_GetTicks(), time)) {
        PAL_ProcessEvent();
        PAL_Delay(5);
      }
      time = PAL_GetTicks() + wSpeed;

      for (int k = rgIndex[j]; k < gpScreen->pitch * gpScreen->h; k += 6) {
        BYTE a = ((LPBYTE)(gpScreen->pixels))[k];
        BYTE b = ((LPBYTE)(gpScreenBak->pixels))[k];

        if (i > 0) {
          if ((a & 0x0F) > (b & 0x0F))
            b++;
          else if ((a & 0x0F) < (b & 0x0F))
            b--;
        }
        ((LPBYTE)(gpScreenBak->pixels))[k] = ((a & 0xF0) | (b & 0x0F));
      }

      if (g_wShakeTime != 0) {
        SDL_Rect srcrect, dstrect;
        srcrect.x = 0;
        srcrect.y = 0;
        srcrect.w = 320;
        srcrect.h = 200 - g_wShakeLevel;

        dstrect.x = 0;
        dstrect.y = screenRealY;
        dstrect.w = 320 * gpScreenReal->w / gpScreen->w;
        dstrect.h = (200 - g_wShakeLevel) * screenRealHeight / gpScreen->h;

        if (g_wShakeTime & 1)
          srcrect.y = g_wShakeLevel;
        else
          dstrect.y = (screenRealY + g_wShakeLevel) * screenRealHeight / gpScreen->h;

        SDL_FillSurfaceRect(gpScreenReal, NULL, 0); // 清空背景
        SDL_BlitSurface(gpScreenBak, &srcrect, gpScreenReal, &dstrect);
        VIDEO_RenderCopy();
        g_wShakeTime--;
      } else {
        dstrect.x = 0;
        dstrect.y = screenRealY;
        dstrect.w = gpScreenReal->w;
        dstrect.h = screenRealHeight;

        SDL_BlitSurface(gpScreenBak, NULL, gpScreenReal, &dstrect);
        VIDEO_RenderCopy();
      }
    }
  }
  VIDEO_UpdateScreen(NULL);
}

void VIDEO_SetWindowTitle(const char *pszTitle) { printf("\033]2;%s\033\\", pszTitle); }

SDL_Surface *VIDEO_CreateCompatibleSurface(SDL_Surface *pSource) {
  return VIDEO_CreateCompatibleSizedSurface(pSource, NULL);
}

SDL_Surface *VIDEO_CreateCompatibleSizedSurface(SDL_Surface *pSource, const SDL_Rect *pSize) {
  SDL_Surface *dest = SDL_CreateSurface(pSize ? pSize->w : pSource->w, pSize ? pSize->h : pSource->h, pSource->format);
  if (dest && gpPalette) {
    SDL_SetSurfacePalette(dest, gpPalette);
  }
  return dest;
}

VOID VIDEO_DrawSurfaceToScreen(SDL_Surface *pSurface) {
  if (g_bRenderPaused)
    return;
  SDL_BlitSurfaceScaled(pSurface, NULL, gpScreenReal, NULL, SDL_SCALEMODE_LINEAR);
  VIDEO_RenderCopy();
}
