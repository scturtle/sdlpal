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

#include <SDL3/SDL_main.h>

static VOID PAL_Init(VOID) {
  int e;

  e = PAL_InitGlobals();
  if (e != 0)
    TerminateOnError("fail to init global data: %d.\n", e);

  e = VIDEO_Startup();
  if (e != 0)
    TerminateOnError("fail to init Video: %d.\n", e);

  e = PAL_InitUI();
  if (e != 0)
    TerminateOnError("fail to init UI: %d.\n", e);

  if (!PAL_InitFont())
    TerminateOnError("Fail to init font.\n");

  e = PAL_InitText();
  if (e != 0)
    TerminateOnError("fail to init text: %d.\n", e);

  PAL_InitInput();
  AUDIO_Init();

  VIDEO_SetWindowTitle("Pal");
}

VOID PAL_Shutdown(int exit_code) {
  AUDIO_Shutdown();
  PAL_FreeResources();
  PAL_FreeUI();
  VIDEO_Shutdown();
  PAL_FreeGlobals(); // at last
  SDL_Quit();
  exit(exit_code);
}

VOID PAL_TrademarkScreen(VOID) {
  if (PAL_PlayAVI("1.avi"))
    return;
}

VOID PAL_SplashScreen(VOID) {
  if (PAL_PlayAVI("2.avi"))
    return;
}

// The game entry routine.
static VOID PAL_GameMain(VOID) {
  DWORD dwTime;

  // Show the opening menu.
  g.saveSlot = PAL_OpeningMenu();

  // Initialize game data and set the flags to load the game resources.
  PAL_ReloadInNextTick(g.saveSlot);

  // Run the main game loop.
  dwTime = PAL_GetTicks();
  while (TRUE) {
    // Load the game resources if needed.
    PAL_LoadResources();

    // Clear the input state of previous frame.
    PAL_ClearKeyState();

    // Wait for the time of one frame. Accept input here.
    PAL_DelayUntil(dwTime);

    // Set the time of the next frame.
    dwTime = PAL_GetTicks() + FRAME_TIME;

    // Run the main frame routine.
    PAL_StartFrame();
  }
}

int main(int argc, char *argv[]) {
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO))
    TerminateOnError("Could not initialize SDL: %s.\n", SDL_GetError());

  PAL_LoadConfig(TRUE);

  // Initialize everything
  PAL_Init();

  // Show the trademark screen and splash screen
  /* PAL_TrademarkScreen(); */
  PAL_SplashScreen();

  // Run the main game routine
  PAL_GameMain();

  __builtin_unreachable();
}
