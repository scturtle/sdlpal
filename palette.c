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

SDL_Color *PAL_GetPalette(INT iPaletteNum, BOOL fNight) {
  static SDL_Color palette[256];
  memset(palette, 0xff, sizeof(SDL_Color) * 256);

  // Read the palette data from the pat.mkf file
  BYTE buf[1536];
  FILE *fp = UTIL_OpenRequiredFile("pat.mkf");
  INT size = PAL_MKFReadChunk(buf, 1536, iPaletteNum, fp);

  fclose(fp);

  if (size < 0) {
    return NULL; // Read failed
  } else if (size <= 256 * 3) {
    fNight = FALSE; // no night colors
  }

  for (int i = 0; i < 256; i++) {
    palette[i].r = buf[(fNight ? 256 * 3 : 0) + i * 3] << 2;
    palette[i].g = buf[(fNight ? 256 * 3 : 0) + i * 3 + 1] << 2;
    palette[i].b = buf[(fNight ? 256 * 3 : 0) + i * 3 + 2] << 2;
  }

  return palette;
}

VOID PAL_SetPalette(INT iPaletteNum, BOOL fNight) {
  SDL_Color *p = PAL_GetPalette(iPaletteNum, fNight);
  if (p != NULL)
    VIDEO_SetPalette(p);
}

VOID PAL_FadeOut(INT iDelay) {
  // get the original palette
  SDL_Color palette[256];
  for (int i = 0; i < 256; i++)
    palette[i] = VIDEO_GetPalette()[i];

  SDL_Color newpalette[256];
  memset(newpalette, 0xff, sizeof(SDL_Color) * 256);

  const int speedup = 1; // by scturtle
  Uint64 time = PAL_GetTicks() + iDelay * 10 * 60 / speedup;
  while (TRUE) {
    int j = (int)(time - PAL_GetTicks()) / iDelay / 10;
    if (j < 0)
      break;

    for (int i = 0; i < 256; i++) {
      newpalette[i].r = (palette[i].r * j) >> 6;
      newpalette[i].g = (palette[i].g * j) >> 6;
      newpalette[i].b = (palette[i].b * j) >> 6;
    }

    VIDEO_SetPalette(newpalette);
    UTIL_Delay(10);
  }

  memset(newpalette, 0, sizeof(newpalette));
  VIDEO_SetPalette(newpalette);
}

VOID PAL_FadeIn(INT iPaletteNum, BOOL fNight, INT iDelay) {
  SDL_Color newpalette[256];
  memset(newpalette, 0xff, sizeof(SDL_Color) * 256);

  // get the new palette
  SDL_Color *palette = PAL_GetPalette(iPaletteNum, fNight);

  const int speedup = 1; // by scturtle
  Uint64 time = PAL_GetTicks() + iDelay * 10 * 60 / speedup;
  while (TRUE) {
    int j = (int)(time - PAL_GetTicks()) / iDelay / 10;
    if (j < 0)
      break;

    j = 60 - j;

    for (int i = 0; i < 256; i++) {
      newpalette[i].r = (palette[i].r * j) >> 6;
      newpalette[i].g = (palette[i].g * j) >> 6;
      newpalette[i].b = (palette[i].b * j) >> 6;
    }

    VIDEO_SetPalette(newpalette);
    UTIL_Delay(10);
  }

  VIDEO_SetPalette(palette);
}

VOID PAL_SceneFade(INT iPaletteNum, BOOL fNight, INT iStep) {
  gNeedToFadeIn = FALSE;

  SDL_Color newpalette[256];
  memset(newpalette, 0xff, sizeof(SDL_Color) * 256);

  SDL_Color *palette;
  palette = PAL_GetPalette(iPaletteNum, fNight);
  if (palette == NULL)
    return;

  if (iStep == 0)
    iStep = 1;
  int i = (iStep > 0) ? 0 : 63;
  while ((iStep > 0 && i < 64) || (iStep < 0 && i >= 0)) {
    for (int i = 0; i < 64; i += iStep) {
      Uint64 time = PAL_GetTicks() + 100;

      // Generate the scene
      PAL_ClearKeyState();
      g_InputState.dir = kDirUnknown;
      PAL_GameUpdate(FALSE);
      PAL_MakeScene();
      VIDEO_UpdateScreen(NULL);

      for (int j = 0; j < 256; j++) {
        newpalette[j].r = (palette[j].r * i) >> 6;
        newpalette[j].g = (palette[j].g * i) >> 6;
        newpalette[j].b = (palette[j].b * i) >> 6;
      }
      VIDEO_SetPalette(newpalette);

      PAL_ProcessEvent();
      while (!SDL_TICKS_PASSED(PAL_GetTicks(), time)) {
        PAL_ProcessEvent();
        PAL_Delay(5);
      }
    }
    i += iStep;
  }
}

VOID PAL_PaletteFade(INT iPaletteNum, BOOL fNight, BOOL fUpdateScene) {
  SDL_Color *newpalette = PAL_GetPalette(iPaletteNum, fNight);
  if (newpalette == NULL)
    return;

  SDL_Color palette[256];
  for (int i = 0; i < 256; i++)
    palette[i] = VIDEO_GetPalette()[i];

  SDL_Color t[256];
  memset(t, 0xff, sizeof(t));

  for (int i = 0; i < 32; i++) {
    Uint64 time = PAL_GetTicks() + (fUpdateScene ? FRAME_TIME : FRAME_TIME / 4);

    for (int j = 0; j < 256; j++) {
      t[j].r = (BYTE)(((INT)(palette[j].r) * (31 - i) + (INT)(newpalette[j].r) * i) / 31);
      t[j].g = (BYTE)(((INT)(palette[j].g) * (31 - i) + (INT)(newpalette[j].g) * i) / 31);
      t[j].b = (BYTE)(((INT)(palette[j].b) * (31 - i) + (INT)(newpalette[j].b) * i) / 31);
    }
    VIDEO_SetPalette(t);

    if (fUpdateScene) {
      PAL_ClearKeyState();
      g_InputState.dir = kDirUnknown;
      PAL_GameUpdate(FALSE);
      PAL_MakeScene();
      VIDEO_UpdateScreen(NULL);
    }

    PAL_ProcessEvent();
    while (!SDL_TICKS_PASSED(PAL_GetTicks(), time)) {
      PAL_ProcessEvent();
      PAL_Delay(5);
    }
  }
}

VOID PAL_ColorFade(INT iDelay, BYTE bColor, BOOL fFrom) {
  SDL_Color newpalette[256];
  memset(newpalette, 0xff, sizeof(SDL_Color) * 255);

  SDL_Color *palette = PAL_GetPalette(gCurPalette, gNightPalette);

  iDelay *= 10;
  if (iDelay == 0)
    iDelay = 10;

  if (fFrom) {
    for (int i = 0; i < 256; i++)
      newpalette[i] = palette[bColor];
  } else {
    memcpy(newpalette, palette, sizeof(newpalette));
  }

  for (int i = 0; i < 64; i++) {
    for (int j = 0; j < 256; j++) {
      SDL_Color target = fFrom ? palette[j] : palette[bColor];
      if (newpalette[j].r > target.r)
        newpalette[j].r -= 4;
      else if (newpalette[j].r < target.r)
        newpalette[j].r += 4;

      if (newpalette[j].g > target.g)
        newpalette[j].g -= 4;
      else if (newpalette[j].g < target.g)
        newpalette[j].g += 4;

      if (newpalette[j].b > target.b)
        newpalette[j].b -= 4;
      else if (newpalette[j].b < target.b)
        newpalette[j].b += 4;
    }

    VIDEO_SetPalette(newpalette);
    UTIL_Delay(iDelay);
  }

  if (!fFrom) {
    for (int i = 0; i < 256; i++)
      newpalette[i] = palette[bColor];
  }

  VIDEO_SetPalette(palette);
}

VOID PAL_FadeToRed(VOID) {
  SDL_Color *palette = PAL_GetPalette(gCurPalette, gNightPalette);

  SDL_Color newpalette[256];
  memcpy(newpalette, palette, sizeof(newpalette));

  for (int i = 0; i < gpScreen->pitch * gpScreen->h; i++) {
    if (((LPBYTE)(gpScreen->pixels))[i] == 0x4F)
      ((LPBYTE)(gpScreen->pixels))[i] = 0x4E; // HACKHACK
  }

  VIDEO_UpdateScreen(NULL);

  for (int i = 0; i < 32; i++) {
    for (int j = 0; j < 256; j++) {
      if (j == 0x4F)
        continue; // so that texts will not be affected

      BYTE color = ((INT)palette[j].r + (INT)palette[j].g + (INT)palette[j].b) / 4 + 64;

      if (newpalette[j].r > color)
        newpalette[j].r -= (newpalette[j].r - color > 8 ? 8 : newpalette[j].r - color);
      else if (newpalette[j].r < color)
        newpalette[j].r += (color - newpalette[j].r > 8 ? 8 : color - newpalette[j].r);

      if (newpalette[j].g > 0)
        newpalette[j].g -= (newpalette[j].g > 8 ? 8 : newpalette[j].g);

      if (newpalette[j].b > 0)
        newpalette[j].b -= (newpalette[j].b > 8 ? 8 : newpalette[j].b);
    }

    VIDEO_SetPalette(newpalette);
    UTIL_Delay(75);
  }
}
