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

#ifndef MAIN_H
#define MAIN_H

#include "audio.h"
#include "aviplay.h"
#include "battle.h"
#include "common.h"
#include "fight.h"
#include "global.h"
#include "input.h"
#include "menu.h"
#include "map.h"
#include "palcfg.h"
#include "palette.h"
#include "play.h"
#include "rngplay.h"
#include "scene.h"
#include "script.h"
#include "text.h"
#include "ui.h"
#include "uibattle.h"
#include "video.h"

#define FPS 10
#define FRAME_TIME (1000 / FPS)

VOID PAL_Shutdown(int exit_code);

#endif
