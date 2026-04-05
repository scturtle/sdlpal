#include "common.h"

// Perform the selected action for a player
VOID PAL_BattlePlayerPerformAction(WORD wPlayerIndex);

// Perform action for enemy (attack, magic, confused behavior)
VOID PAL_BattleEnemyPerformAction(WORD wEnemyIndex);

// Essential checks after an action is executed (death, victory conditions, scripts)
VOID PAL_BattlePostActionCheck(BOOL fCheckPlayers);

// Commit player's action decision
VOID PAL_BattleCommitAction(BOOL fRepeat);
