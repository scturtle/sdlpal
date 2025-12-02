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

#ifndef _TEXT_H
#define _TEXT_H

#include "common.h"

typedef enum tagDIALOGPOSITION { kDialogUpper = 0, kDialogCenter, kDialogLower, kDialogCenterWindow } DIALOGLOCATION;

PAL_C_LINKAGE_BEGIN

BOOL PAL_InitFont();

int PAL_CharWidth(uint16_t wChar);

INT PAL_TextWidth(LPCWSTR text);

INT PAL_WordWidth(INT nWordIndex);

INT PAL_InitText(VOID);

LPCWSTR
PAL_GetWord(int iNumWord);

LPCWSTR
PAL_GetMsg(int iNumMsg);

VOID PAL_DrawText(LPCWSTR lpszText, PAL_POS pos, BYTE bColor, BOOL fShadow, BOOL fUpdate);

VOID PAL_StartDialog(BYTE bDialogLocation, BYTE bFontColor, INT iNumCharFace, BOOL fPlayingRNG);

VOID PAL_StartDialogWithOffset(BYTE bDialogLocation, BYTE bFontColor, INT iNumCharFace, BOOL fPlayingRNG, INT xOff,
                               INT yOff);

VOID PAL_ShowDialogText(LPCWSTR lpszText);

VOID PAL_ClearDialog(BOOL fWaitForKey);

VOID PAL_EndDialog(VOID);

BOOL PAL_IsInDialog(VOID);

BOOL PAL_DialogIsPlayingRNG(VOID);

VOID PAL_SetDialogShadow(int shadow);

INT PAL_swprintf(LPWSTR buffer, size_t count, LPCWSTR format, ...);

PAL_C_LINKAGE_END

#endif
