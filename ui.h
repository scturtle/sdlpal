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

#ifndef UI_H
#define UI_H

#include "common.h"

#define DESCTEXT_COLOR                     0x3C

#define CASH_LABEL                         21

#define STATUS_LABEL_EXP                   2
#define STATUS_LABEL_LEVEL                 48
#define STATUS_LABEL_HP                    49
#define STATUS_LABEL_MP                    50
#define STATUS_LABEL_ATTACKPOWER           51
#define STATUS_LABEL_MAGICPOWER            52
#define STATUS_LABEL_RESISTANCE            53
#define STATUS_LABEL_DEXTERITY             54
#define STATUS_LABEL_FLEERATE              55
#define STATUS_COLOR_EQUIPMENT             0xBE

#define BUYMENU_LABEL_CURRENT              35
#define SELLMENU_LABEL_PRICE               25

#define SPRITENUM_SLASH                    39
#define SPRITENUM_ITEMBOX                  70
#define SPRITENUM_CURSOR_YELLOW_UP         66
#define SPRITENUM_CURSOR_UP                67
#define SPRITENUM_CURSOR_YELLOW            68
#define SPRITENUM_CURSOR                   69
#define SPRITENUM_PLAYERINFOBOX            18
#define SPRITENUM_PLAYERFACE_FIRST         48

#define BATTLEWIN_GETEXP_LABEL             30
#define BATTLEWIN_BEATENEMY_LABEL          9
#define BATTLEWIN_DOLLAR_LABEL             10
#define BATTLEWIN_LEVELUP_LABEL            32
#define BATTLEWIN_ADDMAGIC_LABEL           33
#define BATTLEWIN_LEVELUP_LABEL_COLOR      0xBB
#define SPRITENUM_ARROW                    47

#define BATTLE_LABEL_ESCAPEFAIL            31

typedef struct tagBOX {
  SDL_Rect rect;
  SDL_Surface *lpSavedArea;
} BOX, *LPBOX;

typedef enum tagNUMCOLOR { kNumColorYellow, kNumColorBlue, kNumColorCyan } NUMCOLOR;

typedef enum tagNUMALIGN { kNumAlignLeft, kNumAlignMid, kNumAlignRight } NUMALIGN;

INT PAL_InitUI(VOID);

VOID PAL_FreeUI(VOID);

LPCBITMAPRLE PAL_GetUISprite(int spriteNum);

LPBOX
PAL_CreateBox(PAL_POS pos, INT nRows, INT nColumns, INT iStyle, BOOL fSaveScreen);

LPBOX
PAL_CreateBoxWithShadow(PAL_POS pos, INT nRows, INT nColumns, INT iStyle, BOOL fSaveScreen, INT nShadowOffset);

LPBOX
PAL_CreateSingleLineBox(PAL_POS pos, INT nLen, BOOL fSaveScreen);

LPBOX
PAL_CreateSingleLineBoxWithShadow(PAL_POS pos, INT nLen, BOOL fSaveScreen, INT nShadowOffset);

VOID PAL_DeleteBox(LPBOX lpBox);

VOID PAL_DrawNumber(UINT iNum, UINT nLength, PAL_POS pos, NUMCOLOR color, NUMALIGN align);

#endif
