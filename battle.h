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

#ifndef BATTLE_H
#define BATTLE_H

#include "global.h"
#include "uibattle.h"

#define BATTLE_FPS 25
#define BATTLE_FRAME_TIME (1000 / BATTLE_FPS)

typedef enum tagBATTLERESULT {
  kBattleResultWon = 3,          // 玩家赢得战斗
  kBattleResultLost = 1,         // 玩家输掉战斗
  kBattleResultFleed = 0xFFFF,   // 玩家从战斗中逃跑
  kBattleResultTerminated = 0,   // 战斗因脚本执行而终止（如剧情强制结束）
  kBattleResultOnGoing = 1000,   // 战斗正在进行中
  kBattleResultPreBattle = 1001, // 正在运行战前脚本
  kBattleResultPause = 1002,     // 战斗暂停, 运行玩家愤怒脚本
} BATTLERESULT;

typedef enum tagFIGHTERSTATE {
  kFighterWait, // 等待状态
  kFighterCom,  // 指令状态, 行动条已满, 等待玩家选择
  kFighterAct,  // 已确定动作指令
} FIGHTERSTATE;

typedef enum tagBATTLEACTIONTYPE {
  kBattleActionPass,       // 跳过
  kBattleActionDefend,     // 防御
  kBattleActionAttack,     // 普攻
  kBattleActionMagic,      // 法术
  kBattleActionCoopMagic,  // 合体
  kBattleActionFlee,       // 逃跑
  kBattleActionThrowItem,  // 投掷物品
  kBattleActionUseItem,    // 使用物品
  kBattleActionAttackMate, // 攻击友方
} BATTLEACTIONTYPE;

typedef struct tagBATTLEACTION {
  BATTLEACTIONTYPE ActionType; // 动作类型
  WORD wActionID;              // 要使用的物品或仙术的ID
  SHORT sTarget;               // 目标ID（-1 代表全体）
} BATTLEACTION;

typedef struct tagBATTLEENEMY {
  WORD wObjectID;                      // 该敌人的对象ID (0 为 KO'ed)
  ENEMY_T e;                           // 该敌人的详细属性 (从 ENEMY 复制)
  WORD rgwStatus[kStatusAll];          // 状态效果数组
  POISONSTATUS rgPoisons[MAX_POISONS]; // 中毒状态数组
  WORD wScriptOnTurnStart;             // 回合开始时执行的脚本ID (从 OBJECT 复制)
  WORD wScriptOnBattleEnd;             // 战斗结束时执行的脚本ID
  WORD wScriptOnReady;                 // 敌人就绪时执行的脚本ID

  LPSPRITE lpSprite;
  PAL_POS pos;         // 屏幕上的当前坐标
  PAL_POS posOriginal; // 屏幕上的原始坐标
  WORD wCurrentFrame;  // 当前动画帧号
  WORD wPrevHP;        // 行动前的 HP (用于计算扣血动画)
  INT iColorShift;     // 颜色偏移值（用于受击变色等效果）
} BATTLEENEMY;

// 只在这里放置一些战斗专用的数据, 其他数据可以在全局数据中访问
typedef struct tagBATTLEPLAYER {
  BATTLEACTION action;     // 要执行的动作
  BATTLEACTION prevAction; // 上一回合的动作（用于“重复”功能）

  LPSPRITE lpSprite;
  PAL_POS pos;         // 屏幕上的当前坐标
  PAL_POS posOriginal; // 屏幕上的原始坐标
  WORD wCurrentFrame;  // 当前动画帧号
  FIGHTERSTATE state;  // 战斗状态（等待/行动中等）
  BOOL fDefending;     // 处于防御状态
  BOOL fSecondAttack;  // 双次攻击状态
  WORD wPrevHP;        // 行动前的 HP
  WORD wPrevMP;        // 行动前的 MP
  INT iColorShift;     // 颜色偏移值
} BATTLEPLAYER;

typedef enum tagBATTLESPRITETYPE {
  kBattleSpriteTypeEnemy,  // 敌人
  kBattleSpriteTypePlayer, // 玩家
  kBattleSpriteTypeMagic,  // 仙术特效
} BATTLESPRITETYPE;

typedef struct tagBATTLESPRITE {
  BATTLESPRITETYPE wType; // 精灵类型
  WORD wObjectIndex;      // 对象索引
  PAL_POS pos;            // 坐标位置
  SHORT sLayerOffset;     // 图层偏移量（处理遮挡关系）
  BOOL fHaveColorShift;   // 是否应用了颜色偏移
} BATTLESPRITE;

#define MAX_BATTLE_MAGICSPRITE_ITEMS 3
#define MAX_BATTLESPRITE_ITEMS (MAX_ENEMIES_IN_TEAM + MAX_PLAYABLE_PLAYER_ROLES + MAX_BATTLE_MAGICSPRITE_ITEMS)

typedef struct tagSUMMON {
  LPSPRITE lpSprite;  // 召唤灵/酒神的精灵指针
  WORD wCurrentFrame; // 当前帧号
} SUMMON;

#define MAX_BATTLE_ACTIONS 256
#define MAX_KILLED_ENEMIES 256

typedef enum tabBATTLEPHASE {
  kBattlePhaseSelectAction,  // 选择动作阶段（等待玩家输入）
  kBattlePhasePerformAction, // 执行动作阶段（播放动画/计算伤害）
} BATTLEPHASE;

typedef struct tagACTIONQUEUE {
  BOOL fIsEnemy;   // 是否为敌人
  WORD wDexterity; // 身法/速度值（决定行动顺序）
  WORD wIndex;     // 角色在对应数组中的索引
  BOOL fIsSecond;  // 是否为该角色的第二次行动
} ACTIONQUEUE;

#define MAX_ACTIONQUEUE_ITEMS (MAX_PLAYERS_IN_PARTY + MAX_ENEMIES_IN_TEAM * 2)

typedef struct tagBATTLE {

  SDL_Surface *lpSceneBuf;     // 场景缓冲表面（用于合成画面）
  SDL_Surface *lpBackground;   // 战斗背景图片
  SHORT sBackgroundColorShift; // 背景颜色偏移值

  LPSPRITE lpSummonSprite; // 召唤灵的精灵图像
  PAL_POS posSummon;       // 召唤灵的坐标
  INT iSummonFrame;        // 召唤灵的当前帧
  BOOL fSummonColorShift;  // 召唤灵是否变色

  BATTLEUI UI;                // 战斗用户界面数据
  LPBYTE lpEffectSprite;      // 特效精灵数据指针
  LPCBITMAPRLE lpMagicBitmap; // 当前仙术帧的位图（RLE压缩格式）
  int iBlow;                  // 风吹/击飞效果参数

  BATTLESPRITE SpriteDrawSeq[MAX_BATTLESPRITE_ITEMS]; // 精灵绘制序列
  WORD spriteDrawSeqSize;                             // 绘制序列的最大索引
  BOOL fSpriteAddLock;                                // 精灵添加锁

  BATTLEPLAYER rgPlayer[MAX_PLAYERS_IN_PARTY];    // 玩家角色数组
  BATTLEENEMY rgEnemy[MAX_ENEMIES_IN_TEAM];       // 敌人角色数组
  WORD wMaxEnemyIndex;                            // 敌人的最大索引值
  ACTIONQUEUE ActionQueue[MAX_ACTIONQUEUE_ITEMS]; // 动作队列（决定出手顺序）
  int iCurAction;                                 // 当前正在执行的动作索引
  BATTLEPHASE Phase;                              // 当前战斗阶段
  BATTLERESULT BattleResult;                      // 战斗结果状态
  INT iExpGained;                                 // 获得的经验总值
  INT iCashGained;                                // 获得的金钱总值
  INT iHidingTime;                                // 隐身效果剩余回合
  BOOL fIsBoss;                                   // 是否是 BOSS 战
  BOOL fEnemyCleared;                             // 敌人是否已被清除
  BOOL fEnemyMoving;                              // 敌人正在行动中
  WORD wMovingPlayerIndex;                        // 当前正在行动的玩家
  BOOL fRepeat;                                   // 玩家按下了重复
  BOOL fForce;                                    // 玩家按下了自动战斗
  BOOL fFlee;                                     // 玩家按下了逃跑
  BOOL fPrevAutoAtk;                              // 上一回合是否使用了自动攻击
  BOOL fPrevPlayerAutoAtk;                        // 上一个玩家在同一回合是否使用了自动攻击

  WORD coopContributors[MAX_PLAYERS_IN_PARTY]; // 合击技贡献者列表
  BOOL fThisTurnCoop;                          // 本回合是否触发了合击

} BATTLE;

#define BATTLE_PLAYER g_Battle.rgPlayer
#define BATTLE_ENEMY g_Battle.rgEnemy

PAL_C_LINKAGE_BEGIN

extern BATTLE g_Battle;

VOID PAL_LoadBattleSprites(VOID);

VOID PAL_BattleDrawBackground(VOID);

VOID PAL_BattleDrawEnemySprites(WORD wEnemyIndex, SDL_Surface *lpDstSurface);

VOID PAL_BattleDrawPlayerSprites(WORD wPlayerIndex, SDL_Surface *lpDstSurface);

VOID PAL_BattleDrawMagicSprites(INT iMagicNum, SDL_Surface *lpDstSurface, PAL_POS pos);

VOID PAL_BattleClearSpriteObject(VOID);

VOID PAL_BattleSpriteAddUnlock(VOID);

VOID PAL_BattleAddSpriteObject(WORD wType, WORD wObjectIndex, PAL_POS pos, SHORT sLayerOffset, BOOL fHaveColorShift);

VOID PAL_BattleAddFighterSpriteObject(VOID);

VOID PAL_BattleSortSpriteObjecByPos(VOID);

VOID PAL_BattleDrawAllSprites(VOID);

VOID PAL_BattleDrawAllSpritesWithColorShift(BOOL fColorShift);

VOID PAL_BattleMakeScene(VOID);

VOID PAL_BattleFadeScene(VOID);

VOID PAL_BattleEnemyEscape(VOID);

VOID PAL_BattlePlayerEscape(VOID);

BATTLERESULT
PAL_StartBattle(WORD wEnemyTeam, BOOL fIsBoss);

PAL_C_LINKAGE_END

#endif
