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

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* =========================================================================
   1. 终端与协议初始化 (替代 SDL Video/Event 初始化部分)
   ========================================================================= */

static struct termios orig_termios;

static void PAL_RestoreTerminal(void) {
  printf("\033[2J");
  // 退出 KKP 并恢复光标显示
  printf("\033[<u\033[?25h");
  fflush(stdout);
  tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
}

static void PAL_InitTerminal(void) {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(PAL_RestoreTerminal);

  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON | ISIG); // 禁用回显和行缓冲
  tcsetattr(STDIN_FILENO, TCSANOW, &raw);

  // 隐藏光标，开启 KKP (Level 11 包含按下、抬起、重复等所有键盘事件)
  printf("\033[?25l\033[>11u");
  fflush(stdout);
}

/* =========================================================================
   2. 键位映射定义 (移除 SDL 依赖)
   ========================================================================= */

// 定义终端环境下的特殊按键虚拟码
#define KKP_UP 1000
#define KKP_DOWN 1001
#define KKP_RIGHT 1002
#define KKP_LEFT 1003
#define KKP_PGUP 57358 // KKP 标准 PageUp 码
#define KKP_PGDN 57359 // KKP 标准 PageDown 码
#define KKP_HOME 57362 // KKP 标准 Home 码
#define KKP_END 57363  // KKP 标准 End 码

static const int g_KeyMap[][2] = {
    {KKP_UP, kKeyUp},     {KKP_DOWN, kKeyDown}, {KKP_LEFT, kKeyLeft}, {KKP_RIGHT, kKeyRight}, {27, kKeyMenu},
    {13, kKeySearch},     {' ', kKeySearch},    {KKP_PGUP, kKeyPgUp}, {KKP_PGDN, kKeyPgDn},   {KKP_HOME, kKeyHome},
    {KKP_END, kKeyEnd},   {'r', kKeyRepeat},    {'a', kKeyAuto},      {'d', kKeyDefend},      {'e', kKeyUseItem},
    {'w', kKeyThrowItem}, {'q', kKeyFlee},      {'f', kKeyForce},     {'s', kKeyStatus}};

/* =========================================================================
   3. 原有的 PAL 状态与方向逻辑 (保持不变)
   ========================================================================= */

volatile PALINPUTSTATE g_InputState;

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

/* =========================================================================
   4. KKP 事件过滤与轮询 (替代 SDL_PollEvent 和 SDL_EventFilter)
   ========================================================================= */

// 触发按键映射
static void PAL_TriggerKey(int keyCode, int isDown) {
  // 将大写字母统一转换为小写进行匹配
  if (keyCode >= 'A' && keyCode <= 'Z') {
    keyCode += 32;
  }

  for (int i = 0; i < sizeof(g_KeyMap) / sizeof(g_KeyMap[0]); i++) {
    if (g_KeyMap[i][0] == keyCode) {
      if (isDown)
        PAL_KeyDown(g_KeyMap[i][1]);
      else
        PAL_KeyUp(g_KeyMap[i][1]);
      break;
    }
  }
}

VOID PAL_ClearKeyState(VOID) { g_InputState.dwKeyPress = 0; }

VOID PAL_InitInput(VOID) {
  g_InputState.dir = kDirUnknown;
  PAL_InitTerminal();
}

VOID PAL_ProcessEvent(VOID) {
  // 使用 poll 实现非阻塞读取
  struct pollfd pfd = {STDIN_FILENO, POLLIN, 0};

  while (poll(&pfd, 1, 0) > 0) {
    unsigned char buf[256];
    int n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (n <= 0)
      break;
    buf[n] = '\0';

    int i = 0;
    while (i < n) {
      // 匹配转义序列 \033[...
      if (buf[i] == 27 && (i + 1 < n) && buf[i + 1] == '[') {
        int j = i + 2;
        // 寻找序列终止符
        while (j < n && !((buf[j] >= 'A' && buf[j] <= 'Z') || (buf[j] >= 'a' && buf[j] <= 'z') || buf[j] == '~')) {
          j++;
        }

        if (j < n) {
          char cmd = buf[j];
          int code = 0, mods = 1, event = 1; // event=1 默认为 Press

          char seq[64] = {0};
          int len = j - (i + 2);
          if (len > 0 && len < sizeof(seq)) {
            memcpy(seq, &buf[i + 2], len);
          }

          // 尝试解析 CSI 序列参数 (格式: code;mods:event)
          if (sscanf(seq, "%d;%d:%d", &code, &mods, &event) < 3)
            if (sscanf(seq, "%d;%d", &code, &mods) < 2)
              sscanf(seq, "%d", &code);

          if ((code == 'c' || code == 'C') && mods == 5) { // ctrl+c
            PAL_RestoreTerminal();
            exit(0);
          }

          // 处理 KKP 的基础字母、功能键
          if (cmd == 'u') {
            // 啥都不用改，code 已经提取出来了
          }
          // 处理箭头方向键
          else if (cmd >= 'A' && cmd <= 'D') {
            if (cmd == 'A')
              code = KKP_UP;
            else if (cmd == 'B')
              code = KKP_DOWN;
            else if (cmd == 'C')
              code = KKP_RIGHT;
            else if (cmd == 'D')
              code = KKP_LEFT;
          }

          // event: 1=PRESS, 2=REPEAT, 3=RELEASE
          if (event == 1 || event == 2) {
            PAL_TriggerKey(code, 1);
          } else if (event == 3) {
            PAL_TriggerKey(code, 0);
          }

          i = j + 1; // 跳到下一个序列
        } else {
          break; // 不完整的序列
        }
      }
      // 处理 Legacy ESC 按下
      else if (buf[i] == 27) {
        PAL_TriggerKey(27, 1);
        PAL_TriggerKey(27, 0); // 立刻模拟抬起
        i++;
      }
      // 处理 Legacy ASCII 输入 (非 KKP 回退模式)
      else {
        if (buf[i] == 3) { // ctrl+c
          PAL_RestoreTerminal();
          exit(0);
        }
        PAL_TriggerKey(buf[i], 1);
        PAL_TriggerKey(buf[i], 0); // 立刻模拟抬起
        i++;
      }
    }
  }
}
