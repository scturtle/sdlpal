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

#ifndef AUDIO_H
#define AUDIO_H

#include "common.h"

#define AUDIO_CHANNELS 2
#define SAMPLE_RATE 44100
#define OPL_SAMPLE_RATE 49716

PAL_C_LINKAGE_BEGIN

typedef struct tagAUDIODEVICE {
  SDL_AudioDeviceID id;
  SDL_AudioSpec spec; /* Actual-used sound specification */
  VOID *pMusPlayer;
  VOID *pSoundPlayer;
  SDL_AudioStream *mus_stream;
  BOOL fMusicEnabled; /* Is BGM enabled? */
  BOOL fSoundEnabled; /* Is sound effect enabled? */
} AUDIODEVICE;

INT AUDIO_Init(VOID);

VOID AUDIO_Shutdown(VOID);

SDL_AudioDeviceID AUDIO_GetDeviceID(VOID);

SDL_AudioSpec *AUDIO_GetDeviceSpec(VOID);

VOID AUDIO_PlayMusic(INT iNumRIX, BOOL fLoop, FLOAT flFadeTime);

VOID AUDIO_PlaySound(INT iSoundNum);

VOID AUDIO_EnableMusic(BOOL fEnable);

BOOL AUDIO_MusicEnabled(VOID);

VOID AUDIO_EnableSound(BOOL fEnable);

BOOL AUDIO_SoundEnabled(VOID);

VOID *RIX_Init(LPCSTR szFileName);

VOID RIX_FillBuffer(VOID *object, LPBYTE stream, INT len);

BOOL RIX_Play(VOID *object, INT iNumRIX, BOOL fLoop, FLOAT flFadeTime);

VOID RIX_Shutdown(VOID *object);

VOID *SOUND_Init(VOID);

VOID SOUND_Shutdown(VOID *object);

VOID SOUND_FillBuffer(VOID *object, LPBYTE stream, INT len);

BOOL SOUND_Play(VOID *object, INT iSoundNum, BOOL fLoop, FLOAT flFadeTime);

PAL_C_LINKAGE_END

#define AUDIO_IsIntegerConversion(a) (((a) % gConfig.iSampleRate) == 0 || (gConfig.iSampleRate % (a)) == 0)

#endif
