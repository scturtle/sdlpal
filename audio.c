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
#include "aviplay.h"
#include "common.h"
#include "global.h"
#include "palcfg.h"
#include <math.h>

static AUDIODEVICE gAudioDevice;

SDL_AudioDeviceID AUDIO_GetDeviceID(VOID) { return gAudioDevice.id; }

INT AUDIO_Init(VOID) {
  gAudioDevice.fMusicEnabled = TRUE;
  gAudioDevice.fSoundEnabled = TRUE;

  // Open the audio device.
  gAudioDevice.spec.freq = SAMPLE_RATE;
  gAudioDevice.spec.format = SDL_AUDIO_S16LE;
  gAudioDevice.spec.channels = AUDIO_CHANNELS;

  gAudioDevice.id = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &gAudioDevice.spec);
  if (!gAudioDevice.id)
    return -3;

  // Initialize the sound subsystem.
  gAudioDevice.pSoundPlayer = SOUND_Init();

  // Initialize the music subsystem.
  char buf[PATH_MAX];
  gAudioDevice.pMusPlayer = RIX_Init(UTIL_GetFullPathName(buf, PATH_MAX, gConfig.pszGamePath, "mus.mkf"));

  // Let the callback function run so that musics will be played.
  SDL_ResumeAudioDevice(gAudioDevice.id);

  return 0;
}

VOID AUDIO_Shutdown(VOID) {
  if (gAudioDevice.pSoundPlayer != NULL) {
    SOUND_Shutdown(gAudioDevice.pSoundPlayer);
    gAudioDevice.pSoundPlayer = NULL;
  }
  if (gAudioDevice.pMusPlayer) {
    RIX_Shutdown(gAudioDevice.pMusPlayer);
    gAudioDevice.pMusPlayer = NULL;
  }
}

SDL_AudioSpec *AUDIO_GetDeviceSpec(VOID) { return &gAudioDevice.spec; }

VOID AUDIO_PlaySound(INT iSoundNum) {
  if (gAudioDevice.pSoundPlayer)
    SOUND_Play(gAudioDevice.pSoundPlayer, abs(iSoundNum), FALSE, 0.0f);
}

VOID AUDIO_PlayMusic(INT iNumRIX, BOOL fLoop, FLOAT flFadeTime) {
  if (gAudioDevice.pMusPlayer)
    RIX_Play(gAudioDevice.pMusPlayer, iNumRIX, fLoop, flFadeTime);
}

VOID AUDIO_EnableMusic(BOOL fEnable) { gAudioDevice.fMusicEnabled = fEnable; }

BOOL AUDIO_MusicEnabled(VOID) { return gAudioDevice.fMusicEnabled; }

VOID AUDIO_EnableSound(BOOL fEnable) { gAudioDevice.fSoundEnabled = fEnable; }

BOOL AUDIO_SoundEnabled(VOID) { return gAudioDevice.fSoundEnabled; }
