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

#include "map.h"

LPPALMAP
PAL_LoadMap(INT iMapNum) {
  FILE *fpMAP = UTIL_OpenRequiredFile("map.mkf");
  FILE *fpGOP = UTIL_OpenRequiredFile("gop.mkf");
  BYTE buf[24 * 1024];
  int size = PAL_MKFGetChunkSize(iMapNum, fpMAP);
  LPPALMAP map = (LPPALMAP)malloc(sizeof(PALMAP));
  PAL_MKFReadChunk(buf, size, iMapNum, fpMAP);
  Decompress(buf, (LPBYTE)(map->Tiles), sizeof(map->Tiles));
  size = PAL_MKFGetChunkSize(iMapNum, fpGOP);
  map->pTileSprite = (LPSPRITE)malloc(size);
  PAL_MKFReadChunk(map->pTileSprite, size, iMapNum, fpGOP);
  map->iMapNum = iMapNum;
  return map;
}

VOID PAL_FreeMap(LPPALMAP lpMap) {
  if (lpMap == NULL)
    return;
  if (lpMap->pTileSprite != NULL)
    free(lpMap->pTileSprite);
  free(lpMap);
}

LPCBITMAPRLE
PAL_MapGetTileBitmap(BYTE x, BYTE y, BYTE h, BYTE ucLayer, LPCPALMAP lpMap) {
  if (x >= 64 || y >= 128 || h > 1 || lpMap == NULL) {
    return NULL;
  }
  DWORD d = lpMap->Tiles[y][x][h];
  if (ucLayer == 0) {
    // Bottom layer
    return PAL_SpriteGetFrame(lpMap->pTileSprite, (d & 0xFF) | ((d >> 4) & 0x100));
  } else {
    // Top layer
    d >>= 16;
    return PAL_SpriteGetFrame(lpMap->pTileSprite, ((d & 0xFF) | ((d >> 4) & 0x100)) - 1);
  }
}

BOOL PAL_MapTileIsBlocked(BYTE x, BYTE y, BYTE h, LPCPALMAP lpMap) {
  if (x >= 64 || y >= 128 || h > 1 || lpMap == NULL) {
    return TRUE;
  }
  return (lpMap->Tiles[y][x][h] & 0x2000) >> 13;
}

BYTE PAL_MapGetTileHeight(BYTE x, BYTE y, BYTE h, BYTE ucLayer, LPCPALMAP lpMap) {
  if (y >= 128 || x >= 64 || h > 1 || lpMap == NULL) {
    return 0;
  }
  DWORD d = lpMap->Tiles[y][x][h];
  if (ucLayer) {
    d >>= 16;
  }
  d >>= 8;
  return (BYTE)(d & 0xf);
}

VOID PAL_MapBlitToSurface(LPCPALMAP lpMap, SDL_Surface *lpSurface, const SDL_Rect *lpSrcRect, BYTE ucLayer) {
  // Convert the coordinate
  int sy = lpSrcRect->y / 16;
  int dy = (lpSrcRect->y + lpSrcRect->h) / 16 + 1;
  int sx = lpSrcRect->x / 32;
  int dx = (lpSrcRect->x + lpSrcRect->w) / 32 + 1;

  // Do the drawing.
  for (int y = sy; y < dy; y++) {
    for (int h = 0; h < 2; h++) {
      for (int x = sx; x < dx; x++) {
        LPCBITMAPRLE lpBitmap = PAL_MapGetTileBitmap((BYTE)x, (BYTE)y, (BYTE)h, ucLayer, lpMap);
        if (lpBitmap == NULL) {
          if (ucLayer)
            continue;
          lpBitmap = PAL_MapGetTileBitmap(0, 0, 0, ucLayer, lpMap);
        }
        int xPos = x * 32 + h * 16 - 16 - lpSrcRect->x;
        int yPos = y * 16 + h * 8 - 8 - lpSrcRect->y;
        PAL_RLEBlitToSurface(lpBitmap, lpSurface, PAL_XY(xPos, yPos));
      }
    }
  }
}
