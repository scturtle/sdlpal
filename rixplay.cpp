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
#include <math.h>

#include "adplug/nemuopl.h"
#include "adplug/opl.h"
#include "adplug/rix.h"
#include "adplug/wemuopl.h"

#define SDL_MIX_MAXVOLUME 128

typedef struct tagRIXPLAYER {
  Copl *opl;
  CrixPlayer *rix;

  SDL_AudioStream *stream;

  INT iMusic;
  BOOL fLoop;
  INT iNextMusic;
  BOOL fNextLoop;

  Uint64 dwStartFadeTime;
  INT iTotalFadeOutSamples;
  INT iTotalFadeInSamples;
  INT iRemainingFadeSamples;

  enum { NONE, FADE_IN, FADE_OUT } FadeType;

} RIXPLAYER;

static RIXPLAYER gRixPlayer;

// SDL AudioStream 获取数据时的回调函数
static void SDLCALL RIX_StreamCallback(void *userdata, SDL_AudioStream *stream, int additional_amount,
                                       int total_amount) {
  RIXPLAYER *p = &gRixPlayer;
  if (!AUDIO_MusicEnabled())
    return;

  const int samples_per_tick = OPL_SAMPLE_RATE / 70;
  const size_t required_samples = (size_t)samples_per_tick * AUDIO_CHANNELS;
  const int bytes_per_tick = (int)(required_samples * sizeof(short));

  short opl_buffer[2048];
  if (required_samples > 2048)
    TerminateOnError("opl buffer too small\n");

  int bytes_put = 0;
  while (bytes_put < additional_amount) {
    // 检查并处理淡出完毕后的切歌逻辑
    if (p->FadeType == RIXPLAYER::FADE_OUT && (p->iMusic == -1 || p->iRemainingFadeSamples <= 0)) {
      if (p->iNextMusic > 0) {
        p->iMusic = p->iNextMusic;
        p->iNextMusic = -1;
        p->fLoop = p->fNextLoop;
        p->FadeType = RIXPLAYER::FADE_IN;
        p->iRemainingFadeSamples = p->iTotalFadeInSamples;
        p->rix->rewind(p->iMusic);
      } else {
        p->iMusic = -1;
        p->FadeType = RIXPLAYER::NONE;
        break;
      }
    }

    if (p->iMusic == -1)
      break;

    if (!p->rix->update()) {
      if (!p->fLoop) {
        p->iMusic = -1;
        if (p->FadeType != RIXPLAYER::FADE_OUT) {
          p->FadeType = RIXPLAYER::NONE;
        }
        break;
      }
      p->rix->rewind(p->iMusic);
      if (!p->rix->update()) {
        p->iMusic = -1;
        p->FadeType = RIXPLAYER::NONE;
        break;
      }
    }

    // 生成 OPL 数据
    p->opl->update(opl_buffer, samples_per_tick);

    // 计算并应用音量淡入淡出 (实时调整当前样本音量)
    if (p->FadeType != RIXPLAYER::NONE) {
      short *p_dst = opl_buffer;
      for (size_t i = 0; i < required_samples; i++) {
        int current_vol = SDL_MIX_MAXVOLUME;
        if (p->FadeType == RIXPLAYER::FADE_IN) {
          current_vol = (INT)(SDL_MIX_MAXVOLUME * (1.0 - (double)p->iRemainingFadeSamples / p->iTotalFadeInSamples));
        } else if (p->FadeType == RIXPLAYER::FADE_OUT) {
          current_vol = (INT)(SDL_MIX_MAXVOLUME * ((double)p->iRemainingFadeSamples / p->iTotalFadeOutSamples));
        }

        if (p->iRemainingFadeSamples > 0)
          p->iRemainingFadeSamples--;

        if (p->FadeType == RIXPLAYER::FADE_IN && p->iRemainingFadeSamples <= 0) {
          p->FadeType = RIXPLAYER::NONE;
        }

        *p_dst = (short)((*p_dst) * current_vol / SDL_MIX_MAXVOLUME);
        p_dst++;
      }
    }

    // 将生成及处理完毕的数据喂给 AudioStream
    SDL_PutAudioStreamData(p->stream, opl_buffer, bytes_per_tick);
    bytes_put += bytes_per_tick;
  }
}

VOID RIX_Shutdown() {
  if (gRixPlayer.stream) {
    SDL_UnbindAudioStream(gRixPlayer.stream);
    SDL_DestroyAudioStream(gRixPlayer.stream);
    gRixPlayer.stream = NULL;
  }
  if (gRixPlayer.rix)
    delete gRixPlayer.rix;
  if (gRixPlayer.opl)
    delete gRixPlayer.opl;
}

VOID RIX_Play(INT iNumRIX, BOOL fLoop, FLOAT flFadeTime) {
  RIXPLAYER *p = &gRixPlayer;

  // 音量淡化做在输入流 (OPL速率) 上
  int samples = (int)(flFadeTime * 0.5f * OPL_SAMPLE_RATE * AUDIO_CHANNELS);
  if (samples <= 0)
    samples = 1;

  p->iNextMusic = iNumRIX;
  p->fNextLoop = fLoop;

  p->FadeType = RIXPLAYER::FADE_OUT;
  p->dwStartFadeTime = PAL_GetTicks();

  p->iRemainingFadeSamples = samples;
  p->iTotalFadeOutSamples = samples;
  p->iTotalFadeInSamples = samples;
}

VOID RIX_Init(LPCSTR szFileName) {
  if (!szFileName)
    return;

  // gRixPlayer->opl = new CNemuopl(/*rate=*/OPL_SAMPLE_RATE);
  gRixPlayer.opl = new CWemuopl(/*rate=*/OPL_SAMPLE_RATE, /*bit16=*/true, /*stereo=*/true);
  if (!gRixPlayer.opl) {
    return;
  }

  gRixPlayer.rix = new CrixPlayer(gRixPlayer.opl);
  if (!gRixPlayer.rix) {
    delete gRixPlayer.opl;
    return;
  }

  // 加载 RIX 文件
  if (!gRixPlayer.rix->load(szFileName, CProvider_Filesystem())) {
    delete gRixPlayer.rix;
    delete gRixPlayer.opl;
    return;
  }

  SDL_AudioSpec srcSpec = {SDL_AUDIO_S16, AUDIO_CHANNELS, /*rate=*/OPL_SAMPLE_RATE};
  const SDL_AudioSpec *dstSpec = AUDIO_GetDeviceSpec();

  // 创建单一 Stream，用于完成速率转换及音频输出缓冲
  gRixPlayer.stream = SDL_CreateAudioStream(&srcSpec, dstSpec);
  if (!gRixPlayer.stream) {
    delete gRixPlayer.rix;
    delete gRixPlayer.opl;
    return;
  }

  gRixPlayer.FadeType = RIXPLAYER::NONE;
  gRixPlayer.iMusic = -1;
  gRixPlayer.iNextMusic = -1;
  gRixPlayer.fLoop = FALSE;
  gRixPlayer.fNextLoop = FALSE;

  // 设置流获取回调并绑定到音频设备
  SDL_SetAudioStreamGetCallback(gRixPlayer.stream, RIX_StreamCallback, nullptr);
  SDL_BindAudioStream(AUDIO_GetDeviceID(), gRixPlayer.stream);
}
