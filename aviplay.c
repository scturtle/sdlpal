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

//
// aviplay.c
//
// Simple quick and dirty AVI player specially designed for PAL Win95.
//

/*
 * Portions based on:
 *
 * Microsoft Video-1 Decoder
 * Copyright (C) 2003 The FFmpeg project
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * Microsoft Video-1 Decoder by Mike Melanson (melanson@pcisys.net)
 * For more information about the MS Video-1 format, visit:
 *   http://www.pcisys.net/~melanson/codecs/
 */

#include "aviplay.h"
#include "audio.h"
#include "input.h"
#include "palcfg.h"
#include "riff.h"
#include "video.h"
#include <pthread.h>

#define MAX_AVI_LEVELS 3
#define FLG_MAIN_HDR 0x01
#define FLG_VID_FMT 0x02
#define FLG_AUD_FMT 0x04
#define FLG_ALL_HDRS 0x07

typedef struct AVIPlayState {
  pthread_mutex_t mutex;
  SDL_Surface *surface;
  long videoEndPos;
  uint32_t usPerFrame;
  uint32_t bufSize;
  SDL_AudioStream *stream;
  uint8_t *chunkBuf;
} AVIPlayState;

static AVIPlayState g_avi;

static BOOL PAL_ReadAVIInfo(FILE *fp) {
  RIFFHeader hdr;
  AVIMainHeader avih;
  AVIStreamHeader strh = {0};
  BitmapInfoHeader bih;
  WAVEFormatEx wfe;
  uint32_t blkType[MAX_AVI_LEVELS], curLvl = 0, flags = 0;
  long nextPos[MAX_AVI_LEVELS], pos = 0;

  fseek(fp, 0, SEEK_SET);
  if (fread(&hdr, sizeof(hdr), 1, fp) != 1)
    return FALSE;

  if (hdr.signature != RIFF_RIFF || hdr.type != RIFF_AVI)
    return FALSE;
  nextPos[curLvl] = (pos += sizeof(RIFFHeader)) + hdr.length;
  blkType[curLvl++] = hdr.type;

  while (!feof(fp) && curLvl > 0) {
    RIFFBlockHeader blk;
    fseek(fp, pos, SEEK_SET);
    if (fread(&blk.type, sizeof(RIFFChunkHeader), 1, fp) != 1)
      return FALSE;
    pos += sizeof(RIFFChunkHeader);

    if (blk.type == AVI_LIST) {
      if (fread(&blk.list.type, sizeof(uint32_t), 1, fp) != 1)
        return FALSE;
    }

    switch (blkType[curLvl - 1]) {
    case RIFF_AVI:
      if (curLvl != 1)
        return FALSE;
      if (blk.type == AVI_LIST) {
        nextPos[curLvl] = pos + blk.length;
        blkType[curLvl++] = blk.list.type;
        pos += 4; // Skip LIST type
        continue;
      }
      break;

    case AVI_hdrl:
      if (curLvl != 2)
        return FALSE;
      if (blk.type == AVI_avih) {
        if ((flags & FLG_MAIN_HDR) || fread(&avih, sizeof(avih), 1, fp) != 1)
          return FALSE;
        flags |= FLG_MAIN_HDR;
        if (!avih.dwWidth || !avih.dwHeight || (avih.dwFlags & AVIF_MUSTUSEINDEX))
          return FALSE;
      } else if (blk.type == AVI_LIST && blk.list.type == AVI_strl) {
        nextPos[curLvl] = pos + blk.length;
        blkType[curLvl++] = blk.list.type;
        pos += 4;
        continue;
      }
      break;

    case AVI_movi:
      if (curLvl != 2 || (flags & FLG_ALL_HDRS) != FLG_ALL_HDRS)
        return FALSE;

      fseek(fp, pos - sizeof(RIFFChunkHeader), SEEK_SET);

      g_avi.videoEndPos = nextPos[curLvl - 1];
      g_avi.usPerFrame = avih.dwMicroSecPerFrame;
      g_avi.surface = SDL_CreateSurface(bih.biWidth, bih.biHeight, SDL_PIXELFORMAT_XRGB1555);
      g_avi.bufSize = avih.dwSuggestedBufferSize + sizeof(RIFFChunkHeader);
      g_avi.chunkBuf = g_avi.bufSize > 0 ? UTIL_malloc(g_avi.bufSize) : NULL;

      SDL_AudioSpec srcSpec;
      srcSpec.format = (wfe.format.wBitsPerSample == 8) ? SDL_AUDIO_U8 : SDL_AUDIO_S16LE;
      srcSpec.channels = wfe.format.nChannels;
      srcSpec.freq = (float)wfe.format.nSamplesPerSec;
      SDL_AudioSpec *dstSpec = AUDIO_GetDeviceSpec();
      pthread_mutex_lock(&g_avi.mutex);
      g_avi.stream = SDL_CreateAudioStream(&srcSpec, dstSpec);
      SDL_BindAudioStream(AUDIO_GetDeviceID(), g_avi.stream);

      pthread_mutex_unlock(&g_avi.mutex);

      return TRUE;

    case AVI_strl:
      if (curLvl != 3)
        return FALSE;
      if (blk.type == AVI_strh) {
        if (strh.fccType != 0 || fread(&strh, sizeof(strh), 1, fp) != 1)
          return FALSE;
      } else if (blk.type == AVI_strf) {
        if (strh.fccType == AVI_vids) {
          if (fread(&bih, sizeof(bih), 1, fp) != 1)
            return FALSE;
          if ((flags & FLG_VID_FMT) ||
              (bih.biCompression != CODEC_MSVC && bih.biCompression != CODEC_msvc && bih.biCompression != CODEC_CRAM &&
               bih.biCompression != CODEC_cram) ||
              bih.biBitCount != 16)
            return FALSE;
          flags |= FLG_VID_FMT;
        } else if (strh.fccType == AVI_auds) {
          if (fread(&wfe, sizeof(WAVEFormatPCM) + 2, 1, fp) != 1)
            return FALSE;
          if ((flags & FLG_AUD_FMT) || wfe.format.wFormatTag != CODEC_PCM_U8)
            return FALSE;
          flags |= FLG_AUD_FMT;
        }
        strh.fccType = 0;
      }
      break;
    }
    pos += blk.length;
    while (curLvl > 0 && pos == nextPos[curLvl - 1])
      curLvl--;
    if (curLvl > 0 && pos > nextPos[curLvl - 1])
      return FALSE;
  }
  return FALSE;
}

static RIFFChunk *PAL_ReadDataChunk(FILE *fp, long endPos, void *ubuf, uint32_t ulen) {
  RIFFBlockHeader hdr;
  RIFFChunk *chunk = NULL;
  long pos = feof(fp) ? endPos : ftell(fp);

  while (!chunk && pos < endPos) {
    if (fread(&hdr, sizeof(RIFFChunkHeader), 1, fp) != 1)
      return NULL;
    pos += sizeof(RIFFChunkHeader);

    switch (hdr.type) {
    case AVI_01wb:
    case AVI_00db:
    case AVI_00dc:
      chunk = (ubuf && ulen >= sizeof(RIFFChunkHeader) + hdr.length)
                  ? (RIFFChunk *)ubuf
                  : UTIL_malloc(sizeof(RIFFChunkHeader) + hdr.length);
      if (fread(chunk->data, hdr.length, 1, fp) != 1) {
        if (chunk != ubuf)
          free(chunk);
        return NULL;
      }
      chunk->header = hdr.chunk;
      break;
    case AVI_LIST:
      if (fread(&hdr.list.type, 4, 1, fp) != 1)
        return NULL;
      if (hdr.list.type == AVI_rec)
        break;
    default:
      fseek(fp, pos += hdr.length, SEEK_SET);
    }
  }
  return chunk;
}

static void PAL_RenderAVIFrame(SDL_Surface *s, const RIFFChunk *c) {
#define AV_RL16(x) ((((const uint8_t *)(x))[1] << 8) | ((const uint8_t *)(x))[0])
  uint16_t *pixels = (uint16_t *)s->pixels;
  uint32_t ptr = 0, skip = 0, stride = s->pitch >> 1;
  int row_dec = stride + 4, w = s->w >> 2, h = s->h >> 2, total = w * h;

  for (int by = h; by > 0; by--) {
    int bptr = ((by * 4) - 1) * stride;
    for (int bx = w; bx > 0; bx--) {
      if (skip) {
        bptr += 4;
        skip--;
        total--;
        continue;
      }
      if (ptr + 2 > c->header.length)
        return;

      int pptr = bptr;
      uint8_t a = c->data[ptr++], b = c->data[ptr++];

      if (!a && !b && !total)
        return;
      else if ((b & 0xFC) == 0x84)
        skip = ((b - 0x84) << 8) + a - 1;
      else if (b < 0x80) {
        uint16_t flags = (b << 8) | a, clr[8];
        if (ptr + 4 > c->header.length)
          return;
        clr[0] = AV_RL16(&c->data[ptr]);
        ptr += 2;
        clr[1] = AV_RL16(&c->data[ptr]);
        ptr += 2;

        if (clr[0] & 0x8000) {
          if (ptr + 12 > c->header.length)
            return;
          for (int i = 2; i < 8; i++, ptr += 2)
            clr[i] = AV_RL16(&c->data[ptr]);
          for (int py = 0; py < 4; py++, pptr -= row_dec)
            for (int px = 0; px < 4; px++, flags >>= 1)
              pixels[pptr++] = clr[((py & 2) << 1) + (px & 2) + ((flags & 1) ^ 1)];
        } else {
          for (int py = 0; py < 4; py++, pptr -= row_dec)
            for (int px = 0; px < 4; px++, flags >>= 1)
              pixels[pptr++] = clr[(flags & 1) ^ 1];
        }
      } else {
        uint16_t cval = (b << 8) | a;
        for (int py = 0; py < 4; py++, pptr -= row_dec)
          for (int px = 0; px < 4; px++)
            pixels[pptr++] = cval;
      }
      bptr += 4;
      total--;
    }
  }
}

BOOL PAL_PlayAVI(LPCSTR path) {
  if (!gConfig.fEnableAviPlay)
    return FALSE;
  FILE *fp = UTIL_OpenFile(path);
  if (!fp) {
    printf("Cannot open AVI file: %s!\n", path);
    return FALSE;
  }

  if (!PAL_ReadAVIInfo(fp)) {
    printf("Failed to parse AVI file or its format not supported!\n");
    fclose(fp);
    return FALSE;
  }

  PAL_ClearKeyState();
  BOOL end = FALSE;
  uint64_t usDelta = 0, now = PAL_GetTicks(), start = now, next;

  while (!end) {
    RIFFChunk *c = PAL_ReadDataChunk(fp, g_avi.videoEndPos, g_avi.chunkBuf, g_avi.bufSize);
    if (!c)
      break;

    if (c->header.type == AVI_00dc || c->header.type == AVI_00db) {
      next = start + (g_avi.usPerFrame / 1000);
      usDelta += g_avi.usPerFrame % 1000;
      next += usDelta / 1000;
      usDelta %= 1000;

      PAL_RenderAVIFrame(g_avi.surface, c);
      VIDEO_DrawSurfaceToScreen(g_avi.surface);

      now = PAL_GetTicks();
      UTIL_Delay(now >= next ? 1 : next - now);
      start = PAL_GetTicks();

      if (g_InputState.dwKeyPress & (kKeyMenu | kKeySearch))
        end = TRUE;
    } else if (c->header.type == AVI_01wb) {
      SDL_PutAudioStreamData(g_avi.stream, c->data, c->header.length);
    }
    if (c != (RIFFChunk *)g_avi.chunkBuf)
      free(c);
  }

  if (g_avi.surface) {
    SDL_DestroySurface(g_avi.surface);
    g_avi.surface = NULL;
  }
  if (g_avi.chunkBuf) {
    free(g_avi.chunkBuf);
    g_avi.chunkBuf = NULL;
  }
  if (g_avi.stream) {
    SDL_DestroyAudioStream(g_avi.stream);
    g_avi.stream = NULL;
  }
  fclose(fp);
  return TRUE;
}
