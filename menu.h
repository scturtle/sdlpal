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

#ifndef PAL_MENU_H
#define PAL_MENU_H

#include "common.h"

#define MENUITEM_COLOR                     0x4F
#define MENUITEM_COLOR_INACTIVE            0x18
#define MENUITEM_COLOR_CONFIRMED           0x2C
#define MENUITEM_COLOR_SELECTED_INACTIVE   0x1C
#define MENUITEM_COLOR_SELECTED_FIRST      0xF9
#define MENUITEM_COLOR_SELECTED_TOTALNUM   6

#define ITEMUSEMENU_COLOR_STATLABEL        0xBB

#define MENUITEM_COLOR_SELECTED                                    \
   (MENUITEM_COLOR_SELECTED_FIRST +                                \
      PAL_GetTicks() / (600 / MENUITEM_COLOR_SELECTED_TOTALNUM)    \
      % MENUITEM_COLOR_SELECTED_TOTALNUM)

#define MENUITEM_COLOR_EQUIPPEDITEM        0xC8

typedef struct tagMENUITEM {
  WORD wValue;
  WORD wNumWord;
  BOOL fEnabled;
  PAL_POS pos;
} MENUITEM, *LPMENUITEM;
typedef const MENUITEM *LPCMENUITEM;

typedef VOID (*LPITEMCHANGED_CALLBACK)(WORD);

#define MENUITEM_VALUE_CANCELLED 0xFFFF

PAL_C_LINKAGE_BEGIN
  
INT PAL_MenuTextMaxWidth(LPCMENUITEM rgMenuItem, INT nMenuItem);

WORD PAL_ItemSelectMenuUpdate(VOID);

VOID PAL_ItemSelectMenuInit(WORD wItemFlags);

WORD PAL_ItemSelectMenu(LPITEMCHANGED_CALLBACK lpfnMenuItemChanged, WORD wItemFlags);

WORD PAL_MagicSelectionMenuUpdate(VOID);

VOID PAL_MagicSelectionMenuInit(WORD wPlayerRole, BOOL fInBattle, WORD wDefaultMagic);

WORD PAL_MagicSelectionMenu(WORD wPlayerRole, BOOL fInBattle, WORD wDefaultMagic);

INT PAL_OpeningMenu(VOID);

INT PAL_SaveSlotMenu(WORD wDefaultSlot);

BOOL PAL_ConfirmMenu(VOID);

VOID PAL_InGameMagicMenu(VOID);

VOID PAL_InGameMenu(VOID);

VOID PAL_PlayerStatus(VOID);

WORD PAL_ItemUseMenu(WORD wItemToUse);

VOID PAL_BuyMenu(WORD wStoreNum);

VOID PAL_SellMenu(VOID);

VOID PAL_EquipItemMenu(WORD wItem);

PAL_C_LINKAGE_END

#endif
