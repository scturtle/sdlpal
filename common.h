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

#ifndef _COMMON_H
#define _COMMON_H

#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <stdarg.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <float.h>

#include <SDL3/SDL.h>

inline int max(int a, int b) { return a > b ? a : b; }
inline int min(int a, int b) { return a < b ? a : b; }

// For SDL 1.2 compatibility
#ifndef SDL_TICKS_PASSED
#define SDL_TICKS_PASSED(A, B) ((A) >= (B))
#endif

# include <unistd.h>
# include <dirent.h>

# ifndef FALSE
#  define FALSE               0
# endif
# ifndef TRUE
#  define TRUE                1
# endif

#define VOID                void

typedef char                CHAR;
typedef wchar_t             WCHAR;
typedef short               SHORT;
typedef long                LONG;
typedef unsigned short      WORD;
typedef unsigned int        DWORD;
typedef int                 INT;
typedef bool                BOOL;
typedef unsigned int        UINT;
typedef unsigned char       BYTE, *LPBYTE;
typedef const BYTE         *LPCBYTE;
typedef float               FLOAT;
typedef const CHAR         *LPCSTR;
typedef WCHAR              *LPWSTR;
typedef const WCHAR        *LPCWSTR;

typedef LPBYTE  LPSPRITE, LPBITMAPRLE;
typedef LPCBYTE LPCSPRITE, LPCBITMAPRLE;

typedef enum tagPALDIRECTION {
  kDirSouth = 0,
  kDirWest = 1,
  kDirNorth = 2,
  kDirEast = 3,
  kDirUnknown = 4,
} PALDIRECTION;

typedef struct PAL_POS_t {
  int16_t x, y;
} PAL_POS;

#define PAL_XY(x, y) ((PAL_POS) { (x), (y) })
#define PAL_X(xy) ((xy).x)
#define PAL_Y(xy) ((xy).y)
#define PAL_XY_OFFSET(xy, xx, yy)  PAL_XY(PAL_X(xy) + (xx), PAL_Y(xy) + (yy))

// maximum number of players in party
#define     MAX_PLAYERS_IN_PARTY         3
// total number of possible player roles
#define     MAX_PLAYER_ROLES             6
// totally number of playable player roles
#define     MAX_PLAYABLE_PLAYER_ROLES    5
// maximum entries of inventory
#define     MAX_INVENTORY                256
// maximum items in a store
#define     MAX_STORE_ITEM               9
// total number of magic attributes
#define     NUM_MAGIC_ELEMENTAL          5
// maximum number of enemies in a team
#define     MAX_ENEMIES_IN_TEAM          5
// maximum number of equipments for a player
#define     MAX_PLAYER_EQUIPMENTS        6
// maximum number of magics for a player
#define     MAX_PLAYER_MAGICS            32
// maximum number of scenes
#define     MAX_SCENES                   300
// maximum number of objects
#define     MAX_OBJECTS                  600
// maximum number of event objects
#define     MAX_EVENTS                   5077
// maximum number of effective poisons to players
#define     MAX_POISONS                  16
// maximum number of level
#define     MAX_LEVELS                   99
// buffer size for decompress rle image
#define     PAL_RLEBUFSIZE               64000

#ifdef __cplusplus
# define PAL_C_LINKAGE       extern "C"
# define PAL_C_LINKAGE_BEGIN PAL_C_LINKAGE {
# define PAL_C_LINKAGE_END   }
#else
# define PAL_C_LINKAGE
# define PAL_C_LINKAGE_BEGIN
# define PAL_C_LINKAGE_END
#endif

#define PAL_fread(buf, elem, num, fp) if (fread((buf), (elem), (num), (fp)) < (num)) return -1

PAL_C_LINKAGE_BEGIN

INT PAL_RLEBlitToSurface(LPCBITMAPRLE lpBitmapRLE, SDL_Surface *lpDstSurface, PAL_POS pos);

INT PAL_RLEBlitToSurfaceWithShadow(LPCBITMAPRLE lpBitmapRLE, SDL_Surface *lpDstSurface, PAL_POS pos, BOOL bShadow);

INT PAL_RLEBlitWithColorShift(LPCBITMAPRLE lpBitmapRLE, SDL_Surface *lpDstSurface, PAL_POS pos, INT iColorShift);

INT PAL_RLEBlitMonoColor(LPCBITMAPRLE lpBitmapRLE, SDL_Surface *lpDstSurface, PAL_POS pos, BYTE bColor, INT iColorShift);

INT PAL_RLEGetWidth(LPCBITMAPRLE lpBitmapRLE);

INT PAL_RLEGetHeight(LPCBITMAPRLE lpBitmapRLE);

WORD PAL_SpriteGetNumFrames(LPCSPRITE lpSprite);

LPCBITMAPRLE
PAL_SpriteGetFrame(LPCSPRITE lpSprite, INT iFrameNum);

INT PAL_MKFGetChunkCount(FILE *fp);

INT PAL_MKFGetChunkSize(UINT uiChunkNum, FILE *fp);

INT PAL_MKFReadChunk(LPBYTE lpBuffer, UINT uiBufferSize, UINT uiChunkNum, FILE *fp);

INT PAL_MKFGetDecompressedSize(UINT uiChunkNum, FILE *fp);

INT PAL_MKFDecompressChunk(LPBYTE lpBuffer, UINT uiBufferSize, UINT uiChunkNum, FILE *fp);

// From yj1.c:
INT Decompress(const VOID *Source, VOID *Destination, INT DestSize);

void PAL_Delay(uint32_t ms);

uint64_t PAL_GetTicks();

void PAL_DelayUntil(uint32_t ms);

char *UTIL_va(char *buffer, int buflen, const char *format, ...);
#define PAL_va(i, fmt, ...) UTIL_va((char[PATH_MAX]){0}, PATH_MAX, fmt, __VA_ARGS__)

int RandomLong(int from, int to);

float RandomFloat(float from, float to);

void UTIL_Delay(unsigned int ms);

void TerminateOnError(const char *fmt, ...);

void *UTIL_malloc(size_t buffer_size);

FILE *UTIL_OpenRequiredFile(LPCSTR lpszFileName);

FILE *UTIL_OpenFile(LPCSTR lpszFileName);

FILE *UTIL_OpenFileForMode(LPCSTR lpszFileName, LPCSTR szMode);

FILE *UTIL_OpenFileAtPathForMode(LPCSTR lpszPath, LPCSTR lpszFileName, LPCSTR szMode);

const char *UTIL_CombinePath(char *buffer, size_t buflen, int numentry, ...);

const char *UTIL_GetFullPathName(char *buffer, size_t buflen, const char *basepath, const char *subpath);

PAL_C_LINKAGE_END

#endif
