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

#ifndef GLOBAL_H
#define GLOBAL_H

#include "common.h"
#include "map.h"
#include "ui.h"

//
// SOME NOTES ON "AUTO SCRIPT" AND "TRIGGER SCRIPT":
//
// Auto scripts are executed automatically in each frame.
//
// Trigger scripts are only executed when the event is triggered (player touched
// an event object, player triggered an event script by pressing Spacebar).
//

typedef enum tagSTATUS {
  kStatusConfused = 0, // 疯魔
  kStatusParalyzed,    // 定身
  kStatusSleep,        // 昏睡
  kStatusSilence,      // 咒封
  kStatusPuppet,       // 死者继续攻击
  kStatusBravery,      // 普通攻击加强
  kStatusProtect,      // 防御加强
  kStatusHaste,        // 身法加强
  kStatusDualAttack,   // 两次攻击
  kStatusAll
} STATUS;

typedef enum tagBODYPART {
  kBodyPartHead = 0, // 头戴
  kBodyPartBody,     // 披挂
  kBodyPartShoulder, // 身穿
  kBodyPartHand,     // 手持
  kBodyPartFeet,     // 脚穿
  kBodyPartWear,     // 佩戴
  kBodyPartExtra,    // 梦蛇/愤怒/虚弱带来的外形改变/属性提升
} BODYPART;

typedef enum tagOBJECTSTATE : SHORT {
  kObjStateHidden = 0, // 隐藏
  kObjStateNormal = 1, // 穿墙
  kObjStateBlocker = 2 // 实体
} OBJECTSTATE;

typedef enum tagTRIGGERMODE : WORD {
  kTriggerNone = 0,         // 无法触发
  kTriggerSearchNear = 1,   // 近距离调查触发
  kTriggerSearchNormal = 2, // 中距离调查触发
  kTriggerSearchFar = 3,    // 远距离调查触发
  kTriggerTouchNear = 4,    // 近距离接触触发
  kTriggerTouchNormal = 5,  // 中距离接触触发
  kTriggerTouchFar = 6,     // 远距离接触触发
  kTriggerTouchFarther = 7, // 超远距离接触触发
  kTriggerTouchFarthest = 8 // 超特远距离接触触发
} TRIGGERMODE;

typedef struct tagEVENTOBJECT {
  SHORT sVanishTime;              // 隐匿帧, 乘以 10 即秒数
  WORD x;                         // 在地图上的 X 轴坐标
  WORD y;                         // 在地图上的 Y 轴坐标
  SHORT sLayer;                   // 图层
  WORD wTriggerScript;            // 触发脚本
  WORD wAutoScript;               // 自动脚本
  OBJECTSTATE sState;             // 对象状态 (<0 为暂时隐藏)
  TRIGGERMODE wTriggerMode;       // 触发方式
  WORD wSpriteNum;                // 场景形象编号
  WORD nSpriteFrames;             // 每个方向有多少帧 (一般为 3)
  WORD wDirection;                // 面朝方向 (第几组帧)
  WORD wCurrentFrameNum;          // 当前帧编号
  WORD nScriptIdleFrame;          // 触发脚本的空白帧数
  WORD wSpritePtrOffset;          // FIXME: ???
  WORD nSpriteFramesAuto;         // 总帧数
  WORD wScriptIdleFrameCountAuto; // 自动脚本的空白帧数
} EVENTOBJECT, *LPEVENTOBJECT;

typedef struct tagSCENE {
  WORD wMapNum;           // 地图编号
  WORD wScriptOnEnter;    // 进场脚本
  WORD wScriptOnLeave;    // 传送出地图前执行脚本
  WORD wEventObjectIndex; // 场景下所有的事件的起始编号 (为 index + 1)
} SCENE_T;

// object including system strings, players, items, magics, enemies and poison scripts.

// system strings and players
typedef struct tagOBJECT_PLAYER {
  WORD wReserved[2];         // always zero
  WORD wScriptOnFriendDeath; // when friends in party dies, execute script from here
  WORD wScriptOnDying;       // when dying, execute script from here
} OBJECT_PLAYER;

typedef enum tagITEMFLAG : WORD {
  kItemFlagUsable = (1 << 0),
  kItemFlagEquipable = (1 << 1),
  kItemFlagThrowable = (1 << 2),
  kItemFlagConsuming = (1 << 3),
  kItemFlagApplyToAll = (1 << 4),
  kItemFlagSellable = (1 << 5),
  kItemFlagEquipableByPlayerRole_First = (1 << 6)
} ITEMFLAG;

// items
typedef struct tagOBJECT_ITEM {
  WORD wBitmap;        // bitmap number in BALL.MKF
  WORD wPrice;         // price
  WORD wScriptOnUse;   // script executed when using this item
  WORD wScriptOnEquip; // script executed when equipping this item
  WORD wScriptOnThrow; // script executed when throwing this item to enemy
  WORD wScriptDesc;    // description script
  ITEMFLAG wFlags;     // flags
} OBJECT_ITEM;

typedef enum tagMAGICFLAG : WORD {
  kMagicFlagUsableOutsideBattle = (1 << 0),
  kMagicFlagUsableInBattle = (1 << 1),
  kMagicFlagUsableToEnemy = (1 << 3),
  kMagicFlagApplyToAll = (1 << 4),
} MAGICFLAG;

// magics
typedef struct tagOBJECT_MAGIC {
  WORD wMagicNumber;     // magic number, according to DATA.MKF #3
  WORD wReserved1;       // always zero
  WORD wScriptOnSuccess; // when magic succeed, execute script from here
  WORD wScriptOnUse;     // when use this magic, execute script from here
  WORD wScriptDesc;      // description script
  WORD wReserved2;       // always zero
  MAGICFLAG wFlags;      // flags
} OBJECT_MAGIC;

// enemies
typedef struct tagOBJECT_ENEMY {
  WORD wEnemyID;             // ID of the enemy, according to DATA.MKF #1.
                             // Also indicates the bitmap number in ABC.MKF.
  WORD wResistanceToSorcery; // resistance to sorcery and poison (0 min, 10 max)
  WORD wScriptOnTurnStart;   // script executed when turn starts
  WORD wScriptOnBattleEnd;   // script executed when battle ends
  WORD wScriptOnReady;       // script executed when the enemy is ready
} OBJECT_ENEMY;

// poisons (scripts executed in each round)
typedef struct tagOBJECT_POISON {
  WORD wPoisonLevel;  // level of the poison
  WORD wColor;        // color of avatars
  WORD wPlayerScript; // script executed when player has this poison (per round)
  WORD wReserved;     // always zero
  WORD wEnemyScript;  // script executed when enemy has this poison (per round)
} OBJECT_POISON;

typedef union tagOBJECT {
  WORD rgwData[7];
  OBJECT_PLAYER player;
  OBJECT_ITEM item;
  OBJECT_MAGIC magic;
  OBJECT_ENEMY enemy;
  OBJECT_POISON poison;
} OBJECT_T;

typedef struct tagSCRIPTENTRY {
  WORD wOperation;    // operation code
  WORD rgwOperand[3]; // operands
} SCRIPTENTRY, *LPSCRIPTENTRY;

typedef struct tagINVENTORY {
  WORD wItem;        // item object code
  WORD nAmount;      // amount of this item
  WORD nAmountInUse; // in-use amount of this item
} INVENTORY;

typedef struct tagSTORE {
  WORD rgwItems[MAX_STORE_ITEM];
} STORE_T;

typedef struct tagENEMY {
  WORD wIdleFrames;                          // idle frame 总帧数
  WORD wMagicFrames;                         // total number of frames when using magics
  WORD wAttackFrames;                        // total number of frames when doing normal attack
  WORD wIdleAnimSpeed;                       // 等待多少帧切换下一张 idle frame
  WORD wActWaitFrames;                       // FIXME: ???
  WORD wYPosOffset;                          // 队形坐标偏移
  SHORT wAttackSound;                        // sound played when this enemy uses normal attack
  SHORT wActionSound;                        // FIXME: ???
  SHORT wMagicSound;                         // sound played when this enemy uses magic
  SHORT wDeathSound;                         // sound played when this enemy dies
  SHORT wCallSound;                          // sound played when entering the battle
  WORD wHealth;                              // 血量
  WORD wExp;                                 // 经验值
  WORD wCash;                                // 奖励
  WORD wLevel;                               // 等级
  WORD wMagic;                               // 法术
  WORD wMagicRate;                           // 施法频率
  WORD wAttackEquivItem;                     // 武器
  WORD wAttackEquivItemRate;                 // 普攻频率
  WORD wStealItem;                           // 可偷物品
  WORD nStealItem;                           // 可偷数量
  SHORT wAttackStrength;                     // 武术
  SHORT wMagicStrength;                      // 灵力
  SHORT wDefense;                            // 防御
  SHORT wDexterity;                          // 身法
  WORD wFleeRate;                            // 吉运
  WORD wPoisonResistance;                    // 毒抗
  WORD wElemResistance[NUM_MAGIC_ELEMENTAL]; // 法抗
  WORD wPhysicalResistance;                  // 物抗
  WORD wDualMove;                            // 攻击两次
  WORD wCollectValue;                        // 灵壶获得值
} ENEMY_T;

typedef struct tagENEMYTEAM {
  WORD id[MAX_ENEMIES_IN_TEAM];
} ENEMYTEAM;

typedef WORD PLAYERS[MAX_PLAYER_ROLES];

// https://github.com/palxex/palresearch/blob/master/PalScript/AttributeID.txt
typedef struct tagPLAYERROLES {
  PLAYERS rgwAvatar;                                                  // 状态表情图像 (in RGM.MKF)
  PLAYERS rgwSpriteNumInBattle;                                       // 战斗模型 (in F.MKF)
  PLAYERS rgwSpriteNum;                                               // 地图模型 (in MGO.MKF)
  PLAYERS rgwName;                                                    // 名字 (in WORD.DAT)
  PLAYERS rgwAttackAll;                                               // 可否攻击全体
  PLAYERS rgwUnknown1;                                                // FIXME: ???
  PLAYERS rgwLevel;                                                   // 等级
  PLAYERS rgwMaxHP;                                                   // 体力最大值
  PLAYERS rgwMaxMP;                                                   // 真气最大值
  PLAYERS rgwHP;                                                      // 体力
  PLAYERS rgwMP;                                                      // 真气
  WORD rgwEquipment[MAX_PLAYER_EQUIPMENTS][MAX_PLAYER_ROLES];         // 佩戴的装备编号
  PLAYERS rgwAttackStrength;                                          // 武术
  PLAYERS rgwMagicStrength;                                           // 灵力
  PLAYERS rgwDefense;                                                 // 防御
  PLAYERS rgwDexterity;                                               // 身法
  PLAYERS rgwFleeRate;                                                // 吉运
  PLAYERS rgwPoisonResistance;                                        // 毒抗
  WORD rgwElementalResistance[NUM_MAGIC_ELEMENTAL][MAX_PLAYER_ROLES]; // 法抗
  PLAYERS rgwUnknown2;                                                // FIXME: ???
  PLAYERS rgwUnknown3;                                                // FIXME: ???
  PLAYERS rgwUnknown4;                                                // FIXME: ???
  PLAYERS rgwCoveredBy;                                               // 谁掩护我
  WORD rgwMagic[MAX_PLAYER_MAGICS][MAX_PLAYER_ROLES];                 // 掌握的仙术编号
  PLAYERS rgwWalkFrames;                                              // 单方向行走帧数 (0(for 3) or 4)
  PLAYERS rgwCooperativeMagic;                                        // 合体法术
  PLAYERS rgwUnknown5;                                                // FIXME: ???
  PLAYERS rgwUnknown6;                                                // FIXME: ???
  PLAYERS rgwDeathSound;                                              // sound played when player dies
  PLAYERS rgwAttackSound;                                             // sound played when player attacks
  PLAYERS rgwWeaponSound;                                             // weapon sound (???)
  PLAYERS rgwCriticalSound;                                           // sound played when player make critical hits
  PLAYERS rgwMagicSound;                                              // sound played when player is casting a magic
  PLAYERS rgwCoverSound;                                              // sound played when player cover others
  PLAYERS rgwDyingSound;                                              // sound played when player is dying
} PLAYERROLES;

typedef enum tagMAGIC_TYPE : WORD {
  kMagicTypeNormal = 0,
  kMagicTypeAttackAll = 1,     // 全体攻击 - 逐个渲染
  kMagicTypeAttackWhole = 2,   // 全体攻击 - 整体渲染
  kMagicTypeAttackField = 3,   // 战场/全屏攻击
  kMagicTypeApplyToPlayer = 4, // 作用于我方单人
  kMagicTypeApplyToParty = 5,  // 作用于我方全队
  kMagicTypeTrance = 8,        // 变身/爆发状态
  kMagicTypeSummon = 9,        // 召唤术
} MAGIC_TYPE;

typedef struct tagMAGIC {
  WORD wEffect;     // 特效编号 (in fire.mkf)
  MAGIC_TYPE wType; // 魔法类型
  SHORT wXOffset;
  SHORT wYOffset;
  SHORT sLayerOffset; // actual layer: PAL_Y(pos) + wYOffset + wMagicLayerOffset
  SHORT wSpeed;       // speed of the effect
  WORD wKeepEffect;   // 0xFFFF 为施法后在战场上留下印记
  WORD wFireDelay;    // 起手帧数 (不在循环里)
  WORD wEffectTimes;  // 循环次数
  WORD wShake;        // 震屏
  WORD wWave;         // 波纹/扭曲
  WORD wUnknown;      // FIXME: ???
  WORD wCostMP;       // MP 消耗
  SHORT wBaseDamage;  // 基础伤害 (-999 for 回梦/夺魂/鬼降/飞龙/灵壶)
  WORD wElemental;    // 元素属性 (0 = 无属性, 最后一个值 = 毒属性)
  SHORT wSound;       // 音效
} MAGIC_T;

typedef struct tagSUMMONGOD {
  WORD wMagicNumber; // magic number, according to DATA.MKF #3
  WORD wType;        // 魔法类型
  SHORT wXOffset;
  SHORT wYOffset;
  WORD wEffect; // 特效编号 (in f.mkf)
  WORD wIdleFrames;
  WORD wMagicFrames;
  WORD wAttackFrames;
  SHORT sColorShift;
  WORD wShake;
  WORD wWave;
  WORD wUnknown;
  WORD wCostMP;
  WORD wBaseDamage;
  WORD wElemental;
  SHORT wSound;
} SUMMONGOD_T;

typedef struct tagBATTLEFIELD {
  WORD wScreenWave;                          // level of screen waving
  SHORT rgsMagicEffect[NUM_MAGIC_ELEMENTAL]; // effect of attributed magics
} BATTLEFIELD_T;

// magics learned when level up
typedef struct tagLEVELUPMAGIC {
  WORD wLevel; // level reached
  WORD wMagic; // magic learned
} LEVELUPMAGIC;

typedef struct tagLEVELUPMAGIC_ALL {
  LEVELUPMAGIC m[MAX_PLAYABLE_PLAYER_ROLES];
} LEVELUPMAGIC_ALL;

typedef struct tagPALPOS {
  WORD x;
  WORD y;
} PALPOS;

typedef struct tagENEMYPOS {
  PALPOS pos[MAX_ENEMIES_IN_TEAM][MAX_ENEMIES_IN_TEAM];
} ENEMYPOS;

// player party
typedef struct tagPARTY {
  WORD role;         // 角色
  SHORT x, y;        // 坐标
  WORD wFrame;       // 当前行走帧
  WORD wImageOffset; // FIXME: ???
} PARTY_T;

// player trail, used for other party members to follow the main party member
typedef struct tagTRAIL {
  WORD x, y;       // position
  WORD wDirection; // direction
} TRAIL_T;

typedef struct tagEXPERIENCE {
  WORD wExp; // current experience points
  WORD wReserved;
  WORD wLevel; // current level
  WORD wCount;
} EXPERIENCE;

typedef struct tagALLEXPERIENCE {
  EXPERIENCE rgPrimaryExp[MAX_PLAYER_ROLES];
  EXPERIENCE rgHealthExp[MAX_PLAYER_ROLES];
  EXPERIENCE rgMagicExp[MAX_PLAYER_ROLES];
  EXPERIENCE rgAttackExp[MAX_PLAYER_ROLES];
  EXPERIENCE rgMagicPowerExp[MAX_PLAYER_ROLES];
  EXPERIENCE rgDefenseExp[MAX_PLAYER_ROLES];
  EXPERIENCE rgDexterityExp[MAX_PLAYER_ROLES];
  EXPERIENCE rgFleeExp[MAX_PLAYER_ROLES];
} ALLEXPERIENCE;

typedef struct tagPOISONSTATUS {
  WORD wPoisonID;     // kind of the poison
  WORD wPoisonScript; // script entry
} POISONSTATUS;

// following are data to be saved or loaded
typedef struct tagSAVEDATA {
  WORD saveSlot;                // 当前存档位置 (1-5)
  PAL_POS viewport;             // 视口坐标
  WORD wMaxPartyMemberIndex;    // 当前队伍人数 - 1
  WORD wCurScene;               // 当前地图 ID
  WORD _paletteOffset;          //
  WORD wPartyDirection;         // 队伍朝向
  WORD wCurMusic;               // 当前 BGM ID
  WORD wCurBattleMusic;         // 当前战斗 BGM ID
  WORD wCurBattleField;         // 当前战场 ID
  WORD wScreenWave;             // 屏幕波浪扭曲程度
  WORD unused;                  //
  WORD wCollectValue;           // 灵葫积累值
  WORD wLayer;                  // 当前地图层级
  WORD wChaseRange;             // 敌人追击范围
  WORD wChasespeedChangeCycles; // 追击速度变化周期
  WORD nFollower;               // NPC 跟随者数量
  WORD unused2[3];              //
  DWORD dwCash;                 // 金钱

  PARTY_T party[MAX_PLAYABLE_PLAYER_ROLES]; // 队伍角色数据
  TRAIL_T trail[MAX_PLAYABLE_PLAYER_ROLES]; // 跟随轨迹数据
  ALLEXPERIENCE Exp;                        // 经验值
  PLAYERROLES playerRoles;                  // 主角群数据

  POISONSTATUS rgPoisonStatus[MAX_POISONS][MAX_PLAYABLE_PLAYER_ROLES]; // 中毒状态

  INVENTORY rgInventory[MAX_INVENTORY]; // 物品数据
  SCENE_T scenes[MAX_SCENES];           // 场景/地图定义数组 (包含地图编号、脚本入口等信息)
  OBJECT_T objects[MAX_OBJECTS];        // 物品/道具定义数组 (包括装备、药品、剧情道具的属性)
  EVENTOBJECT events[MAX_EVENTS];       // 事件对象
} SAVEDATA;

extern SAVEDATA g;

#define SCENE_EVENT_OBJ_IDX_ST g.scenes[g.wCurScene - 1].wEventObjectIndex
#define SCENE_EVENT_OBJ_IDX_ED g.scenes[g.wCurScene].wEventObjectIndex
#define PLAYER g.playerRoles
#define OBJECT g.objects
#define PARTY g.party
#define PARTY_PLAYER(i) g.party[i].role
#define TRAIL g.trail

// game data which is available in data files.
extern SCRIPTENTRY SCRIPT[43557];
extern STORE_T STORE[21];
extern ENEMY_T ENEMY[154];
extern ENEMYTEAM ENEMY_TEAM[380];
extern MAGIC_T MAGIC[104];
extern BATTLEFIELD_T BATTLEFIELD[58];
extern LEVELUPMAGIC_ALL LEVELUP_MAGIC[20];
extern int N_LEVELUP_MAGIC;
extern ENEMYPOS ENEMY_POS;
extern WORD LEVELUP_EXP[MAX_LEVELS + 1];
extern WORD BATTLE_EFFECT[10][2];

extern PLAYERROLES gEquipmentEffect[MAX_PLAYER_EQUIPMENTS + 1];
extern WORD gPlayerStatus[MAX_PLAYER_ROLES][kStatusAll];

extern DWORD gFrameNum;
extern BOOL gEnteringScene;
extern BOOL gNeedToFadeIn;
extern BOOL gInBattle;
extern BOOL gAutoBattle;
extern WORD gLastUnequippedItem;
extern PAL_POS gPartyOffset;
extern WORD gCurPalette;
extern BOOL gNightPalette;
extern SHORT gWaveProgression;

extern FILE *FP_FBP;
extern FILE *FP_MGO;
extern FILE *FP_BALL;
extern FILE *FP_DATA;
extern FILE *FP_F;
extern FILE *FP_FIRE;
extern FILE *FP_RGM;
extern FILE *FP_SSS;

PAL_C_LINKAGE_BEGIN

INT PAL_InitGlobals(VOID);

VOID PAL_FreeGlobals(VOID);

VOID PAL_SaveGame(int iSaveSlot, WORD wSavedTimes);

VOID PAL_InitGameData(INT iSaveSlot);

VOID PAL_ReloadInNextTick(INT iSaveSlot);

INT PAL_CountItem(WORD wObjectID);

BOOL PAL_GetItemIndexToInventory(WORD wObjectID, INT *index);

INT PAL_AddItemToInventory(WORD wObjectID, INT iNum);

BOOL PAL_IncreaseHPMP(WORD role, SHORT sHP, SHORT sMP);

INT PAL_GetItemAmount(WORD wItem);

VOID PAL_UpdateEquipmentEffect(VOID);

VOID PAL_CompressInventory(VOID);

VOID PAL_RemoveEquipmentEffect(WORD role, WORD wEquipPart);

VOID PAL_AddPoisonForPlayer(WORD role, WORD wPoisonID);

VOID PAL_CurePoisonByKind(WORD role, WORD wPoisonID);

VOID PAL_CurePoisonByLevel(WORD role, WORD wMaxLevel);

BOOL PAL_IsPlayerPoisonedByLevel(WORD role, WORD wMinLevel);

BOOL PAL_IsPlayerPoisonedByKind(WORD role, WORD wPoisonID);

WORD PAL_GetPlayerAttackStrength(WORD role);

WORD PAL_GetPlayerMagicStrength(WORD role);

WORD PAL_GetPlayerDefense(WORD role);

WORD PAL_GetPlayerDexterity(WORD role);

WORD PAL_GetPlayerFleeRate(WORD role);

WORD PAL_GetPlayerPoisonResistance(WORD role);

WORD PAL_GetPlayerElementalResistance(WORD role, INT iAttrib);

WORD PAL_GetPlayerBattleSprite(WORD role);

WORD PAL_GetPlayerCooperativeMagic(WORD role);

BOOL PAL_PlayerCanAttackAll(WORD role);

BOOL PAL_AddMagic(WORD role, WORD wMagic);

VOID PAL_RemoveMagic(WORD role, WORD wMagic);

BOOL PAL_SetPlayerStatus(WORD role, WORD wStatusID, WORD wNumRound);

VOID PAL_RemovePlayerStatus(WORD role, WORD wStatusID);

VOID PAL_ClearAllPlayerStatus(VOID);

VOID PAL_PlayerLevelUp(WORD role, WORD wNumLevel);

BOOL PAL_collectItem();

PAL_C_LINKAGE_END

#endif
