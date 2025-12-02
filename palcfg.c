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
// palcfg.c: Configuration definition.
//  @Author: Lou Yihua <louyihua@21cn.com>, 2016.
//

#include "palcfg.h"
#include "global.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

CONFIGURATION gConfig;

// 去除字符串首尾空格
static char *trim_whitespace(char *str) {
  char *end;
  while (isspace((unsigned char)*str))
    str++;
  if (*str == 0)
    return str;

  end = str + strlen(str) - 1;
  while (end > str && isspace((unsigned char)*end))
    end--;
  end[1] = '\0';
  return str;
}

// 解析一行配置
static void parse_line(char *line) {
  char *key, *value;
  char *separator = strchr(line, '=');

  // 如果没有等号或是注释行，忽略
  if (!separator || line[0] == '#')
    return;

  *separator = '\0'; // 切割字符串
  key = trim_whitespace(line);
  value = trim_whitespace(separator + 1);

  if (strlen(key) == 0 || strlen(value) == 0)
    return;

  // 字符串匹配与赋值
  if (strcasecmp(key, "GamePath") == 0) {
    if (gConfig.pszGamePath)
      free(gConfig.pszGamePath);
    gConfig.pszGamePath = strdup(value);
  } else if (strcasecmp(key, "FullScreen") == 0) {
    gConfig.fFullScreen = atoi(value) ? TRUE : FALSE;
  } else if (strcasecmp(key, "EnableAviPlay") == 0) {
    gConfig.fEnableAviPlay = atoi(value) ? TRUE : FALSE;
  } else if (strcasecmp(key, "WindowWidth") == 0) {
    gConfig.dwScreenWidth = (DWORD)strtoul(value, NULL, 10);
  } else if (strcasecmp(key, "WindowHeight") == 0) {
    gConfig.dwScreenHeight = (DWORD)strtoul(value, NULL, 10);
  }
}

void PAL_LoadConfig(BOOL fFromFile) {
  FILE *fp;
  char buf[512];

  memset(&gConfig, 0, sizeof(CONFIGURATION));

  // 默认值
  gConfig.fFullScreen = FALSE;
  gConfig.fEnableAviPlay = TRUE;
  gConfig.dwScreenWidth = 640;
  gConfig.dwScreenHeight = 400;

  if (fFromFile && (fp = fopen("sdlpal.cfg", "r"))) {
    while (fgets(buf, sizeof(buf), fp))
      parse_line(buf);
    fclose(fp);
  }

  // 后处理
  if (!gConfig.pszGamePath) {
    gConfig.pszGamePath = strdup("./");
  }
}

void PAL_FreeConfig(void) {
  if (gConfig.pszGamePath)
    free(gConfig.pszGamePath);
  memset(&gConfig, 0, sizeof(CONFIGURATION));
}
