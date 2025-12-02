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
// Portions based on PalLibrary by Lou Yihua <louyihua@21cn.com>.
// Copyright (c) 2006-2007, Lou Yihua.
//

#include "main.h"

// Read a frame from a RNG animation.
static INT PAL_RNGReadFrame(LPBYTE lpBuffer, UINT uiBufferSize, UINT uiRngNum, UINT uiFrameNum, FILE *fpRngMKF) {
  if (lpBuffer == NULL || fpRngMKF == NULL || uiBufferSize == 0)
    return -1;

  // Get the total number of chunks.
  UINT uiChunkCount = PAL_MKFGetChunkCount(fpRngMKF);
  if (uiRngNum >= uiChunkCount)
    return -1;

  // Get the offset of the chunk.
  fseek(fpRngMKF, 4 * uiRngNum, SEEK_SET);
  UINT uiOffset = 0, uiNextOffset = 0;
  PAL_fread(&uiOffset, sizeof(UINT), 1, fpRngMKF);
  PAL_fread(&uiNextOffset, sizeof(UINT), 1, fpRngMKF);

  // Get the length of the chunk.
  INT nChunk = uiNextOffset - uiOffset;
  if (nChunk == 0)
    return -1;

  // Get the number of frames.
  fseek(fpRngMKF, uiOffset, SEEK_SET);
  UINT nFrames = 0;
  PAL_fread(&nFrames, sizeof(UINT), 1, fpRngMKF);
  nFrames = (nFrames - 4) / 4;
  if (uiFrameNum >= nFrames)
    return -1;

  // Get the offset of the frame.
  fseek(fpRngMKF, uiOffset + 4 * uiFrameNum, SEEK_SET);
  UINT frameOffset = 0, nextFrameOffset = 0;
  PAL_fread(&frameOffset, sizeof(UINT), 1, fpRngMKF);
  PAL_fread(&nextFrameOffset, sizeof(UINT), 1, fpRngMKF);

  // Get the length of the frame.
  int frameLen = nextFrameOffset - frameOffset;
  if ((UINT)frameLen > uiBufferSize)
    return -2;
  if (frameLen == 0)
    return -1;

  fseek(fpRngMKF, uiOffset + frameOffset, SEEK_SET);
  return (int)fread(lpBuffer, 1, frameLen, fpRngMKF);
}

// Blit one frame in an RNG animation to an SDL surface.
// The surface should contain the last frame of the RNG, or blank if it's the first frame.
// NOTE: Assume the surface is already locked, and the surface is a 320x200 8-bit one.
static INT PAL_RNGBlitToSurface(const uint8_t *rng, int length, SDL_Surface *lpDstSurface) {

  // Check for invalid parameters.
  if (lpDstSurface == NULL || length < 0)
    return -1;

#define COPY_PIXEL                                                                                                     \
  ((LPBYTE)(lpDstSurface->pixels))[(dst_ptr / 320) * lpDstSurface->pitch + (dst_ptr % 320)] = rng[ptr++], dst_ptr++;

  // Draw the frame to the surface.
  int ptr = 0;
  int dst_ptr = 0;
  while (ptr < length) {
    uint8_t data = rng[ptr++];
    uint16_t wdata;
    switch (data) {
    case 0x00:
    case 0x13:
      // End
      return 0;

    // 0x02-0x04: 跳过若干像素不绘制
    case 0x02:
      dst_ptr += 2;
      break;

    case 0x03:
      data = rng[ptr++];
      dst_ptr += (data + 1) * 2;
      break;

    case 0x04:
      wdata = rng[ptr++];
      wdata |= rng[ptr++] << 8;
      dst_ptr += ((unsigned int)wdata + 1) * 2;
      break;

    // 0x06-0x0a: 绘制连续 2 到 10 个字节的像素数据
    case 0x06:
    case 0x07:
    case 0x08:
    case 0x09:
    case 0x0a:
      for (int i = 0; i < data - (0x06 - 1); i++) {
        COPY_PIXEL;
        COPY_PIXEL;
      }
      break;

    case 0x0b:
      data = rng[ptr++];
      for (int i = 0; i <= data; i++) {
        COPY_PIXEL;
        COPY_PIXEL;
      }
      break;

    case 0x0c:
      wdata = rng[ptr++];
      wdata |= rng[ptr++] << 8;
      for (int i = 0; i <= wdata; i++) {
        COPY_PIXEL;
        COPY_PIXEL;
      }
      break;

    // 重复模式
    case 0x0d:
    case 0x0e:
    case 0x0f:
    case 0x10:
      for (int i = 0; i < data - (0x0d - 2); i++) {
        COPY_PIXEL;
        COPY_PIXEL;
        ptr -= 2;
      }
      ptr += 2;
      break;

    case 0x11:
      data = rng[ptr++];
      for (int i = 0; i <= data; i++) {
        COPY_PIXEL;
        COPY_PIXEL;
        ptr -= 2;
      }
      ptr += 2;
      break;

    case 0x12:
      wdata = rng[ptr++];
      wdata |= rng[ptr++] << 8;
      for (int i = 0; i <= wdata; i++) {
        COPY_PIXEL;
        COPY_PIXEL;
        ptr -= 2;
      }
      ptr += 2;
      break;
    }
  }

  return 0;
}

// Play a RNG movie.
VOID PAL_RNGPlay(INT iNumRNG, INT iStartFrame, INT iEndFrame, INT iSpeed) {
  uint8_t rng[65000], buf[65000];
  FILE *fp = UTIL_OpenRequiredFile("rng.mkf");
  if (iEndFrame > 0)
    iEndFrame++;
  for (double iTime = PAL_GetTicks(); iStartFrame != iEndFrame; iStartFrame++) {
    // Read, decompress and render the frame
    if (PAL_RNGReadFrame(buf, sizeof(buf), iNumRNG, iStartFrame, fp) < 0)
      break;
    if (PAL_RNGBlitToSurface(rng, Decompress(buf, rng, sizeof(rng)), gpScreen) < 0)
      break;

    // Update the screen
    VIDEO_UpdateScreen(NULL);

    // Fade in the screen if needed
    if (gNeedToFadeIn) {
      PAL_FadeIn(gCurPalette, gNightPalette, 1);
      gNeedToFadeIn = FALSE;
    }

    // Delay for a while
    iTime += 1000.0 / (iSpeed == 0 ? 16 : iSpeed);
    PAL_DelayUntil(iTime);
  }

  fclose(fp);
}
