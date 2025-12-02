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
// Portions based on PALx Project by palxex.
// Copyright (c) 2006-2008, Pal Lockheart <palxex@gmail.com>.
//

#include "main.h"
#include <errno.h>
#include <iconv.h>
#include <locale.h>
#include <wctype.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define FONT_COLOR_DEFAULT 0x4F
#define FONT_COLOR_YELLOW 0x2D
#define FONT_COLOR_RED 0x1A
#define FONT_COLOR_CYAN 0x8D
#define FONT_COLOR_CYAN_ALT 0x8C
#define FONT_COLOR_RED_ALT 0x17

#define WORD_LENGTH 10

BOOL g_fUpdatedInBattle = FALSE;

static iconv_t cd;

static char g_word_data[1 << 16];
static LPWSTR g_word_ptr[600];
static DWORD g_msg_offset[13600];
static int g_n_words;
static int g_n_msgs;
static BYTE g_bufDialogIcons[282];

typedef struct {
  int current_line;
  uint8_t current_font_color;
  PAL_POS pos_icon;
  PAL_POS pos_dialog_title;
  PAL_POS pos_dialog_text;
  uint8_t dialog_position;
  uint8_t icon_id;
  int delay_time;
  int dialog_shadow;
  bool user_skip;
  bool playing_rng;
  bool updated_in_battle;
} DialogState;

static DialogState s = {
    .pos_dialog_title = PAL_XY(12, 8),
    .pos_dialog_text = PAL_XY(44, 26),
    .current_font_color = FONT_COLOR_DEFAULT,
    .dialog_position = kDialogUpper,
    .user_skip = FALSE,
    .playing_rng = FALSE,
};

static FT_Library g_ftLibrary = NULL;
static FT_Face g_ftFace = NULL;

const char *simSunFontPath = "simsun.ttf";
const char *unifontPath = "unifont.bdf";
const int _font_height = 16;

BOOL PAL_InitFont() {
  FT_Error error;
  error = FT_Init_FreeType(&g_ftLibrary);
  if (error)
    TerminateOnError("PAL_InitFont: Could not init FreeType library\n");
  error = FT_New_Face(g_ftLibrary, simSunFontPath, 0, &g_ftFace);
  if (error)
    error = FT_New_Face(g_ftLibrary, unifontPath, 0, &g_ftFace);
  if (error)
    TerminateOnError("could not open font simsun.ttf or unifont.bdf\n");
  FT_Set_Pixel_Sizes(g_ftFace, _font_height, _font_height);
  FT_Select_Charmap(g_ftFace, FT_ENCODING_UNICODE);
  return TRUE;
}

int PAL_CharWidth(uint16_t wChar) {
  if (g_ftFace && FT_Load_Char(g_ftFace, wChar, FT_LOAD_NO_BITMAP) == 0)
    return g_ftFace->glyph->advance.x >> 6;
  return (wChar < 0x80) ? 8 : 16;
}

INT PAL_TextWidth(LPCWSTR text) {
  size_t w = 0;
  for (int j = 0, l = wcslen(text); j < l; j++)
    w += PAL_CharWidth(text[j]);
  return w;
}

INT PAL_WordWidth(INT nWordIndex) { return (PAL_TextWidth(PAL_GetWord(nWordIndex)) + 8) >> 4; }

void PAL_DrawCharOnSurface(uint16_t wChar, SDL_Surface *lpSurface, PAL_POS pos, uint8_t bColor) {
  if (!lpSurface || !g_ftFace)
    return;

  if (FT_Load_Char(g_ftFace, wChar, FT_LOAD_RENDER | FT_LOAD_MONOCHROME))
    return;

  FT_GlyphSlot slot = g_ftFace->glyph;
  FT_Bitmap *bm = &slot->bitmap;

  int ascender = g_ftFace->size->metrics.ascender >> 6;
  int start_y = PAL_Y(pos) + ascender - slot->bitmap_top;
  int start_x = PAL_X(pos) + slot->bitmap_left;

  uint8_t *dst_pixels = (uint8_t *)lpSurface->pixels;
  int dst_pitch = lpSurface->pitch;
  int dst_w = lpSurface->w;
  int dst_h = lpSurface->h;

  for (int r = 0; r < bm->rows; r++) {
    int ty = start_y + r;
    if (ty < 0 || ty >= dst_h)
      continue;
    const uint8_t *src_row = bm->buffer + r * bm->pitch;
    uint8_t *dst_row = dst_pixels + ty * dst_pitch;
    for (int c = 0; c < bm->width; c++) {
      int tx = start_x + c;
      if (tx < 0 || tx >= dst_w)
        continue;
      if (src_row[c >> 3] & (0x80 >> (c & 7))) {
        dst_row[tx] = bColor;
      }
    }
  }
}

INT PAL_MultiByteToWideChar(LPCSTR mbs, int mbslength, LPWSTR wcs, int wcslength);

INT PAL_InitText(VOID) {
  LPBYTE temp;

  // setup iconv
  setlocale(LC_CTYPE, "UTF-8");
  cd = iconv_open("WCHAR_T", "GBK");

  // read words from word.dat
  FILE *fpWord = UTIL_OpenRequiredFile("word.dat");
  fseek(fpWord, 0, SEEK_END);
  int word_bs = ftell(fpWord);
  g_n_words = (word_bs + (WORD_LENGTH - 1)) / WORD_LENGTH;
  temp = (LPBYTE)UTIL_malloc(WORD_LENGTH * g_n_words);
  fseek(fpWord, 0, SEEK_SET);
  fread(temp, 1, word_bs, fpWord);
  fclose(fpWord);

  for (int i = 0; i < g_n_words; i++) {
    int base = i * WORD_LENGTH;
    int pos = base + WORD_LENGTH - 1;
    while (pos >= base && (temp[pos] == ' ' || temp[pos] == '1'))
      temp[pos--] = 0;
  }

  // convert from gbk to unicode
  for (int i = 0, wpos = 0; i < g_n_words; i++) {
    g_word_ptr[i] = (LPWSTR)g_word_data + wpos;
    int l =
        PAL_MultiByteToWideChar((LPCSTR)temp + i * WORD_LENGTH, WORD_LENGTH, g_word_ptr[i], sizeof(g_word_data) - wpos);
    wpos += l + 1;
  }
  free(temp);

  // read the message offsets from sss.mkf
  int n_offsets = PAL_MKFGetChunkSize(3, FP_SSS) / sizeof(DWORD);
  g_n_msgs = n_offsets - 1;
  PAL_MKFReadChunk((LPBYTE)(g_msg_offset), n_offsets * sizeof(DWORD), 3, FP_SSS);

  // read dialog icons from data.mkf
  PAL_MKFReadChunk(g_bufDialogIcons, 282, 12, FP_DATA);
  return 0;
}

LPCWSTR
PAL_GetWord(int iNumWord) { return iNumWord >= g_n_words ? L"" : g_word_ptr[iNumWord]; }

LPCWSTR
PAL_GetMsg(int iNumMsg) {
  if (iNumMsg >= g_n_msgs)
    return L"";

  int offset = g_msg_offset[iNumMsg];
  int length = g_msg_offset[iNumMsg + 1] - g_msg_offset[iNumMsg];

  CHAR bef[1024];
  FILE *fpMsg = UTIL_OpenRequiredFile("m.msg");
  fseek(fpMsg, offset, SEEK_SET);
  fread(bef, 1, length, fpMsg);

  static WCHAR cvt[1024];
  int l = PAL_MultiByteToWideChar(bef, length, cvt, sizeof(cvt) / sizeof(WCHAR));
  cvt[l] = 0;
  return cvt;
}

VOID PAL_DrawText(LPCWSTR lpszText, PAL_POS pos, BYTE bColor, BOOL fShadow, BOOL fUpdate) {
  int x, y;
  SDL_Rect urect;

  urect.x = x = PAL_X(pos);
  urect.y = y = PAL_Y(pos);
  urect.h = _font_height + (fShadow ? 1 : 0);
  urect.w = 0;

  // Draw the character
  while (*lpszText) {
    WCHAR wch = *lpszText;
    if (fShadow) {
      PAL_DrawCharOnSurface(wch, gpScreen, PAL_XY(x + 1, y), 0);
      PAL_DrawCharOnSurface(wch, gpScreen, PAL_XY(x, y + 1), 0);
      PAL_DrawCharOnSurface(wch, gpScreen, PAL_XY(x + 1, y + 1), 0);
    }
    PAL_DrawCharOnSurface(wch, gpScreen, PAL_XY(x, y), bColor);
    int char_width = PAL_CharWidth(wch);
    x += char_width;
    urect.w += char_width;
    lpszText++;
  }

  // Update the screen area
  if (fUpdate && urect.w > 0) {
    if (fShadow)
      urect.w++, urect.h++;
    if (urect.x + urect.w > 320)
      urect.w = 320 - urect.x;
    if (urect.y + urect.h > 200)
      urect.h = 200 - urect.y;
    VIDEO_UpdateScreen(&urect);
  }
}

VOID PAL_StartDialog(BYTE bDialogLocation, BYTE bFontColor, INT iNumCharFace, BOOL fPlayingRNG) {
  PAL_StartDialogWithOffset(bDialogLocation, bFontColor, iNumCharFace, fPlayingRNG, 0, 0);
}

VOID PAL_StartDialogWithOffset(BYTE bDialogLocation, BYTE bFontColor, INT iNumCharFace, BOOL fPlayingRNG, INT xOff,
                               INT yOff) {
  if (gInBattle && !g_fUpdatedInBattle) {
    // Update the screen in battle, or the graphics may seem messed up
    VIDEO_UpdateScreen(NULL);
    g_fUpdatedInBattle = TRUE;
  }

  s.icon_id = 0;
  s.pos_icon = PAL_XY(0, 0);
  s.current_line = 0;
  s.pos_dialog_title = PAL_XY(12, 8);
  s.user_skip = FALSE;
  s.current_font_color = bFontColor ? bFontColor : FONT_COLOR_DEFAULT;
  s.delay_time = 3;

  if (fPlayingRNG && iNumCharFace) {
    VIDEO_BackupScreen(gpScreen);
    s.playing_rng = TRUE;
  }

  if (iNumCharFace) {
    // Display the character face at the upper/lower part of the screen
    BYTE buf[PAL_RLEBUFSIZE];
    SDL_Rect rect;
    if (PAL_MKFReadChunk(buf, PAL_RLEBUFSIZE, iNumCharFace, FP_RGM) > 0) {
      rect.w = PAL_RLEGetWidth((LPCBITMAPRLE)buf);
      rect.h = PAL_RLEGetHeight((LPCBITMAPRLE)buf);
      rect.x = (bDialogLocation == kDialogUpper ? 48 : 270) - rect.w / 2 + xOff;
      rect.y = (bDialogLocation == kDialogUpper ? 55 : 144) - rect.h / 2 + yOff;
      PAL_RLEBlitToSurface((LPCBITMAPRLE)buf, gpScreen, PAL_XY(rect.x, rect.y));
      VIDEO_UpdateScreen(&rect);
    }
  }

  switch (bDialogLocation) {
  case kDialogUpper:
    s.pos_dialog_title = PAL_XY(iNumCharFace ? 80 : 12, 8);
    s.pos_dialog_text = PAL_XY(iNumCharFace ? 96 : 44, 26);
    break;
  case kDialogCenter:
    s.pos_dialog_text = PAL_XY(80, 40);
    break;
  case kDialogLower:
    s.pos_dialog_title = PAL_XY(iNumCharFace > 0 ? 4 : 12, 108);
    s.pos_dialog_text = PAL_XY(iNumCharFace > 0 ? 20 : 44, 126);
    break;
  case kDialogCenterWindow:
    s.pos_dialog_text = PAL_XY(160, 40);
    break;
  }

  s.pos_dialog_title = PAL_XY_OFFSET(s.pos_dialog_title, xOff, yOff);
  s.pos_dialog_text = PAL_XY_OFFSET(s.pos_dialog_text, xOff, yOff);
  s.dialog_position = bDialogLocation;
}

// Wait for player to press a key after showing a dialog.
static VOID PAL_DialogWaitForKeyWithMaximumSeconds(FLOAT fMaxSeconds) {

  // get the current palette
  SDL_Color palette[256];
  SDL_Color *pCurrentPalette = PAL_GetPalette(gCurPalette, gNightPalette);
  memcpy(palette, pCurrentPalette, sizeof(palette));

  if (s.dialog_position == kDialogUpper || s.dialog_position == kDialogLower) {
    // show the icon
    LPCBITMAPRLE p = PAL_SpriteGetFrame(g_bufDialogIcons, s.icon_id);
    if (p) {
      SDL_Rect rect;
      rect.x = PAL_X(s.pos_icon);
      rect.y = PAL_Y(s.pos_icon);
      rect.w = 16;
      rect.h = 16;
      PAL_RLEBlitToSurface(p, gpScreen, s.pos_icon);
      VIDEO_UpdateScreen(&rect);
    }
  }

  PAL_ClearKeyState();

  uint32_t dwBeginningTicks = PAL_GetTicks();
  while (TRUE) {
    UTIL_Delay(100);

    if (s.dialog_position == kDialogUpper || s.dialog_position == kDialogLower) {
      // palette shift
      SDL_Color t = palette[0xF9];
      for (int i = 0xF9; i < 0xFE; i++)
        palette[i] = palette[i + 1];
      palette[0xFE] = t;
      VIDEO_SetPalette(palette);
    }

    if (fabs(fMaxSeconds) > FLT_EPSILON && PAL_GetTicks() - dwBeginningTicks > 1000 * fMaxSeconds)
      break;

    if (g_InputState.dwKeyPress != 0)
      break;
  }

  if (s.dialog_position == kDialogUpper || s.dialog_position == kDialogLower)
    PAL_SetPalette(gCurPalette, gNightPalette);

  PAL_ClearKeyState();

  s.user_skip = FALSE;
}

static VOID PAL_DialogWaitForKey(VOID) { PAL_DialogWaitForKeyWithMaximumSeconds(0); }

int TEXT_DisplayText(LPCWSTR lpszText, int x, int y, BOOL isDialog) {
  WCHAR text[2];
  BYTE color, isNumber = 0;

  while (lpszText != NULL && *lpszText != '\0') {
    switch (*lpszText) {
    case '-':
      s.current_font_color = s.current_font_color == FONT_COLOR_CYAN ? FONT_COLOR_DEFAULT : FONT_COLOR_CYAN;
      lpszText++;
      break;
    case '\'':
      s.current_font_color = s.current_font_color == FONT_COLOR_RED ? FONT_COLOR_DEFAULT : FONT_COLOR_RED;
      lpszText++;
      break;
    case '@':
      s.current_font_color = s.current_font_color == FONT_COLOR_RED_ALT ? FONT_COLOR_DEFAULT : FONT_COLOR_RED_ALT;
      lpszText++;
      break;
    case '\"':
      if (!isDialog)
        s.current_font_color = s.current_font_color == FONT_COLOR_YELLOW ? FONT_COLOR_DEFAULT : FONT_COLOR_YELLOW;
      lpszText++;
      break;

    case '$':
      // Set the delay time of text-displaying
      s.delay_time = wcstol(lpszText + 1, NULL, 10) * 10 / 7;
      lpszText += 3;
      break;

    case '~':
      // Delay for a period and quit
      if (s.user_skip)
        VIDEO_UpdateScreen(NULL);
      if (!isDialog)
        UTIL_Delay(wcstol(lpszText + 1, NULL, 10) * 80 / 7);
      s.current_line = -1;
      s.user_skip = FALSE;
      return x; // don't go further

    case ')':
      // 爱心表情
      s.icon_id = 1;
      lpszText++;
      break;

    case '(':
      // 汗滴表情
      s.icon_id = 2;
      lpszText++;
      break;

    default:
      text[0] = *lpszText++;
      text[1] = 0;

      color = s.current_font_color;
      if (isDialog) {
        if (s.current_font_color == FONT_COLOR_DEFAULT)
          color = 0;
        isNumber = text[0] >= '0' && text[0] <= '9';
      }

      // Update the screen on each draw operation is time-consuming, so disable it if user want to skip
      if (isNumber)
        PAL_DrawNumber(text[0] - '0', 1, PAL_XY(x, y + 4), kNumColorYellow, kNumAlignLeft);
      else
        PAL_DrawText(text, PAL_XY(x, y), color, /*shadow=*/!isDialog, /*update=*/!isDialog && !s.user_skip);

      x += PAL_CharWidth(text[0]);

      if (!isDialog && !s.user_skip) {
        PAL_ClearKeyState();
        UTIL_Delay(s.delay_time * 8);

        // User pressed a key to skip the dialog
        if (g_InputState.dwKeyPress & (kKeySearch | kKeyMenu))
          s.user_skip = TRUE;
      }
    }
  }
  return x;
}

// Show one line of the dialog text.
VOID PAL_ShowDialogText(LPCWSTR lpszText) {
  PAL_ClearKeyState();
  s.icon_id = 0;

  if (gInBattle && !g_fUpdatedInBattle) {
    // Update the screen in battle, or the graphics may seem messed up
    VIDEO_UpdateScreen(NULL);
    g_fUpdatedInBattle = TRUE;
  }

  if (s.current_line > 3) {
    // The rest dialogs should be shown in the next page.
    PAL_DialogWaitForKey();
    s.current_line = 0;
    VIDEO_RestoreScreen(gpScreen);
    VIDEO_UpdateScreen(NULL);
  }

  if (s.dialog_position == kDialogCenterWindow) {
    // The text should be shown in a small window at the center of the screen
    int w = wcslen(lpszText), len = 0;
    for (int i = 0; i < w; i++)
      len += PAL_CharWidth(lpszText[i]) >> 3;

    PAL_POS pos = PAL_XY_OFFSET(s.pos_dialog_text, -len * 4, 0);
    LPBOX lpBox = PAL_CreateSingleLineBoxWithShadow(pos, (len + 1) / 2, FALSE, s.dialog_shadow);

    SDL_Rect rect;
    rect.x = PAL_X(pos);
    rect.y = PAL_Y(pos);
    rect.w = 320 - rect.x * 2 + 32;
    rect.h = 64;
    VIDEO_UpdateScreen(&rect);

    // Show the text on the screen
    TEXT_DisplayText(lpszText, PAL_X(pos) + 8 + ((len & 1) << 2), PAL_Y(pos) + 10, TRUE);
    VIDEO_UpdateScreen(&rect);

    PAL_DialogWaitForKeyWithMaximumSeconds(1.4);

    PAL_DeleteBox(lpBox);
    VIDEO_UpdateScreen(&rect);

    PAL_EndDialog();

  } else {
    size_t len = wcslen(lpszText);
    if (s.current_line == 0 && s.dialog_position != kDialogCenter &&
        (lpszText[len - 1] == 0xff1a || // '：'
         lpszText[len - 1] == ':')) {
      // name of character
      PAL_DrawText(lpszText, s.pos_dialog_title, FONT_COLOR_CYAN_ALT, TRUE, TRUE);
    } else {
      // Save the screen before we show the first line of dialog
      if (!s.playing_rng && s.current_line == 0)
        VIDEO_BackupScreen(gpScreen);

      int x = PAL_X(s.pos_dialog_text);
      int y = PAL_Y(s.pos_dialog_text) + s.current_line * 18;
      x = TEXT_DisplayText(lpszText, x, y, FALSE);

      // and update the full screen at once after all texts are drawn
      if (s.user_skip)
        VIDEO_UpdateScreen(NULL);

      s.pos_icon = PAL_XY(x, y);
      s.current_line++;
    }
  }
}

VOID PAL_ClearDialog(BOOL fWaitForKey) {
  if (s.current_line > 0 && fWaitForKey)
    PAL_DialogWaitForKey();
  s.current_line = 0;
}

VOID PAL_EndDialog(VOID) {
  PAL_ClearDialog(TRUE);

  // Set some default parameters, as there are some parts of script
  // which doesn't have a "start dialog" instruction before showing the dialog.
  s.pos_dialog_title = PAL_XY(12, 8);
  s.pos_dialog_text = PAL_XY(44, 26);
  s.current_font_color = FONT_COLOR_DEFAULT;
  s.dialog_position = kDialogUpper;
  s.user_skip = FALSE;
  s.playing_rng = FALSE;
}

VOID PAL_SetDialogShadow(int shadow) { s.dialog_shadow = shadow; }

BOOL PAL_IsInDialog(VOID) { return (s.current_line != 0); }

BOOL PAL_DialogIsPlayingRNG(VOID) { return s.playing_rng; }

INT PAL_MultiByteToWideChar(LPCSTR mbs, int mbslength, LPWSTR wcs, int wcslength)
/*++
  Purpose:

    Convert multi-byte string into the corresponding unicode string.

  Parameters:

    [IN]  mbs - Pointer to the multi-byte string.
    [IN]  mbslength - Length of the multi-byte string, or -1 for auto-detect.
    [IN]  wcs - Pointer to the wide string buffer.
    [IN]  wcslength - Length of the wide string buffer.

  Return value:

    The length of converted wide string. If mbslength is set to -1, the returned
    value includes the terminal null-char; otherwise, the null-char is not included.
--*/
{
  if (cd == (iconv_t)-1)
    return 0;
  char *in_ptr = (char *)mbs;
  char *out_ptr_start = (char *)wcs;
  char *out_ptr = out_ptr_start;
  size_t out_bytes_max = wcslength * sizeof(*wcs);
  size_t out_bytes_left = out_bytes_max;
  size_t in_bytes = (ssize_t)mbslength == -1 ? strlen(mbs) + 1 : mbslength;
  for (size_t in_bytes_left = in_bytes; in_bytes_left; in_bytes_left--) {
    size_t res = iconv(cd, &in_ptr, &in_bytes_left, &out_ptr, &out_bytes_left);
    if (res != (size_t)-1)
      break;
  }
  return (out_bytes_max - out_bytes_left) / sizeof(*wcs);
}

INT PAL_swprintf(LPWSTR buffer, size_t count, LPCWSTR format, ...) {
  int ret;
  va_list ap;
  va_start(ap, format);
  ret = vswprintf(buffer, count, format, ap);
  va_end(ap);
  return ret;
}
