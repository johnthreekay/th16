// A stand-in caller for EnemyManager's boss bookkeeping.
//
// Not ZUN's code and never run: it gives link-time code generation calls
// or address uses the decompiled code does not have, so that it compiles
// the real functions as in the original (see docs/workflow.md, "Placeholders and
// stand-in callers"). The harness files are split the way the work was;
// regrouping them changes LTCG's choices for unrelated functions.
#include "../AsciiManager.h"
#include "../Enemy.h"
#include "../EnemyManager.h"
#include "../Input.h"

// Stands for ECL's setBoss (0x420667), the only caller of
// set_life_bar_hidden (0x41a950). It passes 0, which our LTCG folds into
// the function and the original's did not; a varying value keeps the
// parameter. Both functions reach the manager through g_EnemyManager, so
// LTCG drops their `this`.
void harness_enemy_manager_boss(int index, EnemyInf *enemy, int value)
{
    g_EnemyManager->set_life_bar_hidden(value);
    g_EnemyManager->set_boss_id(index, enemy);
}
