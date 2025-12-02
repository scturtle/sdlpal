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

#include "global.h"
#include "input.h"
#include "palcfg.h"
#include <errno.h>

/* ---------- rle ---------- */

inline BYTE PAL_CalcShadowColor(BYTE bSourceColor) { return ((bSourceColor & 0xF0) | ((bSourceColor & 0x0F) >> 1)); }

static INT PAL_RLEBlitToSurfaceGeneric(LPCBITMAPRLE lpBitmapRLE, SDL_Surface *lpDstSurface, PAL_POS pos,
                                       BOOL bShadow,    // 阴影
                                       INT iColorShift, // 色偏
                                       BOOL bForceMono, // 强制单色
                                       BYTE bMonoColor  // 单色
) {
  if (!lpBitmapRLE || !lpDstSurface)
    return -1;

  const uint8_t *src = (const uint8_t *)lpBitmapRLE;

  // 1. Skip Magic Number (0x02000000)
  if (src[0] == 0x02 && src[1] == 0x00 && src[2] == 0x00 && src[3] == 0x00) {
    src += 4;
  }

  // 2. Read Dimensions
  int width = src[0] | (src[1] << 8);
  int height = src[2] | (src[3] << 8);
  src += 4;

  // Destination offsets
  int dx = PAL_X(pos);
  int dy = PAL_Y(pos);

  // Check whether bitmap intersects the surface.
  if (width + dx <= 0 || dx >= lpDstSurface->w || height + dy <= 0 || dy >= lpDstSurface->h) {
    return 0;
  }

  int dstPitch = lpDstSurface->pitch;
  uint8_t *dstPixels = (uint8_t *)lpDstSurface->pixels;

  // Current position within the bitmap logic
  int cur_x = 0;
  int cur_y = 0;

  // Total pixels to process
  int total = width * height;
  int processed = 0;

  while (processed < total) {
    uint8_t code = *src++;
    int count;
    bool skip = false;

    // Decode RLE Packet
    if ((code & 0x80) && (code <= 0x80 + width)) {
      count = code - 0x80;
      skip = true;
    } else {
      count = code;
      skip = false;
    }

    // Process the run (it might wrap across multiple lines)
    while (count > 0) {
      // How many pixels can we write/skip on the current line?
      int runLen = (cur_x + count > width) ? (width - cur_x) : count;

      // Only draw if NOT skipping and current line is vertically within bounds
      int screen_y = dy + cur_y;
      if (!skip && screen_y >= 0 && screen_y < lpDstSurface->h) {
        int screenX = dx + cur_x;

        // Horizontal Clipping
        int drawX = screenX;
        int drawLen = runLen;
        int srcOffset = 0; // Offset into the current RLE packet data

        // Clip Left
        if (drawX < 0) {
          srcOffset = -drawX;
          drawLen -= srcOffset;
          drawX = 0;
        }

        // Clip Right
        if (drawX + drawLen > lpDstSurface->w) {
          drawLen = lpDstSurface->w - drawX;
        }

        // Draw if we have valid pixels
        if (drawLen > 0) {
          uint8_t *pDst = dstPixels + screen_y * dstPitch + drawX;

          if (bShadow) {
            // 阴影
            for (int k = 0; k < drawLen; k++) {
              pDst[k] = PAL_CalcShadowColor(pDst[k]);
            }

          } else if (iColorShift || bForceMono) {
            // 色偏 或 强制单色
            const uint8_t *pSrc = src + srcOffset;
            for (int k = 0; k < drawLen; k++) {
              int val = pSrc[k];
              int lower = val & 0x0F;
              lower += iColorShift;
              if (lower > 0x0F)
                lower = 0x0F;
              else if (lower < 0)
                lower = 0;
              if (bForceMono) {
                // mono 模式：使用传入颜色的高位
                pDst[k] = (uint8_t)((bMonoColor & 0xF0) | lower);
              } else {
                // 保持源像素的高位
                pDst[k] = (uint8_t)((val & 0xF0) | lower);
              }
            }
          } else {
            memcpy(pDst, src + srcOffset, drawLen);
          }
        }
      }

      // Update State
      if (!skip)
        src += runLen; // Advance data pointer only for data runs

      cur_x += runLen;
      count -= runLen;
      processed += runLen;

      // Handle Line Wrapping
      if (cur_x >= width) {
        cur_x = 0;
        cur_y++;
      }
    }
  }

  return 0;
}

// APIs to blit an RLE-compressed bitmap to an 8-bit SDL surface.

INT PAL_RLEBlitToSurface(LPCBITMAPRLE lpBitmapRLE, SDL_Surface *lpDstSurface, PAL_POS pos) {
  return PAL_RLEBlitToSurfaceGeneric(lpBitmapRLE, lpDstSurface, pos, /*bShadow=*/FALSE, /*iColorShift=*/0,
                                     /*bForceMono=*/FALSE, /*bMonoColor=*/0);
}

INT PAL_RLEBlitToSurfaceWithShadow(LPCBITMAPRLE lpBitmapRLE, SDL_Surface *lpDstSurface, PAL_POS pos, BOOL bShadow) {
  return PAL_RLEBlitToSurfaceGeneric(lpBitmapRLE, lpDstSurface, pos, bShadow, /*iColorShift=*/0, /*bForceMono=*/FALSE,
                                     /*bMonoColor=*/0);
}

INT PAL_RLEBlitWithColorShift(LPCBITMAPRLE lpBitmapRLE, SDL_Surface *lpDstSurface, PAL_POS pos, INT iColorShift) {
  return PAL_RLEBlitToSurfaceGeneric(lpBitmapRLE, lpDstSurface, pos, /*bShadow=*/FALSE, iColorShift,
                                     /*bForceMono=*/FALSE, /*bMonoColor=*/0);
}

INT PAL_RLEBlitMonoColor(LPCBITMAPRLE lpBitmapRLE, SDL_Surface *lpDstSurface, PAL_POS pos, BYTE bColor,
                         INT iColorShift) {
  return PAL_RLEBlitToSurfaceGeneric(lpBitmapRLE, lpDstSurface, pos, /*bShadow=*/FALSE, iColorShift,
                                     /*bForceMono=*/TRUE, /*bMonoColor=*/bColor);
}

INT PAL_RLEGetWidth(LPCBITMAPRLE lpBitmapRLE) {
  if (lpBitmapRLE == NULL)
    return 0;

  // Skip the 0x00000002 in the file header.
  if (lpBitmapRLE[0] == 0x02 && lpBitmapRLE[1] == 0x00 && lpBitmapRLE[2] == 0x00 && lpBitmapRLE[3] == 0x00)
    lpBitmapRLE += 4;

  // Return the width of the bitmap.
  return lpBitmapRLE[0] | (lpBitmapRLE[1] << 8);
}

INT PAL_RLEGetHeight(LPCBITMAPRLE lpBitmapRLE) {
  if (lpBitmapRLE == NULL)
    return 0;

  // Skip the 0x00000002 in the file header.
  if (lpBitmapRLE[0] == 0x02 && lpBitmapRLE[1] == 0x00 && lpBitmapRLE[2] == 0x00 && lpBitmapRLE[3] == 0x00)
    lpBitmapRLE += 4;

  // Return the height of the bitmap.
  return lpBitmapRLE[2] | (lpBitmapRLE[3] << 8);
}

WORD PAL_SpriteGetNumFrames(LPCSPRITE lpSprite) {
  if (lpSprite == NULL)
    return 0;
  return (lpSprite[0] | (lpSprite[1] << 8)) - 1;
}

LPCBITMAPRLE
PAL_SpriteGetFrame(LPCSPRITE lpSprite, INT iFrameNum) {
  if (lpSprite == NULL)
    return NULL;

  // Hack for broken sprites like the Bloody-Mouth Bug
  // imagecount = (lpSprite[0] | (lpSprite[1] << 8)) - 1;
  int imagecount = (lpSprite[0] | (lpSprite[1] << 8));

  if (iFrameNum < 0 || iFrameNum >= imagecount)
    return NULL;

  // Get the offset of the frame
  iFrameNum <<= 1;
  int offset = ((lpSprite[iFrameNum] | (lpSprite[iFrameNum + 1] << 8)) << 1);
  if (offset == 0x18444)
    offset = (WORD)offset;
  return &lpSprite[offset];
}

/* ---------- mkf ---------- */

INT PAL_MKFGetChunkCount(FILE *fp) {
  if (fp == NULL)
    return 0;

  int32_t iNumChunk;
  fseek(fp, 0, SEEK_SET);
  if (fread(&iNumChunk, sizeof(int32_t), 1, fp) == 1)
    return (iNumChunk - 4) >> 2;
  else
    return 0;
}

INT PAL_MKFGetChunkSize(UINT uiChunkNum, FILE *fp) {
  // Get the total number of chunks.
  UINT uiChunkCount = PAL_MKFGetChunkCount(fp);
  if (uiChunkNum >= uiChunkCount)
    return -1;

  // Get the offset of the specified chunk and the next chunk.
  UINT uiOffset, uiNextOffset;
  fseek(fp, 4 * uiChunkNum, SEEK_SET);
  PAL_fread(&uiOffset, sizeof(UINT), 1, fp);
  PAL_fread(&uiNextOffset, sizeof(UINT), 1, fp);

  // Return the length of the chunk.
  return uiNextOffset - uiOffset;
}

INT PAL_MKFReadChunk(LPBYTE lpBuffer, UINT uiBufferSize, UINT uiChunkNum, FILE *fp) {
  if (lpBuffer == NULL || fp == NULL || uiBufferSize == 0)
    return -1;

  // Get the total number of chunks.
  UINT uiChunkCount = PAL_MKFGetChunkCount(fp);
  if (uiChunkNum >= uiChunkCount)
    return -1;

  // Get the offset of the chunk.
  UINT uiOffset, uiNextOffset;
  fseek(fp, 4 * uiChunkNum, SEEK_SET);
  PAL_fread(&uiOffset, 4, 1, fp);
  PAL_fread(&uiNextOffset, 4, 1, fp);

  // Get the length of the chunk.
  UINT uiChunkLen = uiNextOffset - uiOffset;

  if (uiChunkLen > uiBufferSize)
    return -2;

  if (uiChunkLen != 0) {
    fseek(fp, uiOffset, SEEK_SET);
    return (int)fread(lpBuffer, 1, uiChunkLen, fp);
  }

  return -1;
}

INT PAL_MKFGetDecompressedSize(UINT uiChunkNum, FILE *fp) {
  if (fp == NULL)
    return -1;

  // Get the total number of chunks.
  UINT uiChunkCount;
  uiChunkCount = PAL_MKFGetChunkCount(fp);
  if (uiChunkNum >= uiChunkCount)
    return -1;

  // Get the offset of the chunk.
  UINT uiOffset;
  fseek(fp, 4 * uiChunkNum, SEEK_SET);
  PAL_fread(&uiOffset, 4, 1, fp);

  // Read the header.
  UINT size;
  fseek(fp, uiOffset, SEEK_SET);
  PAL_fread(&size, sizeof(UINT), 1, fp);
  return size;
}

INT PAL_MKFDecompressChunk(LPBYTE lpBuffer, UINT uiBufferSize, UINT uiChunkNum, FILE *fp) {
  int len = PAL_MKFGetChunkSize(uiChunkNum, fp);
  if (len <= 0)
    return len;

  LPBYTE buf = (LPBYTE)malloc(len);
  if (buf == NULL)
    return -3;

  if (PAL_MKFReadChunk(buf, len, uiChunkNum, fp) < 0)
    return -1;

  len = Decompress(buf, lpBuffer, uiBufferSize);
  free(buf);

  return len;
}

/* ---------- util functions ---------- */

void PAL_Delay(uint32_t ms) { SDL_Delay(ms); }
uint64_t PAL_GetTicks() { return SDL_GetTicks(); }

void PAL_DelayUntil(uint32_t ms) {
  PAL_ProcessEvent();
  while (PAL_GetTicks() < ms) {
    PAL_ProcessEvent();
    PAL_Delay(1);
  }
}

char *UTIL_va(char *buffer, int buflen, const char *format, ...) {
  if (buflen > 0 && buffer) {
    va_list argptr;
    va_start(argptr, format);
    vsnprintf(buffer, buflen, format, argptr);
    va_end(argptr);
    return buffer;
  } else {
    return NULL;
  }
}

static int g_isSeeded = 0;

static void lsrand() {
  srand48(time(NULL));
  g_isSeeded = 1;
}

int RandomLong(int from, int to) {
  if (to <= from)
    return from;
  if (!g_isSeeded)
    lsrand();
  return from + (lrand48() % (to - from + 1));
}

float RandomFloat(float from, float to) {
  if (to <= from)
    return from;
  if (!g_isSeeded)
    lsrand();
  return from + (float)(drand48() * (double)(to - from));
}

void UTIL_Delay(unsigned int ms) {
  uint64_t t = SDL_GetTicks() + ms;
  PAL_ProcessEvent();
  while (SDL_GetTicks() < t) {
    PAL_Delay(1);
    PAL_ProcessEvent();
  }
}

void TerminateOnError(const char *fmt, ...) {
  va_list argptr;
  char string[256];
  extern VOID PAL_Shutdown(int);
  va_start(argptr, fmt);
  vsnprintf(string, sizeof(string), fmt, argptr);
  va_end(argptr);
  fprintf(stderr, "\nFATAL ERROR: %s\n", string);
  PAL_Shutdown(255);
}

void *UTIL_malloc(size_t buffer_size) {
  void *buffer;
  buffer = malloc(buffer_size);
  if (buffer == NULL)
    TerminateOnError("malloc() failure\n");
  return buffer;
}

/* ---------- file ---------- */

FILE *UTIL_OpenRequiredFile(LPCSTR lpszFileName) {
  FILE *fp = UTIL_OpenFile(lpszFileName);
  if (!fp)
    fp = fopen(lpszFileName, "rb");
  if (!fp)
    TerminateOnError("File open error(%d): %s!\n", errno, lpszFileName);
  return fp;
}

FILE *UTIL_OpenFile(LPCSTR lpszFileName) { return UTIL_OpenFileForMode(lpszFileName, "rb"); }

FILE *UTIL_OpenFileForMode(LPCSTR lpszFileName, LPCSTR szMode) {
  return UTIL_OpenFileAtPathForMode(gConfig.pszGamePath, lpszFileName, szMode);
}

FILE *UTIL_OpenFileAtPathForMode(LPCSTR lpszPath, LPCSTR lpszFileName, LPCSTR szMode) {
  if (!lpszPath || !lpszFileName || !szMode)
    return NULL;

  char path[PATH_MAX];

  // Try to find an existing file for reading/appending
  if (UTIL_GetFullPathName(path, sizeof(path), lpszPath, lpszFileName)) {
    return fopen(path, szMode);
  }

  // If file doesn't exist and mode implies writing/creating (not 'r')
  if (szMode[0] != 'r') {
    snprintf(path, sizeof(path), "%s/%s", lpszPath, lpszFileName);
    return fopen(path, szMode);
  }

  return NULL;
}

const char *UTIL_GetFullPathName(char *buffer, size_t buflen, const char *basepath, const char *subpath) {
  if (!buffer || !basepath || !subpath || buflen == 0)
    return NULL;

  // 1. Try exact match first
  snprintf(buffer, buflen, "%s/%s", basepath, subpath);
  if (access(buffer, F_OK) == 0)
    return buffer;

  // 2. Fallback to case-insensitive directory search
  DIR *dir = opendir(basepath);
  if (!dir)
    return NULL;

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
    if (strcasecmp(entry->d_name, subpath) == 0) {
      snprintf(buffer, buflen, "%s/%s", basepath, entry->d_name);
      closedir(dir);
      return buffer; // If readdir found it, it exists. No need for a second access() call.
    }
  }

  closedir(dir);
  buffer[0] = '\0';
  return NULL;
}
