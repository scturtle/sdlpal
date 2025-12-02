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

#ifndef VIDEO_H
#define VIDEO_H

#include "common.h"

PAL_C_LINKAGE_BEGIN

VOID VIDEO_CopySurface(SDL_Surface *s, const SDL_Rect *sr, SDL_Surface *t, const SDL_Rect *tr);
VOID VIDEO_CopyEntireSurface(SDL_Surface *s, SDL_Surface *t);
VOID VIDEO_BackupScreen(SDL_Surface *s);
VOID VIDEO_RestoreScreen(SDL_Surface *t);
VOID VIDEO_FreeSurface(SDL_Surface *s);
VOID VIDEO_FillScreenBlack();

// Blit an uncompressed bitmap in FBP.MKF to an SDL surface. Assume surface is a 8-bit 320x200 one.
INT VIDEO_FBPBlitToSurface(LPBYTE lpBitmapFBP, SDL_Surface *lpDstSurface);

extern SDL_Surface *gpScreen;
extern SDL_Surface *gpScreenBak;
extern volatile BOOL g_bRenderPaused;

INT VIDEO_Startup(VOID);

VOID VIDEO_Shutdown(VOID);

VOID VIDEO_UpdateScreen(const SDL_Rect *lpRect);

VOID VIDEO_SetPalette(SDL_Color rgPalette[256]);

SDL_Color *VIDEO_GetPalette(VOID);

VOID VIDEO_ToggleFullscreen(VOID);

VOID VIDEO_ShakeScreen(WORD wShakeTime, WORD wShakeLevel);

VOID VIDEO_SwitchScreen(WORD wSpeed);

VOID VIDEO_FadeScreen(WORD wSpeed);

void VIDEO_SetWindowTitle(const char *pszTitle);

SDL_Surface *VIDEO_CreateCompatibleSurface(SDL_Surface *pSource);

SDL_Surface *VIDEO_CreateCompatibleSizedSurface(SDL_Surface *pSource, const SDL_Rect *pSize);

VOID VIDEO_DrawSurfaceToScreen(SDL_Surface *pSurface);

VOID VIDEO_RenderCopy(VOID);

PAL_C_LINKAGE_END

#endif
