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
#include <float.h>

SDL_Window *gpWindow = NULL;
SDL_Renderer *gpRenderer = NULL;
SDL_Texture *gpTexture = NULL;
SDL_Surface *gpScreen = NULL;     // 8-bit 原始游戏画布 (320x200)
SDL_Surface *gpScreenBak = NULL;  // 备份画布
SDL_Surface *gpScreenReal = NULL; // 32-bit 转换后画布 (320x200)
static SDL_Palette *gpPalette = NULL;

volatile BOOL g_bRenderPaused = FALSE;
static BOOL bScaleScreen = TRUE;

// 震动参数
static WORD g_wShakeTime = 0;
static WORD g_wShakeLevel = 0;

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
  // 创建窗口
  gpWindow = SDL_CreateWindow("Pal", gConfig.dwScreenWidth, gConfig.dwScreenHeight, SDL_WINDOW_RESIZABLE);
  if (!gpWindow)
    return -1;

  // 创建渲染器
  gpRenderer = SDL_CreateRenderer(gpWindow, NULL);
  if (!gpRenderer)
    return -1;
  // SDL_SetRenderVSync(gpRenderer, 1); // 开启 VSync

  // 设置逻辑分辨率
  SDL_SetRenderLogicalPresentation(gpRenderer, 320, 200, SDL_LOGICAL_PRESENTATION_LETTERBOX);

  gpScreen = SDL_CreateSurface(320, 200, SDL_PIXELFORMAT_INDEX8);
  gpScreenBak = SDL_CreateSurface(320, 200, SDL_PIXELFORMAT_INDEX8);
  // 32-bit Surface 用于像素转换
  gpScreenReal = SDL_CreateSurface(320, 200, SDL_PIXELFORMAT_XRGB8888);

  // 创建纹理
  gpTexture = SDL_CreateTexture(gpRenderer, SDL_PIXELFORMAT_XRGB8888, SDL_TEXTUREACCESS_STREAMING, 320, 200);

  // 设置缩放质量
  SDL_SetTextureScaleMode(gpTexture, SDL_SCALEMODE_NEAREST);

  // 调色板
  gpPalette = SDL_CreatePalette(256);

  if (!gpScreen || !gpScreenBak || !gpScreenReal || !gpTexture || !gpPalette) {
    VIDEO_Shutdown();
    return -2;
  }
  return 0;
}

VOID VIDEO_Shutdown(VOID) {
  SDL_DestroyTexture(gpTexture);
  SDL_DestroySurface(gpScreen);
  SDL_DestroySurface(gpScreenBak);
  SDL_DestroySurface(gpScreenReal);
  if (gpPalette)
    SDL_DestroyPalette(gpPalette);
  SDL_DestroyRenderer(gpRenderer);
  SDL_DestroyWindow(gpWindow);

  gpTexture = NULL;
  gpScreen = NULL;
  gpRenderer = NULL;
  gpWindow = NULL;
}

VOID VIDEO_RenderCopy(VOID) {
  if (g_bRenderPaused)
    return;
  SDL_UpdateTexture(gpTexture, NULL, gpScreenReal->pixels, gpScreenReal->pitch);
  SDL_RenderClear(gpRenderer);
  SDL_RenderTexture(gpRenderer, gpTexture, NULL, NULL);
  SDL_RenderPresent(gpRenderer);
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

    SDL_ClearSurface(gpScreenReal, 0, 0, 0, 0); // 清空背景
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

VOID VIDEO_ToggleFullscreen(VOID) {
  gConfig.fFullScreen = !gConfig.fFullScreen;
  SDL_SetWindowFullscreen(gpWindow, gConfig.fFullScreen);
}

VOID VIDEO_ShakeScreen(WORD wShakeTime, WORD wShakeLevel) {
  g_wShakeTime = wShakeTime;
  g_wShakeLevel = wShakeLevel;
}

// switch the screen from gpScreenBak to gpScreen with effect
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

// Fade from the backup screen buffer to the current screen buffer
VOID VIDEO_FadeScreen(WORD wSpeed) {
  const int rgIndex[6] = {0, 3, 1, 5, 2, 4};
  SDL_Rect dstrect;
  short offset = 240 - 200;
  short screenRealHeight = gpScreenReal->h;
  short screenRealY = 0;

  if (SDL_MUSTLOCK(gpScreenReal)) {
    if (!SDL_LockSurface(gpScreenReal))
      return;
  }

  if (!bScaleScreen) {
    screenRealHeight -= offset;
    screenRealY = offset / 2;
  }

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

      //
      // Blend the pixels in the 2 buffers, and put the result into the
      // backup buffer
      //
      for (int k = rgIndex[j]; k < gpScreen->pitch * gpScreen->h; k += 6) {
        BYTE a = ((LPBYTE)(gpScreen->pixels))[k];
        BYTE b = ((LPBYTE)(gpScreenBak->pixels))[k];

        if (i > 0) {
          if ((a & 0x0F) > (b & 0x0F)) {
            b++;
          } else if ((a & 0x0F) < (b & 0x0F)) {
            b--;
          }
        }

        ((LPBYTE)(gpScreenBak->pixels))[k] = ((a & 0xF0) | (b & 0x0F));
      }

      //
      // Draw the backup buffer to the screen
      //
      if (g_wShakeTime != 0) {
        //
        // Shake the screen
        //
        SDL_Rect srcrect, dstrect;

        srcrect.x = 0;
        srcrect.y = 0;
        srcrect.w = 320;
        srcrect.h = 200 - g_wShakeLevel;

        dstrect.x = 0;
        dstrect.y = screenRealY;
        dstrect.w = 320 * gpScreenReal->w / gpScreen->w;
        dstrect.h = (200 - g_wShakeLevel) * screenRealHeight / gpScreen->h;

        if (g_wShakeTime & 1) {
          srcrect.y = g_wShakeLevel;
        } else {
          dstrect.y = (screenRealY + g_wShakeLevel) * screenRealHeight / gpScreen->h;
        }

        SDL_BlitSurface(gpScreenBak, &srcrect, gpScreenReal, &dstrect);

        if (g_wShakeTime & 1) {
          dstrect.y = (screenRealY + screenRealHeight - g_wShakeLevel) * screenRealHeight / gpScreen->h;
        } else {
          dstrect.y = screenRealY;
        }

        dstrect.h = g_wShakeLevel * screenRealHeight / gpScreen->h;

        SDL_FillSurfaceRect(gpScreenReal, &dstrect, 0);
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

  if (SDL_MUSTLOCK(gpScreenReal)) {
    SDL_UnlockSurface(gpScreenReal);
  }

  //
  // Draw the result buffer to the screen as the final step
  //
  VIDEO_UpdateScreen(NULL);
}

void VIDEO_SetWindowTitle(const char *pszTitle) { SDL_SetWindowTitle(gpWindow, pszTitle); }

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
  // 自动缩放绘制到 Real 面板
  SDL_BlitSurfaceScaled(pSurface, NULL, gpScreenReal, NULL, SDL_SCALEMODE_LINEAR);
  VIDEO_RenderCopy();
}
