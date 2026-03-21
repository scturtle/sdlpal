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

#include "menu.h"
#include "main.h"

const int kWordLength = 10;
static int gCurInvMenuItem = 0; // 当前物品栏选中的索引
static int gNumInventory = 0;   // 物品总数
static WORD gItemFlags = 0;
static BOOL gNoDesc = FALSE;

static struct MAGICITEM {
  WORD wMagic;
  WORD wMP;
  BOOL fEnabled;
} rgMagicItem[MAX_PLAYER_MAGICS];

static int gCurMagicMenuItem = 0; // 当前选中的魔法的索引
static int gNumMagic = 0;         // 魔法总数
static WORD gPlayerMP = 0;

static int PAL_CalculateMenuNewIndex(int currentIndex, int totalItems, int itemsPerLine, int linesPerPage) {
  int delta = 0;
  DWORD keys = g_InputState.dwKeyPress;

  // clang-format off
  if (keys & kKeyUp)         delta = -itemsPerLine;
  else if (keys & kKeyDown)  delta = itemsPerLine;
  else if (keys & kKeyLeft)  delta = -1;
  else if (keys & kKeyRight) delta = 1;
  else if (keys & kKeyPgUp)  delta = -(itemsPerLine * linesPerPage);
  else if (keys & kKeyPgDn)  delta = itemsPerLine * linesPerPage;
  else if (keys & kKeyHome)  delta = -currentIndex;
  else if (keys & kKeyEnd)   delta = totalItems - currentIndex - 1;
  // clang-format on

  int newIndex = currentIndex + delta;
  if (newIndex < 0)
    return 0;
  if (newIndex >= totalItems && totalItems > 0)
    return totalItems - 1;

  return newIndex;
}

static VOID PAL_RunDescScript(WORD wScript, int flag) {
  int line = 0;
  while (wScript && SCRIPT[wScript].wOperation != 0) {
    if (SCRIPT[wScript].wOperation == 0xFFFF) {
      WORD arg = flag | line++;
      wScript = PAL_RunAutoScript(wScript, arg);
    } else {
      wScript = PAL_RunAutoScript(wScript, 0);
    }
  }
}

INT PAL_MenuTextMaxWidth(LPCMENUITEM rgMenuItem, INT nMenuItem) {
  int r = 0;
  for (int i = 0; i < nMenuItem; i++) {
    int w = (PAL_TextWidth(PAL_GetWord(rgMenuItem[i].wNumWord)) + 8) >> 4;
    if (r < w)
      r = w;
  }
  return r;
}

// Update item selection menu based on user input.
// Return object ID of the selected item. 0 if cancelled, 0xFFFF if not confirmed.
WORD PAL_ItemSelectMenuUpdate(VOID) {
  const int iItemsPerLine = 32 / kWordLength;
  const int iItemTextWidth = 8 * kWordLength + 20;
  const int iLinesPerPage = 7;
  const int iCursorXOffset = kWordLength * 5 / 2;
  const int iAmountXOffset = kWordLength * 8 + 1;
  const int iPageLineOffset = (iLinesPerPage + 1) / 2;
  const int iPictureYOffset = 0;
  PAL_POS cursorPos = PAL_XY(15 + iCursorXOffset, 22);

  // Process input
  if (g_InputState.dwKeyPress & kKeyMenu)
    return 0;
  gCurInvMenuItem = PAL_CalculateMenuNewIndex(gCurInvMenuItem, gNumInventory, iItemsPerLine, iLinesPerPage);

  // cheat: add one to amount of current item // by scturtle
  if (g_InputState.dwKeyPress & kKeyAuto)
    g.rgInventory[gCurInvMenuItem].nAmount += 1;

  // Redraw the box
  PAL_CreateBoxWithShadow(PAL_XY(2, 0), iLinesPerPage - 1, 17, 1, FALSE, 0);

  // Draw the texts in the current page
  int i = gCurInvMenuItem / iItemsPerLine * iItemsPerLine - iItemsPerLine * iPageLineOffset;
  if (i < 0)
    i = 0;

  const int xBase = 0, yBase = 140;

  for (int j = 0; j < iLinesPerPage; j++) {
    for (int k = 0; k < iItemsPerLine; k++) {
      WORD wObject = g.rgInventory[i].wItem;
      BYTE bColor = MENUITEM_COLOR;

      if (i >= MAX_INVENTORY || wObject == 0) {
        // End of the list reached
        j = iLinesPerPage;
        break;
      }

      BOOL isSelected = (i == gCurInvMenuItem);
      BOOL isInactive = !(OBJECT[wObject].item.wFlags & gItemFlags) ||
                        ((SHORT)g.rgInventory[i].nAmount <= (SHORT)g.rgInventory[i].nAmountInUse);
      BOOL isEquipped = (g.rgInventory[i].nAmount == 0);

      if (isInactive) {
        bColor = isSelected ? MENUITEM_COLOR_SELECTED_INACTIVE : MENUITEM_COLOR_INACTIVE;
      } else if (isEquipped) {
        bColor = MENUITEM_COLOR_EQUIPPEDITEM;
      } else {
        bColor = isSelected ? MENUITEM_COLOR_SELECTED : MENUITEM_COLOR;
      }

      // Draw the text
      PAL_DrawText(PAL_GetWord(wObject), PAL_XY(15 + k * iItemTextWidth, 12 + j * 18), bColor, TRUE, FALSE);

      if (i == gCurInvMenuItem) {
        cursorPos = PAL_XY(15 + iCursorXOffset + k * iItemTextWidth, 22 + j * 18);

        // Draw the picture of current selected item
        PAL_RLEBlitToSurfaceWithShadow(PAL_GetUISprite(SPRITENUM_ITEMBOX), gpScreen,
                                       PAL_XY(xBase + 5, yBase + 5 - iPictureYOffset), TRUE);
        PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_ITEMBOX), gpScreen, PAL_XY(xBase, yBase - iPictureYOffset));

        static BYTE bufImage[2048];
        static WORD lastBitmap = 0xFFFF;
        WORD currBitmap = OBJECT[wObject].item.wBitmap;
        if (lastBitmap != currBitmap) {
          if (PAL_MKFReadChunk(bufImage, 2048, currBitmap, FP_BALL) > 0)
            lastBitmap = currBitmap;
          else
            lastBitmap = 0xFFFF;
        }
        if (lastBitmap != 0xFFFF)
          PAL_RLEBlitToSurface(bufImage, gpScreen, PAL_XY(xBase + 8, yBase + 7 - iPictureYOffset));
      }

      // Draw the amount of this item
      if ((SHORT)g.rgInventory[i].nAmount - (SHORT)g.rgInventory[i].nAmountInUse > 1) {
        PAL_DrawNumber(g.rgInventory[i].nAmount - g.rgInventory[i].nAmountInUse, 2,
                       PAL_XY(15 + iAmountXOffset + k * iItemTextWidth, 17 + j * 18), kNumColorCyan, kNumAlignRight);
      }

      i++;
    }
  }

  // Draw the cursor on the current selected item
  PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_CURSOR), gpScreen, cursorPos);

  WORD wObject = g.rgInventory[gCurInvMenuItem].wItem;

  // Draw the description of the selected item
  if (!gNoDesc)
    PAL_RunDescScript(OBJECT[wObject].item.wScriptDesc, PAL_ITEM_DESC_BOTTOM);

  if (g_InputState.dwKeyPress & kKeySearch) {
    if ((OBJECT[wObject].item.wFlags & gItemFlags) &&
        (SHORT)g.rgInventory[gCurInvMenuItem].nAmount > (SHORT)g.rgInventory[gCurInvMenuItem].nAmountInUse) {
      if (g.rgInventory[gCurInvMenuItem].nAmount > 0) {
        int x = gCurInvMenuItem % iItemsPerLine;
        int y = min(gCurInvMenuItem / iItemsPerLine, iPageLineOffset);

        PAL_DrawText(PAL_GetWord(wObject), PAL_XY(15 + x * iItemTextWidth, 12 + y * 18), MENUITEM_COLOR_CONFIRMED,
                     FALSE, FALSE);

        // Draw the cursor on the current selected item
        PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_CURSOR), gpScreen, cursorPos);
      }

      return wObject;
    }
  }

  return 0xFFFF;
}

// Initialize the item selection menu.
VOID PAL_ItemSelectMenuInit(WORD wItemFlags) {
  gItemFlags = wItemFlags;

  // Compress the inventory
  PAL_CompressInventory();

  // Count the total number of items in inventory
  gNumInventory = 0;
  while (gNumInventory < MAX_INVENTORY && g.rgInventory[gNumInventory].wItem != 0)
    gNumInventory++;

  // Also add usable equipped items to the list
  if ((wItemFlags & kItemFlagUsable) && !gInBattle) {
    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
      WORD p = PARTY_PLAYER(i);
      for (int j = 0; j < MAX_PLAYER_EQUIPMENTS; j++) {
        WORD w = PLAYER.rgwEquipment[j][p];
        if (OBJECT[w].item.wFlags & kItemFlagUsable) {
          if (gNumInventory < MAX_INVENTORY) {
            g.rgInventory[gNumInventory].wItem = w;
            g.rgInventory[gNumInventory].nAmount = 0;
            g.rgInventory[gNumInventory].nAmountInUse = (WORD)-1;
            gNumInventory++;
          }
        }
      }
    }
  }
}

WORD PAL_ItemSelectMenu(LPITEMCHANGED_CALLBACK lpfnMenuItemChanged, WORD wItemFlags) {
  PAL_ItemSelectMenuInit(wItemFlags);

  int iPrevIndex = gCurInvMenuItem;

  PAL_ClearKeyState();

  if (lpfnMenuItemChanged != NULL) {
    gNoDesc = TRUE; // 卖物品时不显示描述
    (*lpfnMenuItemChanged)(g.rgInventory[gCurInvMenuItem].wItem);
  }

  DWORD dwTime = PAL_GetTicks();

  while (TRUE) {
    if (lpfnMenuItemChanged == NULL) {
      PAL_MakeScene();
    }

    WORD w = PAL_ItemSelectMenuUpdate();
    VIDEO_UpdateScreen(NULL);

    PAL_ClearKeyState();

    PAL_ProcessEvent();
    while (!SDL_TICKS_PASSED(PAL_GetTicks(), dwTime)) {
      PAL_ProcessEvent();
      if (g_InputState.dwKeyPress != 0)
        break;
      PAL_Delay(5);
    }
    dwTime = PAL_GetTicks() + FRAME_TIME;

    if (w != 0xFFFF) {
      gNoDesc = FALSE;
      return w;
    }

    // selection is changed, call callback function
    if (iPrevIndex != gCurInvMenuItem) {
      if (gCurInvMenuItem >= 0 && gCurInvMenuItem < MAX_INVENTORY) {
        if (lpfnMenuItemChanged != NULL) {
          (*lpfnMenuItemChanged)(g.rgInventory[gCurInvMenuItem].wItem);
        }
      }
      iPrevIndex = gCurInvMenuItem;
    }
  }

  __builtin_unreachable();
}

WORD PAL_MagicSelectionMenuUpdate(VOID) {
  const int iItemsPerLine = 32 / kWordLength;
  const int iItemTextWidth = 8 * kWordLength + 7;
  const int iLinesPerPage = 5;
  const int iCursorXOffset = kWordLength * 5 / 2;
  const int iPageLineOffset = iLinesPerPage / 2;

  // Check for inputs
  if (g_InputState.dwKeyPress & kKeyMenu)
    return 0;
  gCurMagicMenuItem = PAL_CalculateMenuNewIndex(gCurMagicMenuItem, gNumMagic, iItemsPerLine, iLinesPerPage);

  // Create the box.
  PAL_CreateBoxWithShadow(PAL_XY(10, 42), iLinesPerPage - 1, 16, 1, FALSE, 0);

  // Draw the description of the selected magic.
  WORD wScript = OBJECT[rgMagicItem[gCurMagicMenuItem].wMagic].item.wScriptDesc;
  PAL_RunDescScript(wScript, 0);

  // Draw the MP of the selected magic.
  PAL_CreateSingleLineBox(PAL_XY(0, 0), 5, FALSE);
  PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_SLASH), gpScreen, PAL_XY(45, 14));
  PAL_DrawNumber(rgMagicItem[gCurMagicMenuItem].wMP, 4, PAL_XY(15, 14), kNumColorYellow, kNumAlignRight);
  PAL_DrawNumber(gPlayerMP, 4, PAL_XY(50, 14), kNumColorCyan, kNumAlignRight);

  // Draw the texts of the current page
  int i = gCurMagicMenuItem / iItemsPerLine * iItemsPerLine - iItemsPerLine * iPageLineOffset;
  if (i < 0)
    i = 0;

  for (int j = 0; j < iLinesPerPage; j++) {
    for (int k = 0; k < iItemsPerLine; k++) {
      BYTE bColor = MENUITEM_COLOR;

      if (i >= gNumMagic) {
        // End of the list reached
        j = iLinesPerPage;
        break;
      }

      if (i == gCurMagicMenuItem) {
        if (rgMagicItem[i].fEnabled)
          bColor = MENUITEM_COLOR_SELECTED;
        else
          bColor = MENUITEM_COLOR_SELECTED_INACTIVE;
      } else if (!rgMagicItem[i].fEnabled) {
        bColor = MENUITEM_COLOR_INACTIVE;
      }

      // Draw the text
      PAL_DrawText(PAL_GetWord(rgMagicItem[i].wMagic), PAL_XY(35 + k * iItemTextWidth, 54 + j * 18), bColor, TRUE,
                   FALSE);

      // Draw the cursor on the current selected item
      if (i == gCurMagicMenuItem) {
        PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_CURSOR), gpScreen,
                             PAL_XY(35 + iCursorXOffset + k * iItemTextWidth, 64 + j * 18));
      }

      i++;
    }
  }

  if (g_InputState.dwKeyPress & kKeySearch) {
    if (rgMagicItem[gCurMagicMenuItem].fEnabled) {
      int x = gCurMagicMenuItem % iItemsPerLine;
      int y = min(gCurMagicMenuItem / iItemsPerLine, iPageLineOffset);

      x = 35 + x * iItemTextWidth;
      y = 54 + y * 18;

      PAL_DrawText(PAL_GetWord(rgMagicItem[gCurMagicMenuItem].wMagic), PAL_XY(x, y), MENUITEM_COLOR_CONFIRMED, FALSE,
                   TRUE);

      // Draw the cursor on the current selected item
      PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_CURSOR), gpScreen, PAL_XY(x + iCursorXOffset, y + 10));

      return rgMagicItem[gCurMagicMenuItem].wMagic;
    }
  }

  return 0xFFFF;
}

VOID PAL_MagicSelectionMenuInit(WORD wPlayerRole, BOOL fInBattle, WORD wDefaultMagic) {
  gCurMagicMenuItem = 0;
  gNumMagic = 0;

  gPlayerMP = PLAYER.rgwMP[wPlayerRole];

  // Put all magics of this player to the array
  for (int i = 0; i < MAX_PLAYER_MAGICS; i++) {
    WORD w = PLAYER.rgwMagic[i][wPlayerRole];
    if (w != 0) {
      rgMagicItem[gNumMagic].wMagic = w;

      WORD num = OBJECT[w].magic.wMagicNumber;
      rgMagicItem[gNumMagic].wMP = MAGIC[num].wCostMP;

      rgMagicItem[gNumMagic].fEnabled = TRUE;
      if (rgMagicItem[gNumMagic].wMP > gPlayerMP)
        rgMagicItem[gNumMagic].fEnabled = FALSE;

      WORD flag = OBJECT[w].magic.wFlags;
      if (fInBattle) {
        if (!(flag & kMagicFlagUsableInBattle))
          rgMagicItem[gNumMagic].fEnabled = FALSE;
      } else {
        if (!(flag & kMagicFlagUsableOutsideBattle))
          rgMagicItem[gNumMagic].fEnabled = FALSE;
      }

      gNumMagic++;
    }
  }

  // Sort the array
  for (int i = 0; i < gNumMagic - 1; i++) {
    BOOL fCompleted = TRUE;
    for (int j = 0; j < gNumMagic - 1 - i; j++) {
      if (rgMagicItem[j].wMagic > rgMagicItem[j + 1].wMagic) {
        struct MAGICITEM t = rgMagicItem[j];
        rgMagicItem[j] = rgMagicItem[j + 1];
        rgMagicItem[j + 1] = t;
        fCompleted = FALSE;
      }
    }
    if (fCompleted)
      break;
  }

  // Place the cursor to the default item
  for (int i = 0; i < gNumMagic; i++) {
    if (rgMagicItem[i].wMagic == wDefaultMagic)
      gCurMagicMenuItem = i;
  }
}

WORD PAL_MagicSelectionMenu(WORD wPlayerRole, BOOL fInBattle, WORD wDefaultMagic) {
  PAL_MagicSelectionMenuInit(wPlayerRole, fInBattle, wDefaultMagic);
  PAL_ClearKeyState();

  DWORD dwTime = PAL_GetTicks();
  while (TRUE) {
    PAL_MakeScene();

    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++)
      PAL_PlayerInfoBox(PAL_XY(45 + 78 * i, 165), i, FALSE);

    WORD w = PAL_MagicSelectionMenuUpdate();
    VIDEO_UpdateScreen(NULL);

    PAL_ClearKeyState();
    if (w != 0xFFFF)
      return w;

    PAL_ProcessEvent();
    while (!SDL_TICKS_PASSED(PAL_GetTicks(), dwTime)) {
      PAL_ProcessEvent();
      if (g_InputState.dwKeyPress != 0)
        break;
      PAL_Delay(5);
    }
    dwTime = PAL_GetTicks() + FRAME_TIME;
  }

  __builtin_unreachable();
}

static WORD GetSavedTimes(int iSaveSlot) {
  WORD wSavedTimes = 0;
  FILE *fp = UTIL_OpenFile(PAL_va(0, "%d.rpg", iSaveSlot));
  if (fp != NULL) {
    if (fread(&wSavedTimes, sizeof(WORD), 1, fp) != 1)
      wSavedTimes = 0;
    fclose(fp);
  }
  return wSavedTimes;
}

static WORD PAL_ReadMenu(LPITEMCHANGED_CALLBACK lpfnMenuItemChanged, LPCMENUITEM rgMenuItem, INT nMenuItem,
                         WORD wDefaultItem, BYTE bLabelColor)
/*++
  Purpose:
    Execute a menu.

  Parameters:
    [IN]  lpfnMenuItemChanged - Callback function which is called when user
                                changed the current menu item.
    [IN]  rgMenuItem - Array of the menu items.
    [IN]  nMenuItem - Number of menu items.
    [IN]  wDefaultItem - default item index.
    [IN]  bLabelColor - color of the labels.

  Return value:
    Return value of the selected menu item. MENUITEM_VALUE_CANCELLED if cancelled.
--*/
{
  WORD wCurrentItem = (wDefaultItem < nMenuItem) ? wDefaultItem : 0;

  // Draw all the menu texts.
  g_bRenderPaused = TRUE;
  for (int i = 0; i < nMenuItem; i++) {
    BYTE bColor = rgMenuItem[i].fEnabled ? bLabelColor
                  : i == wCurrentItem    ? MENUITEM_COLOR_SELECTED_INACTIVE
                                         : MENUITEM_COLOR_INACTIVE;
    PAL_DrawText(PAL_GetWord(rgMenuItem[i].wNumWord), rgMenuItem[i].pos, bColor, TRUE, TRUE);
  }
  g_bRenderPaused = FALSE;

  VIDEO_UpdateScreen(NULL);

  if (lpfnMenuItemChanged != NULL) {
    (*lpfnMenuItemChanged)(rgMenuItem[wDefaultItem].wValue);
  }

  while (TRUE) {
    PAL_ClearKeyState();

    // Redraw the selected item if needed.
    if (rgMenuItem[wCurrentItem].fEnabled) {
      PAL_DrawText(PAL_GetWord(rgMenuItem[wCurrentItem].wNumWord), rgMenuItem[wCurrentItem].pos,
                   MENUITEM_COLOR_SELECTED, FALSE, TRUE);
    }

    PAL_ProcessEvent();

    if (g_InputState.dwKeyPress & (kKeyDown | kKeyRight | kKeyUp | kKeyLeft | kKeyDefend)) {
      g_bRenderPaused = TRUE;

      // Dehighlight the unselected item.
      BYTE bColor = rgMenuItem[wCurrentItem].fEnabled ? bLabelColor : MENUITEM_COLOR_INACTIVE;
      PAL_DrawText(PAL_GetWord(rgMenuItem[wCurrentItem].wNumWord), rgMenuItem[wCurrentItem].pos, bColor, FALSE, TRUE);

      if (g_InputState.dwKeyPress & (kKeyDown | kKeyRight)) {
        wCurrentItem = wCurrentItem == nMenuItem - 1 ? 0 : wCurrentItem + 1;
      } else if (g_InputState.dwKeyPress & (kKeyUp | kKeyLeft)) {
        wCurrentItem = wCurrentItem == 0 ? nMenuItem - 1 : wCurrentItem - 1;
      }

      // Highlight the selected item.
      bColor = rgMenuItem[wCurrentItem].fEnabled ? MENUITEM_COLOR_SELECTED : MENUITEM_COLOR_SELECTED_INACTIVE;
      PAL_DrawText(PAL_GetWord(rgMenuItem[wCurrentItem].wNumWord), rgMenuItem[wCurrentItem].pos, bColor, FALSE, TRUE);

      g_bRenderPaused = FALSE;
      VIDEO_UpdateScreen(NULL);

      if (lpfnMenuItemChanged != NULL) {
        (*lpfnMenuItemChanged)(rgMenuItem[wCurrentItem].wValue);
      }
    } else if (g_InputState.dwKeyPress & kKeyMenu) {
      // User cancelled
      BYTE bColor = rgMenuItem[wCurrentItem].fEnabled ? bLabelColor : MENUITEM_COLOR_INACTIVE;
      PAL_DrawText(PAL_GetWord(rgMenuItem[wCurrentItem].wNumWord), rgMenuItem[wCurrentItem].pos, bColor, FALSE, TRUE);
      break;
    } else if (g_InputState.dwKeyPress & kKeySearch) {
      // User pressed Enter
      if (rgMenuItem[wCurrentItem].fEnabled) {
        PAL_DrawText(PAL_GetWord(rgMenuItem[wCurrentItem].wNumWord), rgMenuItem[wCurrentItem].pos,
                     MENUITEM_COLOR_CONFIRMED, FALSE, TRUE);
        return rgMenuItem[wCurrentItem].wValue;
      }
    }

    // Use delay function to avoid high CPU usage.
    PAL_Delay(50);
  }

  return MENUITEM_VALUE_CANCELLED;
}

static VOID PAL_DrawOpeningMenuBackground(VOID) {
  LPBYTE buf = (LPBYTE)malloc(320 * 200);
  if (buf == NULL)
    return;
  PAL_MKFDecompressChunk(buf, 320 * 200, 2, FP_FBP);
  VIDEO_FBPBlitToSurface(buf, gpScreen);
  VIDEO_UpdateScreen(NULL);
  free(buf);
}

// Return which saved slot to load from (1-5). 0 to start a new game.
INT PAL_OpeningMenu(VOID) {
  // Play the background music
  AUDIO_PlayMusic(4, TRUE, 1);

  // Draw the background
  PAL_DrawOpeningMenuBackground();
  PAL_FadeIn(0, FALSE, 1);

  WORD wItemSelected;
  WORD wDefaultItem = 0;

  MENUITEM rgMainMenuItem[2] = {// value label enabled position
                                {0, 7, TRUE, PAL_XY(125, 95)},
                                {1, 8, TRUE, PAL_XY(125, 112)}};

  while (TRUE) {
    // Activate the menu
    wItemSelected = PAL_ReadMenu(NULL, rgMainMenuItem, 2, wDefaultItem, MENUITEM_COLOR);

    if (wItemSelected == MENUITEM_VALUE_CANCELLED) {
      PAL_QuitGame();
      continue;
    }

    if (wItemSelected == 0) {
      // Start a new game
      break;
    } else {
      // Load game
      VIDEO_BackupScreen(gpScreen);
      wItemSelected = PAL_SaveSlotMenu(1);
      VIDEO_RestoreScreen(gpScreen);
      VIDEO_UpdateScreen(NULL);
      if (wItemSelected != MENUITEM_VALUE_CANCELLED) {
        break;
      }
      wDefaultItem = 0;
    }
  }

  // Fade out the screen and the music
  AUDIO_PlayMusic(0, FALSE, 1);
  PAL_FadeOut(1);

  if (wItemSelected == 0) {
    PAL_PlayAVI("3.avi");
  }

  return (INT)wItemSelected;
}

INT PAL_SaveSlotMenu(WORD wDefaultSlot) {
  // Create the boxes and create the menu items
  LPBOX rgpBox[5];
  MENUITEM rgMenuItem[5];
  for (int i = 0; i < 5; i++) {
    rgpBox[i] = PAL_CreateSingleLineBox(PAL_XY(195, 7 + 38 * i), 6, FALSE);
    rgMenuItem[i].wValue = i + 1;
    rgMenuItem[i].fEnabled = TRUE;
    rgMenuItem[i].wNumWord = 43 + i;
    rgMenuItem[i].pos = PAL_XY(210, 17 + 38 * i);
  }

  // Draw the numbers of saved times
  for (int i = 1; i <= 5; i++)
    PAL_DrawNumber((UINT)GetSavedTimes(i), 4, PAL_XY(270, 38 * i - 17), kNumColorYellow, kNumAlignRight);

  // Activate the menu
  WORD wItemSelected = PAL_ReadMenu(NULL, rgMenuItem, 5, wDefaultSlot - 1, MENUITEM_COLOR);

  // Delete the boxes
  for (int i = 0; i < 5; i++)
    PAL_DeleteBox(rgpBox[i]);

  const SDL_Rect rect = {195, 7, 120, 190};
  VIDEO_UpdateScreen(&rect);

  return wItemSelected;
}

static WORD PAL_SelectionMenu(int nWords, int nDefault, WORD wItems[]) {
  LPBOX rgpBox[4];
  MENUITEM rgMenuItem[4];
  int w[4] = {(nWords >= 1 && wItems[0]) ? PAL_WordWidth(wItems[0]) : 1,
              (nWords >= 2 && wItems[1]) ? PAL_WordWidth(wItems[1]) : 1,
              (nWords >= 3 && wItems[2]) ? PAL_WordWidth(wItems[2]) : 1,
              (nWords >= 4 && wItems[3]) ? PAL_WordWidth(wItems[3]) : 1};
  int dx[4] = {(w[0] - 1) * 16, (w[1] - 1) * 16, (w[2] - 1) * 16, (w[3] - 1) * 16}, i;
  PAL_POS pos[4] = {PAL_XY(145, 110), PAL_XY(220 + dx[0], 110), PAL_XY(145, 160), PAL_XY(220 + dx[2], 160)};
  WORD wReturnValue;

  const SDL_Rect rect = {130, 100, 125 + max(dx[0] + dx[1], dx[2] + dx[3]), 100};

  for (i = 0; i < nWords; i++)
    if (nWords > i && !wItems[i])
      return MENUITEM_VALUE_CANCELLED;

  // Create menu items
  for (i = 0; i < nWords; i++) {
    rgMenuItem[i].fEnabled = TRUE;
    rgMenuItem[i].pos = pos[i];
    rgMenuItem[i].wValue = i;
    rgMenuItem[i].wNumWord = wItems[i];
  }

  // Create the boxes
  dx[1] = dx[0];
  dx[3] = dx[2];
  dx[0] = dx[2] = 0;
  for (i = 0; i < nWords; i++) {
    rgpBox[i] = PAL_CreateSingleLineBox(PAL_XY(130 + 75 * (i % 2) + dx[i], 100 + 50 * (i / 2)), w[i] + 1, TRUE);
  }

  // Activate the menu
  wReturnValue = PAL_ReadMenu(NULL, rgMenuItem, nWords, nDefault, MENUITEM_COLOR);

  // Delete the boxes
  for (i = 0; i < nWords; i++) {
    PAL_DeleteBox(rgpBox[i]);
  }

  VIDEO_UpdateScreen(&rect);

  return wReturnValue;
}

BOOL PAL_ConfirmMenu(VOID) {
  WORD wItems[2] = {19, 20};
  WORD wReturnValue = PAL_SelectionMenu(2, 0, wItems);
  return (wReturnValue == MENUITEM_VALUE_CANCELLED || wReturnValue == 0) ? FALSE : TRUE;
}

static BOOL PAL_SwitchMenu(BOOL fEnabled) {
  WORD wItems[2] = {17, 18};
  WORD wReturnValue = PAL_SelectionMenu(2, fEnabled ? 1 : 0, wItems);
  return (wReturnValue == MENUITEM_VALUE_CANCELLED) ? fEnabled : ((wReturnValue == 0) ? FALSE : TRUE);
}

// Show the cash amount at the top left corner of the screen.
// Return pointer to the saved screen part.
LPBOX
PAL_ShowCash(DWORD dwCash) {
  LPBOX lpBox = PAL_CreateSingleLineBox(PAL_XY(0, 0), 5, TRUE);
  if (lpBox == NULL)
    return NULL;
  // Draw the text label.
  PAL_DrawText(PAL_GetWord(CASH_LABEL), PAL_XY(10, 10), 0, FALSE, FALSE);
  // Draw the cash amount.
  PAL_DrawNumber(dwCash, 6, PAL_XY(49, 14), kNumColorYellow, kNumAlignRight);
  return lpBox;
}

static WORD iCurSystemMenuItem;
static VOID PAL_SystemMenu_OnItemChange(WORD wCurrentItem) { iCurSystemMenuItem = wCurrentItem - 1; }

// Return TRUE if user made some operations in the menu, FALSE if user cancelled.
static BOOL PAL_SystemMenu(VOID) {
  const SDL_Rect rect = {40, 60, 280, 135};

  // Create menu items
  const MENUITEM rgSystemMenuItem[] = {
      // value label          enabled pos
      {1, 11, TRUE, PAL_XY(53, 72)},      //
      {2, 12, TRUE, PAL_XY(53, 72 + 18)}, //
      {3, 13, TRUE, PAL_XY(53, 72 + 36)}, //
      {4, 14, TRUE, PAL_XY(53, 72 + 54)}, //
      {5, 15, TRUE, PAL_XY(53, 72 + 72)},
  };
  const int nSystemMenuItem = sizeof(rgSystemMenuItem) / sizeof(MENUITEM);

  // Create the menu box.
  LPBOX lpMenuBox = PAL_CreateBox(PAL_XY(40, 60), nSystemMenuItem - 1,
                                  PAL_MenuTextMaxWidth(rgSystemMenuItem, nSystemMenuItem) - 1, 0, TRUE);

  // Perform the menu.
  WORD wReturnValue =
      PAL_ReadMenu(PAL_SystemMenu_OnItemChange, rgSystemMenuItem, nSystemMenuItem, iCurSystemMenuItem, MENUITEM_COLOR);

  if (wReturnValue == MENUITEM_VALUE_CANCELLED) {
    // User cancelled the menu
    PAL_DeleteBox(lpMenuBox);
    VIDEO_UpdateScreen(&rect);
    return FALSE;
  }

  int iSlot;
  switch (wReturnValue) {
  case 1:
    // Save game
    iSlot = PAL_SaveSlotMenu(gCurrentSaveSlot);
    if (iSlot != MENUITEM_VALUE_CANCELLED) {
      WORD wSavedTimes = 0;
      for (int i = 1; i <= 5; i++) {
        WORD curSavedTimes = GetSavedTimes(i);
        if (curSavedTimes > wSavedTimes)
          wSavedTimes = curSavedTimes;
      }
      PAL_SaveGame(iSlot, wSavedTimes + 1);
    }
    break;

  case 2:
    // Load game
    iSlot = PAL_SaveSlotMenu(gCurrentSaveSlot);
    if (iSlot != MENUITEM_VALUE_CANCELLED) {
      AUDIO_PlayMusic(0, FALSE, 1);
      PAL_FadeOut(1);
      PAL_ReloadInNextTick(iSlot);
    }
    break;

  case 3:
    // Music
    AUDIO_EnableMusic(PAL_SwitchMenu(AUDIO_MusicEnabled()));
    break;

  case 4:
    // Sound
    AUDIO_EnableSound(PAL_SwitchMenu(AUDIO_SoundEnabled()));
    break;

  case 5:
    // Quit
    PAL_QuitGame();
    break;
  }

  PAL_DeleteBox(lpMenuBox);
  return TRUE;
}

static void PAL_DrawPlayerInfoBoxes() {
  for (int i = 0; i <= g.wMaxPartyMemberIndex; i++)
    PAL_PlayerInfoBox(PAL_XY(45 + 78 * i, 165), i, TRUE);
}

static WORD PAL_SelectPlayer(WORD wPlayer, WORD w, WORD wMagic) {
  // Redraw the player info boxes first
  PAL_DrawPlayerInfoBoxes();

  // Draw the cursor on the selected item
  SDL_Rect rect = {0, 158, 320, 6};
  VIDEO_RestoreScreen(gpScreen);
  PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_CURSOR_UP), gpScreen, PAL_XY(75 + 78 * wPlayer, rect.y));
  VIDEO_UpdateScreen(&rect);

  while (TRUE) {
    PAL_ClearKeyState();
    PAL_ProcessEvent();

    if (g_InputState.dwKeyPress & kKeyMenu) {
      wPlayer = MENUITEM_VALUE_CANCELLED;
      break;
    } else if (g_InputState.dwKeyPress & kKeySearch) {
      OBJECT[wMagic].magic.wScriptOnUse =
          PAL_RunTriggerScript(OBJECT[wMagic].magic.wScriptOnUse, PARTY_PLAYER(wPlayer));

      if (g_fScriptSuccess) {
        OBJECT[wMagic].magic.wScriptOnSuccess =
            PAL_RunTriggerScript(OBJECT[wMagic].magic.wScriptOnSuccess, PARTY_PLAYER(wPlayer));

        if (g_fScriptSuccess) {
          PLAYER.rgwMP[PARTY_PLAYER(w)] -= MAGIC[OBJECT[wMagic].magic.wMagicNumber].wCostMP;

          //
          // Check if we have run out of MP
          //
          if (PLAYER.rgwMP[PARTY_PLAYER(w)] < MAGIC[OBJECT[wMagic].magic.wMagicNumber].wCostMP) {
            //
            // Don't go further if run out of MP
            //
            wPlayer = MENUITEM_VALUE_CANCELLED;
          }
        }
      }

      break;
    } else if (g_InputState.dwKeyPress & (kKeyLeft | kKeyUp)) {
      if (wPlayer > 0) {
        wPlayer--;
        break;
      }
    } else if (g_InputState.dwKeyPress & (kKeyRight | kKeyDown)) {
      if (wPlayer < g.wMaxPartyMemberIndex) {
        wPlayer++;
        break;
      }
    }

    PAL_Delay(1);
  }
  return wPlayer;
}

VOID PAL_InGameMagicMenu(VOID) {
  static WORD w;

  if (g.wMaxPartyMemberIndex == 0) {
    w = 0;
  } else {
    // Draw the player info boxes
    PAL_DrawPlayerInfoBoxes();

    // Generate one menu items for each player in the party
    int y = 75;
    MENUITEM rgMenuItem[MAX_PLAYERS_IN_PARTY];
    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
      assert(i <= MAX_PLAYERS_IN_PARTY);
      rgMenuItem[i].wValue = i;
      rgMenuItem[i].wNumWord = PLAYER.rgwName[PARTY_PLAYER(i)];
      rgMenuItem[i].fEnabled = (PLAYER.rgwHP[PARTY_PLAYER(i)] > 0);
      rgMenuItem[i].pos = PAL_XY(48, y);
      y += 18;
    }

    // Draw the box
    PAL_CreateBox(PAL_XY(35, 62), g.wMaxPartyMemberIndex,
                  PAL_MenuTextMaxWidth(rgMenuItem, g.wMaxPartyMemberIndex + 1) - 1, 0, FALSE);

    w = PAL_ReadMenu(NULL, rgMenuItem, g.wMaxPartyMemberIndex + 1, w, MENUITEM_COLOR);
    if (w == MENUITEM_VALUE_CANCELLED) {
      return;
    }
  }

  WORD wMagic = 0;

  while (TRUE) {
    wMagic = PAL_MagicSelectionMenu(PARTY_PLAYER(w), FALSE, wMagic);
    if (wMagic == 0) {
      break;
    }

    VIDEO_BackupScreen(gpScreen);

    if (OBJECT[wMagic].magic.wFlags & kMagicFlagApplyToAll) {
      OBJECT[wMagic].magic.wScriptOnUse = PAL_RunTriggerScript(OBJECT[wMagic].magic.wScriptOnUse, 0);

      if (g_fScriptSuccess) {
        OBJECT[wMagic].magic.wScriptOnSuccess = PAL_RunTriggerScript(OBJECT[wMagic].magic.wScriptOnSuccess, 0);

        if (g_fScriptSuccess)
          PLAYER.rgwMP[PARTY_PLAYER(w)] -= MAGIC[OBJECT[wMagic].magic.wMagicNumber].wCostMP;
      }

      if (gNeedToFadeIn) {
        PAL_FadeIn(gCurPalette, gNightPalette, 1);
        gNeedToFadeIn = FALSE;
      }
    } else {
      // Need to select which player to use the magic on.
      WORD wPlayer = 0;

      while (wPlayer != MENUITEM_VALUE_CANCELLED) {
        wPlayer = PAL_SelectPlayer(wPlayer, w, wMagic);
      }
    }

    // Redraw the player info boxes
    PAL_DrawPlayerInfoBoxes();
  }
}

static BOOL PAL_InventoryMenu(VOID) {

  MENUITEM rgMenuItem[2] = {
      {1, 22, TRUE, PAL_XY(43, 73)},
      {2, 23, TRUE, PAL_XY(43, 73 + 18)},
  };

  LPBOX lpBox = PAL_CreateBox(PAL_XY(30, 60), 1, PAL_MenuTextMaxWidth(rgMenuItem, 2) - 1, 0, TRUE);

  static WORD w = 0;
  w = PAL_ReadMenu(NULL, rgMenuItem, 2, w - 1, MENUITEM_COLOR);

  if (w == MENUITEM_VALUE_CANCELLED) {
    if (lpBox) {
      SDL_Rect rect = lpBox->rect;
      PAL_DeleteBox(lpBox);
      VIDEO_UpdateScreen(&rect);
    }
    return FALSE;
  }

  switch (w) {
  case 1:
    PAL_GameEquipItem();
    break;

  case 2:
    PAL_GameUseItem();
    break;
  }

  PAL_DeleteBox(lpBox);
  return TRUE;
}

static WORD iCurMainMenuItem;
static VOID PAL_InGameMenu_OnItemChange(WORD wCurrentItem) { iCurMainMenuItem = wCurrentItem - 1; }

VOID PAL_InGameMenu(VOID) {
  // Fix render problem with shadow
  VIDEO_BackupScreen(gpScreen);

  // Create menu items
  MENUITEM rgMainMenuItem[4] = {
      // value label enabled pos
      {1, 3, TRUE, PAL_XY(16, 50)},
      {2, 4, TRUE, PAL_XY(16, 50 + 18)},
      {3, 5, TRUE, PAL_XY(16, 50 + 36)},
      {4, 6, TRUE, PAL_XY(16, 50 + 54)},
  };

  // Display the cash amount.
  LPBOX lpCashBox = PAL_ShowCash(g.dwCash);

  // Create the menu box.
  // Fix render problem with shadow
  LPBOX lpMenuBox = PAL_CreateBox(PAL_XY(3, 37), 3, PAL_MenuTextMaxWidth(rgMainMenuItem, 4) - 1, 0, FALSE);

  // Process the menu
  while (TRUE) {
    WORD wReturnValue = PAL_ReadMenu(PAL_InGameMenu_OnItemChange, rgMainMenuItem, 4, iCurMainMenuItem, MENUITEM_COLOR);
    if (wReturnValue == MENUITEM_VALUE_CANCELLED) {
      break;
    }
    if (wReturnValue == 1) {
      // Status
      PAL_PlayerStatus();
      break;
    } else if (wReturnValue == 2) {
      // Magic
      PAL_InGameMagicMenu();
      break;
    } else if (wReturnValue == 3) {
      // Inventory
      if (PAL_InventoryMenu())
        break;
    } else if (wReturnValue == 4) {
      // System
      if (PAL_SystemMenu())
        break;
    }
  }

  // Remove the boxes.
  PAL_DeleteBox(lpCashBox);
  PAL_DeleteBox(lpMenuBox);

  // Fix render problem with shadow
  VIDEO_RestoreScreen(gpScreen);
}

VOID PAL_PlayerStatus(VOID) {
  struct {
    int label;
    PAL_POS pos;
  } statusUI[] = {{STATUS_LABEL_EXP, PAL_XY(6, 6)},          {STATUS_LABEL_LEVEL, PAL_XY(6, 32)},
                  {STATUS_LABEL_HP, PAL_XY(6, 54)},          {STATUS_LABEL_MP, PAL_XY(6, 76)},
                  {STATUS_LABEL_ATTACKPOWER, PAL_XY(6, 98)}, {STATUS_LABEL_MAGICPOWER, PAL_XY(6, 118)},
                  {STATUS_LABEL_RESISTANCE, PAL_XY(6, 138)}, {STATUS_LABEL_DEXTERITY, PAL_XY(6, 158)},
                  {STATUS_LABEL_FLEERATE, PAL_XY(6, 178)}};
  PAL_POS roleEquipNames[MAX_PLAYER_EQUIPMENTS] = {PAL_XY(195, 38),  PAL_XY(253, 78),  PAL_XY(257, 140),
                                                   PAL_XY(207, 172), PAL_XY(147, 180), PAL_XY(87, 164)};
  PAL_POS roleEquipImageBoxes[MAX_PLAYER_EQUIPMENTS] = {PAL_XY(189, -1),  PAL_XY(247, 39),  PAL_XY(251, 101),
                                                        PAL_XY(201, 133), PAL_XY(141, 141), PAL_XY(81, 125)};
  PAL_POS rolePoisonNames[MAX_POISONS] = {PAL_XY(185, 58),  PAL_XY(185, 76),  PAL_XY(185, 94),  PAL_XY(185, 112),
                                          PAL_XY(185, 130), PAL_XY(185, 148), PAL_XY(185, 166), PAL_XY(185, 184),
                                          PAL_XY(185, 184), PAL_XY(185, 184)};

  BYTE bufBackground[320 * 200];
  BYTE bufImage[PAL_RLEBUFSIZE];

  PAL_MKFDecompressChunk(bufBackground, 320 * 200, 0, FP_FBP);

  int iCurrent = 0;
  while (iCurrent >= 0 && iCurrent <= g.wMaxPartyMemberIndex) {
    int iPlayerRole = PARTY_PLAYER(iCurrent);

    // Draw the background image
    VIDEO_FBPBlitToSurface(bufBackground, gpScreen);

    // Draw the name
    PAL_DrawText(PAL_GetWord(PLAYER.rgwName[iPlayerRole]), PAL_XY(110, 8), MENUITEM_COLOR_CONFIRMED, TRUE, FALSE);

    // Draw the image of player role
    if (PAL_MKFReadChunk(bufImage, PAL_RLEBUFSIZE, PLAYER.rgwAvatar[iPlayerRole], FP_RGM) > 0)
      PAL_RLEBlitToSurface(bufImage, gpScreen, PAL_XY(110, 30));

    // Draw the equipments
    for (int i = 0; i < MAX_PLAYER_EQUIPMENTS; i++) {
      WORD w = PLAYER.rgwEquipment[i][iPlayerRole];
      if (w == 0)
        continue;

      // Draw the image
      if (PAL_MKFReadChunk(bufImage, PAL_RLEBUFSIZE, OBJECT[w].item.wBitmap, FP_BALL) > 0)
        PAL_RLEBlitToSurface(bufImage, gpScreen, PAL_XY_OFFSET(roleEquipImageBoxes[i], 1, 1));

      // Draw the equipment text label
      int offset = PAL_WordWidth(w) * 16;
      if (PAL_X(roleEquipNames[i]) + offset > 320)
        offset = 320 - PAL_X(roleEquipNames[i]) - offset;
      else
        offset = 0;
      PAL_DrawText(PAL_GetWord(w), PAL_XY_OFFSET(roleEquipNames[i], offset, 0), STATUS_COLOR_EQUIPMENT, TRUE, FALSE);
    }

    // Draw the status text labels
    for (int i = 0; i < sizeof(statusUI) / sizeof(statusUI[0]); i++) {
      PAL_DrawText(PAL_GetWord(statusUI[i].label), statusUI[i].pos, MENUITEM_COLOR, TRUE, FALSE);
    }

    // Draw the status stats
    PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_SLASH), gpScreen, PAL_XY(65, 58));
    PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_SLASH), gpScreen, PAL_XY(65, 80));

    PAL_DrawNumber(g.Exp.rgPrimaryExp[iPlayerRole].wExp, 5, PAL_XY(58, 6), kNumColorYellow, kNumAlignRight);
    PAL_DrawNumber(LEVELUP_EXP[PLAYER.rgwLevel[iPlayerRole]], 5, PAL_XY(58, 15), kNumColorCyan, kNumAlignRight);
    PAL_DrawNumber(PLAYER.rgwLevel[iPlayerRole], 2, PAL_XY(54, 35), kNumColorYellow, kNumAlignRight);
    PAL_DrawNumber(PLAYER.rgwHP[iPlayerRole], 4, PAL_XY(42, 56), kNumColorYellow, kNumAlignRight);
    PAL_DrawNumber(PLAYER.rgwMaxHP[iPlayerRole], 4, PAL_XY(63, 61), kNumColorBlue, kNumAlignRight);
    PAL_DrawNumber(PLAYER.rgwMP[iPlayerRole], 4, PAL_XY(42, 78), kNumColorYellow, kNumAlignRight);
    PAL_DrawNumber(PLAYER.rgwMaxMP[iPlayerRole], 4, PAL_XY(63, 83), kNumColorBlue, kNumAlignRight);

    PAL_DrawNumber(PAL_GetPlayerAttackStrength(iPlayerRole), 4, PAL_XY(42, 102), kNumColorYellow, kNumAlignRight);
    PAL_DrawNumber(PAL_GetPlayerMagicStrength(iPlayerRole), 4, PAL_XY(42, 122), kNumColorYellow, kNumAlignRight);
    PAL_DrawNumber(PAL_GetPlayerDefense(iPlayerRole), 4, PAL_XY(42, 142), kNumColorYellow, kNumAlignRight);
    PAL_DrawNumber(PAL_GetPlayerDexterity(iPlayerRole), 4, PAL_XY(42, 162), kNumColorYellow, kNumAlignRight);
    PAL_DrawNumber(PAL_GetPlayerFleeRate(iPlayerRole), 4, PAL_XY(42, 182), kNumColorYellow, kNumAlignRight);

    // Draw all poisons
    for (int i = 0, j = 0; i < MAX_POISONS; i++) {
      WORD w = g.rgPoisonStatus[i][iCurrent].wPoisonID;
      if (w != 0 && OBJECT[w].poison.wPoisonLevel <= 3) {
        PAL_DrawText(PAL_GetWord(w), rolePoisonNames[j++], (BYTE)(OBJECT[w].poison.wColor + 10), TRUE, FALSE);
      }
    }

    // Update the screen
    VIDEO_UpdateScreen(NULL);

    // Wait for input
    PAL_ClearKeyState();

    while (TRUE) {
      UTIL_Delay(1);
      if (g_InputState.dwKeyPress & kKeyMenu) {
        iCurrent = -1;
        break;
      } else if (g_InputState.dwKeyPress & (kKeyLeft | kKeyUp)) {
        iCurrent--;
        break;
      } else if (g_InputState.dwKeyPress & (kKeyRight | kKeyDown | kKeySearch)) {
        iCurrent++;
        break;
      }
    }
  }
}

WORD PAL_ItemUseMenu(WORD wItemToUse) {
  BYTE bSelectedColor = MENUITEM_COLOR_SELECTED_FIRST;
  static SHORT sSelectedPlayer = 0;

  while (TRUE) {
    if (sSelectedPlayer > g.wMaxPartyMemberIndex) {
      sSelectedPlayer = 0;
    }

    // Draw the box
    PAL_CreateBox(PAL_XY(110, 2), 7, 9, 0, FALSE);

    // Draw the stats of the selected player
    PAL_DrawText(PAL_GetWord(STATUS_LABEL_LEVEL), PAL_XY(200, 16), ITEMUSEMENU_COLOR_STATLABEL, TRUE, FALSE);
    PAL_DrawText(PAL_GetWord(STATUS_LABEL_HP), PAL_XY(200, 34), ITEMUSEMENU_COLOR_STATLABEL, TRUE, FALSE);
    PAL_DrawText(PAL_GetWord(STATUS_LABEL_MP), PAL_XY(200, 52), ITEMUSEMENU_COLOR_STATLABEL, TRUE, FALSE);
    PAL_DrawText(PAL_GetWord(STATUS_LABEL_ATTACKPOWER), PAL_XY(200, 70), ITEMUSEMENU_COLOR_STATLABEL, TRUE, FALSE);
    PAL_DrawText(PAL_GetWord(STATUS_LABEL_MAGICPOWER), PAL_XY(200, 88), ITEMUSEMENU_COLOR_STATLABEL, TRUE, FALSE);
    PAL_DrawText(PAL_GetWord(STATUS_LABEL_RESISTANCE), PAL_XY(200, 106), ITEMUSEMENU_COLOR_STATLABEL, TRUE, FALSE);
    PAL_DrawText(PAL_GetWord(STATUS_LABEL_DEXTERITY), PAL_XY(200, 124), ITEMUSEMENU_COLOR_STATLABEL, TRUE, FALSE);
    PAL_DrawText(PAL_GetWord(STATUS_LABEL_FLEERATE), PAL_XY(200, 142), ITEMUSEMENU_COLOR_STATLABEL, TRUE, FALSE);

    int role = PARTY_PLAYER(sSelectedPlayer);

    PAL_DrawNumber(PLAYER.rgwLevel[role], 4, PAL_XY(240, 20), kNumColorYellow, kNumAlignRight);

    PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_SLASH), gpScreen, PAL_XY(263, 38));
    PAL_DrawNumber(PLAYER.rgwMaxHP[role], 4, PAL_XY(261, 40), kNumColorBlue, kNumAlignRight);
    PAL_DrawNumber(PLAYER.rgwHP[role], 4, PAL_XY(240, 37), kNumColorYellow, kNumAlignRight);

    PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_SLASH), gpScreen, PAL_XY(263, 56));
    PAL_DrawNumber(PLAYER.rgwMaxMP[role], 4, PAL_XY(261, 58), kNumColorBlue, kNumAlignRight);
    PAL_DrawNumber(PLAYER.rgwMP[role], 4, PAL_XY(240, 55), kNumColorYellow, kNumAlignRight);

    PAL_DrawNumber(PAL_GetPlayerAttackStrength(role), 4, PAL_XY(240, 74), kNumColorYellow, kNumAlignRight);
    PAL_DrawNumber(PAL_GetPlayerMagicStrength(role), 4, PAL_XY(240, 92), kNumColorYellow, kNumAlignRight);
    PAL_DrawNumber(PAL_GetPlayerDefense(role), 4, PAL_XY(240, 110), kNumColorYellow, kNumAlignRight);
    PAL_DrawNumber(PAL_GetPlayerDexterity(role), 4, PAL_XY(240, 128), kNumColorYellow, kNumAlignRight);
    PAL_DrawNumber(PAL_GetPlayerFleeRate(role), 4, PAL_XY(240, 146), kNumColorYellow, kNumAlignRight);

    // Draw the names of the players in the party
    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
      PAL_DrawText(PAL_GetWord(PLAYER.rgwName[PARTY_PLAYER(i)]), PAL_XY(125, 16 + 20 * i), MENUITEM_COLOR, TRUE, FALSE);
    }

    PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_ITEMBOX), gpScreen, PAL_XY(120, 80));

    int amount = PAL_GetItemAmount(wItemToUse);
    if (amount > 0) {
      // Draw the picture of the item
      BYTE bufImage[2048];
      if (PAL_MKFReadChunk(bufImage, 2048, OBJECT[wItemToUse].item.wBitmap, FP_BALL) > 0) {
        PAL_RLEBlitToSurface(bufImage, gpScreen, PAL_XY(127, 88));
      }
      // Draw the amount and label of the item
      PAL_DrawText(PAL_GetWord(wItemToUse), PAL_XY(116, 143), STATUS_COLOR_EQUIPMENT, TRUE, FALSE);
      PAL_DrawNumber(amount, 2, PAL_XY(170, 133), kNumColorCyan, kNumAlignRight);
    }

    // Update the screen area
    SDL_Rect rect = {110, 2, 200, 180};
    VIDEO_UpdateScreen(&rect);

    // Wait for key
    PAL_ClearKeyState();

    DWORD dwColorChangeTime = 0;
    while (TRUE) {
      // See if we should change the highlight color
      if (SDL_TICKS_PASSED(PAL_GetTicks(), dwColorChangeTime)) {
        if ((WORD)bSelectedColor + 1 >= (WORD)MENUITEM_COLOR_SELECTED_FIRST + MENUITEM_COLOR_SELECTED_TOTALNUM)
          bSelectedColor = MENUITEM_COLOR_SELECTED_FIRST;
        else
          bSelectedColor++;

        dwColorChangeTime = PAL_GetTicks() + (600 / MENUITEM_COLOR_SELECTED_TOTALNUM);

        // Redraw the selected item.
        PAL_DrawText(PAL_GetWord(PLAYER.rgwName[PARTY_PLAYER(sSelectedPlayer)]), PAL_XY(125, 16 + 20 * sSelectedPlayer),
                     bSelectedColor, FALSE, TRUE);
      }

      PAL_ProcessEvent();

      if (g_InputState.dwKeyPress != 0)
        break;

      PAL_Delay(1);
    }

    // the item is used up, any key to exit
    if (amount <= 0) {
      return MENUITEM_VALUE_CANCELLED;
    }

    if (g_InputState.dwKeyPress & (kKeyUp | kKeyLeft)) {
      sSelectedPlayer--;
      if (sSelectedPlayer < 0)
        sSelectedPlayer = g.wMaxPartyMemberIndex;
    } else if (g_InputState.dwKeyPress & (kKeyDown | kKeyRight)) {
      sSelectedPlayer++;
      if (sSelectedPlayer > g.wMaxPartyMemberIndex)
        sSelectedPlayer = 0;
    } else if (g_InputState.dwKeyPress & kKeyMenu) {
      break;
    } else if (g_InputState.dwKeyPress & kKeySearch) {
      return PARTY_PLAYER(sSelectedPlayer);
    }
  }

  return MENUITEM_VALUE_CANCELLED;
}

static BOOL __buymenu_firsttime_render;

static VOID PAL_BuyMenu_OnItemChange(WORD wCurrentItem) {
  // Draw the item bakcground box
  if (__buymenu_firsttime_render)
    PAL_RLEBlitToSurfaceWithShadow(PAL_GetUISprite(SPRITENUM_ITEMBOX), gpScreen, PAL_XY(46, 14), TRUE);
  PAL_RLEBlitToSurface(PAL_GetUISprite(SPRITENUM_ITEMBOX), gpScreen, PAL_XY(40, 8));

  // Draw the picture of current selected item
  BYTE bufImage[2048];
  if (PAL_MKFReadChunk(bufImage, 2048, OBJECT[wCurrentItem].item.wBitmap, FP_BALL) > 0) {
    PAL_RLEBlitToSurface(bufImage, gpScreen, PAL_XY(48, 15));
  }

  // Draw the amount of this item in the inventory
  if (__buymenu_firsttime_render)
    PAL_CreateSingleLineBoxWithShadow(PAL_XY(20, 100), 5, FALSE, 6);
  else
    PAL_CreateSingleLineBoxWithShadow(PAL_XY(20, 100), 5, FALSE, 0);

  PAL_DrawText(PAL_GetWord(BUYMENU_LABEL_CURRENT), PAL_XY(30, 110), 0, FALSE, FALSE);
  PAL_DrawNumber(PAL_CountItem(wCurrentItem), 6, PAL_XY(69, 115), kNumColorYellow, kNumAlignRight);

  // Draw the cash amount
  if (__buymenu_firsttime_render)
    PAL_CreateSingleLineBoxWithShadow(PAL_XY(20, 141), 5, FALSE, 6);
  else
    PAL_CreateSingleLineBoxWithShadow(PAL_XY(20, 141), 5, FALSE, 0);
  PAL_DrawText(PAL_GetWord(CASH_LABEL), PAL_XY(30, 151), 0, FALSE, FALSE);
  PAL_DrawNumber(g.dwCash, 6, PAL_XY(69, 156), kNumColorYellow, kNumAlignRight);

  const SDL_Rect rect = {20, 8, 300, 175};
  VIDEO_UpdateScreen(&rect);

  __buymenu_firsttime_render = FALSE;

  // display description of item to buy // by scturtle
  if (g_InputState.dwKeyPress & kKeyDefend) {
    VIDEO_BackupScreen(gpScreen);
    PAL_RunDescScript(OBJECT[wCurrentItem].item.wScriptDesc, PAL_ITEM_DESC_BOTTOM);
    VIDEO_UpdateScreen(NULL);
    PAL_WaitForAnyKey(0);
    VIDEO_RestoreScreen(gpScreen);
    VIDEO_UpdateScreen(NULL);
  }
}

VOID PAL_BuyMenu(WORD wStoreNum) {
  int count;
  for (count = 0; count < MAX_STORE_ITEM; count++) {
    if (STORE[wStoreNum].rgwItems[count] == 0)
      break;
  }

  // create the menu items
  MENUITEM rgMenuItem[MAX_STORE_ITEM];
  for (int i = 0; i < count; i++) {
    rgMenuItem[i].wValue = STORE[wStoreNum].rgwItems[i];
    rgMenuItem[i].wNumWord = STORE[wStoreNum].rgwItems[i];
    rgMenuItem[i].fEnabled = TRUE;
    rgMenuItem[i].pos = PAL_XY(150, 21 + i * 18);
  }

  // Draw the box
  PAL_CreateBox(PAL_XY(122, 8), 8, 8, 1, FALSE);

  // Draw the number of prices
  for (int i = 0; i < count; i++) {
    WORD w = OBJECT[rgMenuItem[i].wNumWord].item.wPrice;
    PAL_DrawNumber(w, 6, PAL_XY(238, 26 + i * 18), kNumColorYellow, kNumAlignRight);
  }

  WORD wItem = 0;
  __buymenu_firsttime_render = TRUE;

  while (TRUE) {
    wItem = PAL_ReadMenu(PAL_BuyMenu_OnItemChange, rgMenuItem, count, wItem, MENUITEM_COLOR);
    if (wItem == MENUITEM_VALUE_CANCELLED)
      break;

    if (OBJECT[wItem].item.wPrice <= g.dwCash) {
      if (PAL_ConfirmMenu()) {
        // Player bought an item
        g.dwCash -= OBJECT[wItem].item.wPrice;
        PAL_AddItemToInventory(wItem, 1);
      }
    }

    // Place the cursor to the current item on next loop
    for (int i = 0; i < count; i++)
      if (wItem == rgMenuItem[i].wValue)
        wItem = i;
  }
}

static VOID PAL_SellMenu_OnItemChange(WORD wCurrentItem) {
  // Draw the cash amount
  PAL_CreateSingleLineBoxWithShadow(PAL_XY(100, 150), 5, FALSE, 0);
  PAL_DrawText(PAL_GetWord(CASH_LABEL), PAL_XY(110, 160), 0, FALSE, FALSE);
  PAL_DrawNumber(g.dwCash, 6, PAL_XY(148, 165), kNumColorYellow, kNumAlignRight);

  // Draw the price
  PAL_CreateSingleLineBoxWithShadow(PAL_XY(224, 150), 5, FALSE, 0);
  if (OBJECT[wCurrentItem].item.wFlags & kItemFlagSellable) {
    PAL_DrawText(PAL_GetWord(SELLMENU_LABEL_PRICE), PAL_XY(234, 160), 0, FALSE, FALSE);
    PAL_DrawNumber(OBJECT[wCurrentItem].item.wPrice / 2, 6, PAL_XY(272, 165), kNumColorYellow, kNumAlignRight);
  }
}

VOID PAL_SellMenu(VOID) {
  while (TRUE) {
    WORD w = PAL_ItemSelectMenu(PAL_SellMenu_OnItemChange, kItemFlagSellable);
    if (w == 0)
      break;
    if (PAL_ConfirmMenu()) {
      if (PAL_AddItemToInventory(w, -1)) {
        g.dwCash += OBJECT[w].item.wPrice / 2;
      }
    }
  }
}

static LPCWSTR PAL_GetPlayerName(int playerIndex) {
  WORD w = PARTY_PLAYER(playerIndex);
  return PAL_GetWord(PLAYER.rgwName[w]);
}

static bool PAL_CanEquip(WORD wItem, int playerIndex) {
  WORD w = PARTY_PLAYER(playerIndex);
  return OBJECT[wItem].item.wFlags & (kItemFlagEquipableByPlayerRole_First << w);
}

VOID PAL_EquipItemMenu(WORD wItem) {
  gLastUnequippedItem = wItem;

  BYTE bufBackground[320 * 200];
  PAL_MKFDecompressChunk(bufBackground, 320 * 200, 1, FP_FBP);

  BYTE bColor, bSelectedColor;
  DWORD dwColorChangeTime;
  bSelectedColor = MENUITEM_COLOR_SELECTED_FIRST;
  dwColorChangeTime = PAL_GetTicks() + (600 / MENUITEM_COLOR_SELECTED_TOTALNUM);

  int iCurrentPlayer = 0;
  while (TRUE) {
    wItem = gLastUnequippedItem;

    // Draw the background
    VIDEO_FBPBlitToSurface(bufBackground, gpScreen);

    // Draw the item picture
    BYTE bufImage[2048];
    if (PAL_MKFReadChunk(bufImage, 2048, OBJECT[wItem].item.wBitmap, FP_BALL) > 0) {
      PAL_RLEBlitToSurface(bufImage, gpScreen, PAL_XY_OFFSET(PAL_XY(8, 8), 8, 8));
    }

    // Draw the current equipment of the selected player
    PAL_POS equipNames[MAX_PLAYER_EQUIPMENTS] = {PAL_XY(130, 11), PAL_XY(130, 33), PAL_XY(130, 55),
                                                 PAL_XY(130, 77), PAL_XY(130, 99), PAL_XY(130, 121)};
    WORD w = PARTY_PLAYER(iCurrentPlayer);
    for (int i = 0; i < MAX_PLAYER_EQUIPMENTS; i++) {
      if (PLAYER.rgwEquipment[i][w] != 0) {
        PAL_DrawText(PAL_GetWord(PLAYER.rgwEquipment[i][w]), equipNames[i], MENUITEM_COLOR, TRUE, FALSE);
      }
    }

    // Draw the stats of the currently selected player
    PAL_DrawNumber(PAL_GetPlayerAttackStrength(w), 4, PAL_XY(260, 14), kNumColorCyan, kNumAlignRight);
    PAL_DrawNumber(PAL_GetPlayerMagicStrength(w), 4, PAL_XY(260, 36), kNumColorCyan, kNumAlignRight);
    PAL_DrawNumber(PAL_GetPlayerDefense(w), 4, PAL_XY(260, 58), kNumColorCyan, kNumAlignRight);
    PAL_DrawNumber(PAL_GetPlayerDexterity(w), 4, PAL_XY(260, 80), kNumColorCyan, kNumAlignRight);
    PAL_DrawNumber(PAL_GetPlayerFleeRate(w), 4, PAL_XY(260, 102), kNumColorCyan, kNumAlignRight);

    // Draw a box for player selection
    PAL_POS equipRoleListBox = PAL_XY(2, 95);
    PAL_CreateBox(equipRoleListBox, g.wMaxPartyMemberIndex, 2, 0, FALSE);

    // Draw the label of players
    for (int i = 0; i <= g.wMaxPartyMemberIndex; i++) {
      bool canEquip = PAL_CanEquip(wItem, i);
      if (iCurrentPlayer == i) {
        bColor = canEquip ? bSelectedColor : MENUITEM_COLOR_SELECTED_INACTIVE;
      } else {
        bColor = canEquip ? MENUITEM_COLOR : MENUITEM_COLOR_INACTIVE;
      }
      PAL_DrawText(PAL_GetPlayerName(i), PAL_XY_OFFSET(equipRoleListBox, 13, 13 + 18 * i), bColor, TRUE, FALSE);
    }

    // Draw the text label and amount of the item
    if (wItem != 0) {
      PAL_DrawText(PAL_GetWord(wItem), PAL_XY(5, 70), MENUITEM_COLOR_CONFIRMED, TRUE, FALSE);
      PAL_DrawNumber(PAL_GetItemAmount(wItem), 2, PAL_XY(51, 57), kNumColorCyan, kNumAlignRight);
    }

    // Update the screen
    VIDEO_UpdateScreen(NULL);

    // Accept input
    PAL_ClearKeyState();
    while (TRUE) {
      PAL_ProcessEvent();
      // See if we should change the highlight color
      if (SDL_TICKS_PASSED(PAL_GetTicks(), dwColorChangeTime)) {
        if ((WORD)bSelectedColor + 1 >= (WORD)MENUITEM_COLOR_SELECTED_FIRST + MENUITEM_COLOR_SELECTED_TOTALNUM)
          bSelectedColor = MENUITEM_COLOR_SELECTED_FIRST;
        else
          bSelectedColor++;

        dwColorChangeTime = PAL_GetTicks() + (600 / MENUITEM_COLOR_SELECTED_TOTALNUM);

        // Redraw the selected item if needed.
        if (PAL_CanEquip(wItem, iCurrentPlayer)) {
          PAL_DrawText(PAL_GetPlayerName(iCurrentPlayer), PAL_XY_OFFSET(equipRoleListBox, 13, 13 + 18 * iCurrentPlayer),
                       bSelectedColor, TRUE, TRUE);
        }
      }

      if (g_InputState.dwKeyPress != 0)
        break;

      PAL_Delay(1);
    }

    if (wItem == 0)
      return;

    if (g_InputState.dwKeyPress & (kKeyUp | kKeyLeft)) {
      iCurrentPlayer--;
      if (iCurrentPlayer < 0)
        iCurrentPlayer = g.wMaxPartyMemberIndex;
    } else if (g_InputState.dwKeyPress & (kKeyDown | kKeyRight)) {
      iCurrentPlayer++;
      if (iCurrentPlayer > g.wMaxPartyMemberIndex)
        iCurrentPlayer = 0;
    } else if (g_InputState.dwKeyPress & kKeyMenu) {
      return;
    } else if (g_InputState.dwKeyPress & kKeySearch) {
      if (PAL_CanEquip(wItem, iCurrentPlayer)) {
        // Run the equip script
        OBJECT[wItem].item.wScriptOnEquip =
            PAL_RunTriggerScript(OBJECT[wItem].item.wScriptOnEquip, PARTY_PLAYER(iCurrentPlayer));
      }
    }
  }
}
