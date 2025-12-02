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

#include "audio.h"
#include "global.h"
#include "palcfg.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_audio.h>

typedef struct tagWAVEDATA {
  struct tagWAVEDATA *next;
  SDL_AudioStream *stream;
} WAVEDATA;

typedef struct tagSOUNDPLAYER {
  FILE *mkf;
  WAVEDATA soundlist; /* 播放链表头 */
} SOUNDPLAYER, *LPSOUNDPLAYER;

BOOL SOUND_Play(VOID *object, INT iSoundNum, BOOL fLoop, FLOAT flFadeTime) {
  LPSOUNDPLAYER player = (LPSOUNDPLAYER)object;
  if (player == NULL)
    return FALSE;

  // 读取原始 WAV
  int file_len = PAL_MKFGetChunkSize(iSoundNum, player->mkf);
  if (file_len <= 0)
    return FALSE;
  void *file_data = malloc(file_len);
  if (!file_data)
    return FALSE;
  PAL_MKFReadChunk(file_data, file_len, iSoundNum, player->mkf);

  // 解析 WAV
  SDL_AudioSpec src_spec;
  Uint8 *wav_buffer = NULL;
  Uint32 wav_len = 0;
  if (!SDL_LoadWAV_IO(SDL_IOFromConstMem(file_data, file_len), true, &src_spec, &wav_buffer, &wav_len)) {
    free(file_data);
    return FALSE;
  }

  // 创建 stream
  const SDL_AudioSpec *dev_spec = AUDIO_GetDeviceSpec();
  SDL_AudioStream *stream = SDL_CreateAudioStream(&src_spec, dev_spec);
  if (!stream) {
    SDL_free(wav_buffer);
    free(file_data);
    return FALSE;
  }

  // 将音频数据推入流中
  SDL_PutAudioStreamData(stream, wav_buffer, wav_len);
  SDL_FlushAudioStream(stream);

  SDL_free(wav_buffer);
  free(file_data);

  SDL_BindAudioStream(AUDIO_GetDeviceID(), stream);

  // 添加到播放列表
  WAVEDATA *node = (WAVEDATA *)malloc(sizeof(WAVEDATA));
  memset(node, 0, sizeof(WAVEDATA));
  node->stream = stream;
  node->next = NULL;

  WAVEDATA *prev = &player->soundlist;
  WAVEDATA *curr = prev->next;
  while (curr) {
    // 清理
    if (SDL_GetAudioStreamAvailable(curr->stream) == 0 && SDL_GetAudioStreamQueued(curr->stream) == 0) {
      WAVEDATA *to_delete = curr;
      prev->next = curr->next;
      curr = curr->next;
      SDL_DestroyAudioStream(to_delete->stream);
      free(to_delete);
    } else {
      prev = curr;
      curr = curr->next;
    }
  }
  prev->next = node;

  return TRUE;
}

VOID *SOUND_Init(VOID) {
  LPSOUNDPLAYER player = (LPSOUNDPLAYER)malloc(sizeof(SOUNDPLAYER));
  if (!player)
    return NULL;
  player->mkf = UTIL_OpenRequiredFile("sounds.mkf");
  player->soundlist.next = NULL;
  player->soundlist.stream = NULL;
  return player;
}

VOID SOUND_Shutdown(VOID *object) {
  LPSOUNDPLAYER player = (LPSOUNDPLAYER)object;
  if (player) {
    WAVEDATA *curr = player->soundlist.next;
    while (curr) {
      WAVEDATA *next = curr->next;
      if (curr->stream)
        SDL_DestroyAudioStream(curr->stream);
      free(curr);
      curr = next;
    }
    player->soundlist.next = NULL;

    if (player->mkf)
      fclose(player->mkf);
    free(player);
  }
}
