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

volatile PALINPUTSTATE g_InputState;

static const int g_KeyMap[][2] = {
    {SDLK_UP, kKeyUp},         {SDLK_DOWN, kKeyDown},     {SDLK_LEFT, kKeyLeft},    {SDLK_RIGHT, kKeyRight},
    {SDLK_ESCAPE, kKeyMenu},   {SDLK_RETURN, kKeySearch}, {SDLK_SPACE, kKeySearch}, {SDLK_PAGEUP, kKeyPgUp},
    {SDLK_PAGEDOWN, kKeyPgDn}, {SDLK_HOME, kKeyHome},     {SDLK_END, kKeyEnd},      {SDLK_R, kKeyRepeat},
    {SDLK_A, kKeyAuto},        {SDLK_D, kKeyDefend},      {SDLK_E, kKeyUseItem},    {SDLK_W, kKeyThrowItem},
    {SDLK_Q, kKeyFlee},        {SDLK_F, kKeyForce},       {SDLK_S, kKeyStatus}};

static INT PAL_GetCurrDirection(VOID) {
  INT iCurrDir = kDirSouth;

  for (int i = 1; i < 4; i++)
    if (g_InputState.dwKeyOrder[iCurrDir] < g_InputState.dwKeyOrder[i])
      iCurrDir = i;

  if (!g_InputState.dwKeyOrder[iCurrDir])
    iCurrDir = kDirUnknown;

  return iCurrDir;
}

static VOID PAL_KeyDown(INT key) {
  INT iCurrDir = kDirUnknown;

  if (key & kKeyDown)
    iCurrDir = kDirSouth;
  else if (key & kKeyLeft)
    iCurrDir = kDirWest;
  else if (key & kKeyUp)
    iCurrDir = kDirNorth;
  else if (key & kKeyRight)
    iCurrDir = kDirEast;

  if (iCurrDir != kDirUnknown) {
    g_InputState.dwKeyMaxCount++;
    g_InputState.dwKeyOrder[iCurrDir] = g_InputState.dwKeyMaxCount;
    g_InputState.dir = PAL_GetCurrDirection();
  }

  g_InputState.dwKeyPress |= key;
}

static VOID PAL_KeyUp(INT key) {
  INT iCurrDir = kDirUnknown;

  if (key & kKeyDown)
    iCurrDir = kDirSouth;
  else if (key & kKeyLeft)
    iCurrDir = kDirWest;
  else if (key & kKeyUp)
    iCurrDir = kDirNorth;
  else if (key & kKeyRight)
    iCurrDir = kDirEast;

  if (iCurrDir != kDirUnknown) {
    g_InputState.dwKeyOrder[iCurrDir] = 0;
    iCurrDir = PAL_GetCurrDirection();
    g_InputState.dwKeyMaxCount = (iCurrDir == kDirUnknown) ? 0 : g_InputState.dwKeyOrder[iCurrDir];
    g_InputState.dir = iCurrDir;
  }
}

static VOID SDLCALL PAL_EventFilter(const SDL_Event *lpEvent) {

  switch (lpEvent->type) {
  case SDL_EVENT_WILL_ENTER_BACKGROUND:
    g_bRenderPaused = TRUE;
    break;

  case SDL_EVENT_DID_ENTER_FOREGROUND:
    g_bRenderPaused = FALSE;
    VIDEO_UpdateScreen(NULL);
    break;

  case SDL_EVENT_KEY_DOWN:
    if (lpEvent->key.mod & SDL_KMOD_ALT) {
      if (lpEvent->key.key == SDLK_RETURN) {
        VIDEO_ToggleFullscreen();
        break;
      }
    }

    for (int i = 0; i < sizeof(g_KeyMap) / sizeof(g_KeyMap[0]); i++) {
      if (g_KeyMap[i][0] == lpEvent->key.key) {
        PAL_KeyDown(g_KeyMap[i][1]);
      }
    }
    break;

  case SDL_EVENT_KEY_UP:
    for (int i = 0; i < sizeof(g_KeyMap) / sizeof(g_KeyMap[0]); i++) {
      if (g_KeyMap[i][0] == lpEvent->key.key) {
        PAL_KeyUp(g_KeyMap[i][1]);
      }
    }
    break;

  case SDL_EVENT_QUIT:
    PAL_Shutdown(0);
  }
}

VOID PAL_ClearKeyState(VOID) { g_InputState.dwKeyPress = 0; }

VOID PAL_InitInput(VOID) { g_InputState.dir = kDirUnknown; }

VOID PAL_ProcessEvent(VOID) {
  while (1) {
    SDL_Event evt;
    int ret = SDL_PollEvent(&evt);
    if (ret != 0)
      PAL_EventFilter(&evt);
    else
      break;
  }
}
