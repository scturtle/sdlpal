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
// palcfg.h: Configuration definition.
//  @Author: Lou Yihua <louyihua@21cn.com>, 2016.
//

#ifndef CONFIG_H
#define CONFIG_H

#include "common.h"

typedef struct tagCONFIGURATION
{
	char            *pszGamePath;
	DWORD            dwScreenWidth;
	DWORD            dwScreenHeight;
	BOOL             fFullScreen;
	BOOL             fEnableAviPlay;
} CONFIGURATION, *LPCONFIGURATION;

PAL_C_LINKAGE_BEGIN

extern CONFIGURATION gConfig;

void PAL_LoadConfig(BOOL fFromFile);

void PAL_FreeConfig(void);

PAL_C_LINKAGE_END

#endif
