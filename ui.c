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

static LPSPRITE gpSpriteUI = NULL;

static LPBOX PAL_CreateBoxInternal(const SDL_Rect *rect) {
  LPBOX lpBox = (LPBOX)calloc(1, sizeof(BOX));
  if (lpBox == NULL)
    return NULL;
  lpBox->rect = *rect;
  SDL_Surface *bak = VIDEO_CreateCompatibleSizedSurface(gpScreen, rect);
  if (bak == NULL) {
    free(lpBox);
    return NULL;
  }
  VIDEO_CopySurface(gpScreen, rect, bak, NULL);
  lpBox->lpSavedArea = bak;
  return lpBox;
}

INT PAL_InitUI(VOID) {
  int size = PAL_MKFGetChunkSize(9, FP_DATA);
  gpSpriteUI = UTIL_malloc(size);
  PAL_MKFReadChunk(gpSpriteUI, size, 9, FP_DATA);
  return 0;
}

VOID PAL_FreeUI(VOID) {
  if (gpSpriteUI != NULL) {
    free(gpSpriteUI);
    gpSpriteUI = NULL;
  }
}

LPCBITMAPRLE PAL_GetUISprite(int spriteNum) { return PAL_SpriteGetFrame(gpSpriteUI, spriteNum); }

LPBOX
PAL_CreateBox(PAL_POS pos, INT nRows, INT nColumns, INT iStyle, BOOL fSaveScreen) {
  return PAL_CreateBoxWithShadow(pos, nRows, nColumns, iStyle, fSaveScreen, 6);
}

LPBOX
PAL_CreateBoxWithShadow(PAL_POS pos, INT nRows, INT nColumns, INT iStyle, BOOL fSaveScreen, INT nShadowOffset) {
  SDL_Rect rect;
  rect.x = PAL_X(pos);
  rect.y = PAL_Y(pos);
  rect.w = 0;
  rect.h = 0;

  // Main menu from 0 and item menu from 9.
  LPCBITMAPRLE border[3][3];
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      border[i][j] = PAL_GetUISprite(i * 3 + j + iStyle * 9);

  // Get the total width and total height of the box
  for (int i = 0; i < 3; i++) {
    if (i == 1) {
      rect.w += PAL_RLEGetWidth(border[0][i]) * nColumns;
      rect.h += PAL_RLEGetHeight(border[i][0]) * nRows;
    } else {
      rect.w += PAL_RLEGetWidth(border[0][i]);
      rect.h += PAL_RLEGetHeight(border[i][0]);
    }
  }

  // Include shadow
  rect.w += nShadowOffset;
  rect.h += nShadowOffset;

  LPBOX lpBox = NULL;
  if (fSaveScreen) {
    lpBox = PAL_CreateBoxInternal(&rect);
  }

  // Border takes 2 additional rows and columns
  nRows += 2;
  nColumns += 2;

  // Draw the box
  for (int i = 0; i < nRows; i++) {
    int x = rect.x;
    int m = (i == 0) ? 0 : ((i == nRows - 1) ? 2 : 1);
    for (int j = 0; j < nColumns; j++) {
      int n = (j == 0) ? 0 : ((j == nColumns - 1) ? 2 : 1);
      PAL_RLEBlitToSurfaceWithShadow(border[m][n], gpScreen, PAL_XY(x + nShadowOffset, rect.y + nShadowOffset), TRUE);
      PAL_RLEBlitToSurface(border[m][n], gpScreen, PAL_XY(x, rect.y));
      x += PAL_RLEGetWidth(border[m][n]);
    }
    rect.y += PAL_RLEGetHeight(border[m][0]);
  }

  return lpBox;
}

LPBOX
PAL_CreateSingleLineBox(PAL_POS pos, INT nLen, BOOL fSaveScreen) {
  return PAL_CreateSingleLineBoxWithShadow(pos, nLen, fSaveScreen, 6);
}

LPBOX
PAL_CreateSingleLineBoxWithShadow(PAL_POS pos, INT nLen, BOOL fSaveScreen, INT nShadowOffset) {
  // Get the bitmaps
  LPCBITMAPRLE lpBitmapLeft = PAL_GetUISprite(44);
  LPCBITMAPRLE lpBitmapMid = PAL_GetUISprite(45);
  LPCBITMAPRLE lpBitmapRight = PAL_GetUISprite(46);

  SDL_Rect rect;
  rect.x = PAL_X(pos);
  rect.y = PAL_Y(pos);

  // Get the total width and total height of the box
  rect.w = PAL_RLEGetWidth(lpBitmapLeft) + PAL_RLEGetWidth(lpBitmapMid) * nLen + PAL_RLEGetWidth(lpBitmapRight);
  rect.h = PAL_RLEGetHeight(lpBitmapLeft);

  // Include shadow
  rect.w += nShadowOffset;
  rect.h += nShadowOffset;

  LPBOX lpBox = NULL;
  if (fSaveScreen) {
    lpBox = PAL_CreateBoxInternal(&rect);
  }

  // Draw the shadow
  int x = rect.x;
  PAL_RLEBlitToSurfaceWithShadow(lpBitmapLeft, gpScreen, PAL_XY(x + nShadowOffset, rect.y + nShadowOffset), TRUE);
  x += PAL_RLEGetWidth(lpBitmapLeft);
  for (int i = 0; i < nLen; i++) {
    PAL_RLEBlitToSurfaceWithShadow(lpBitmapMid, gpScreen, PAL_XY(x + nShadowOffset, rect.y + nShadowOffset), TRUE);
    x += PAL_RLEGetWidth(lpBitmapMid);
  }
  PAL_RLEBlitToSurfaceWithShadow(lpBitmapRight, gpScreen, PAL_XY(x + nShadowOffset, rect.y + nShadowOffset), TRUE);

  // Draw the box
  PAL_RLEBlitToSurface(lpBitmapLeft, gpScreen, pos);
  rect.x += PAL_RLEGetWidth(lpBitmapLeft);
  for (int i = 0; i < nLen; i++) {
    PAL_RLEBlitToSurface(lpBitmapMid, gpScreen, PAL_XY(rect.x, rect.y));
    rect.x += PAL_RLEGetWidth(lpBitmapMid);
  }
  PAL_RLEBlitToSurface(lpBitmapRight, gpScreen, PAL_XY(rect.x, rect.y));

  return lpBox;
}

VOID PAL_DeleteBox(LPBOX lpBox) {
  if (lpBox == NULL)
    return;
  // Restore the saved screen part
  SDL_Rect rect = lpBox->rect;
  VIDEO_CopySurface(lpBox->lpSavedArea, NULL, gpScreen, &rect);
  // Free the memory used by the box
  VIDEO_FreeSurface(lpBox->lpSavedArea);
  free(lpBox);
}

VOID PAL_DrawNumber(UINT iNum, UINT nLength, PAL_POS pos, NUMCOLOR color, NUMALIGN align) {
  // Get the bitmaps. Blue starts from 29, Cyan from 56, Yellow from 19.
  int idx = (color == kNumColorBlue) ? 29 : ((color == kNumColorCyan) ? 56 : 19);

  LPCBITMAPRLE rglpBitmap[10];
  for (int i = 0; i < 10; i++) {
    rglpBitmap[i] = PAL_GetUISprite(idx + i);
  }

  // Calculate the actual length of the number.
  UINT n = iNum;
  UINT nActualLength = 0;
  while (n > 0) {
    n /= 10;
    nActualLength++;
  }

  if (nActualLength > nLength) {
    nActualLength = nLength;
  } else if (nActualLength == 0) {
    nActualLength = 1;
  }

  int x = PAL_X(pos) - 6;
  int y = PAL_Y(pos);

  switch (align) {
  case kNumAlignLeft:
    x += 6 * nActualLength;
    break;
  case kNumAlignMid:
    x += 3 * (nLength + nActualLength);
    break;
  case kNumAlignRight:
    x += 6 * nLength;
    break;
  }

  // Draw the number.
  while (nActualLength-- > 0) {
    PAL_RLEBlitToSurface(rglpBitmap[iNum % 10], gpScreen, PAL_XY(x, y));
    x -= 6;
    iNum /= 10;
  }
}
