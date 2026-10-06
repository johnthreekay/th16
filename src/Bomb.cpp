#include <stdlib.h>
#include <string.h>

#include "Bomb.h"

#include "AsciiManager.h"
#include "EnemyManager.h"
#include "Globals.h"
#include "Gui.h"
#include "Player.h"
#include "SoundManager.h"
#include "Spellcard.h"

// GLOBAL: TH16 0x4a6da4
BombInf *g_SubseasonBomb;

// GLOBAL: TH16 0x4a6da8
BombInf *g_MainBomb;

// FUNCTION: TH16 0x40d520
i32 BombInf::begin()
{
    return 0;
}

// FUNCTION: TH16 0x40d530
i32 BombInf::on_tick()
{
    return 0;
}

// FUNCTION: TH16 0x40d540
i32 BombInf::on_draw()
{
    return 0;
}

// FUNCTION: TH16 0x40d550
i32 BombInf::method_c(i32 a, i32 b)
{
    return 0;
}

// FUNCTION: TH16 0x40d560
i32 BombInf::method_10()
{
    return 0;
}

// FUNCTION: TH16 0x40d570
void BombInf::method_14()
{
}

// FUNCTION: TH16 0x40d580
BombInf::BombInf()
{
    memset(this, 0, sizeof(*this));
    flags |= 2;
}

// FUNCTION: TH16 0x40d600
i32 BombInf::initialize(i32 is_season)
{
    UpdateFunc *f = g_UpdateFuncRegistry->create_func(on_tick_callback);
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_tick(f, is_season + 24);
    on_tick_func = f;

    f = g_UpdateFuncRegistry->create_func(on_draw_callback);
    f->flags |= UPDATE_FUNC_ACTIVE;
    f->arg = this;
    g_UpdateFuncRegistry->register_on_draw(f, is_season + 40);
    on_draw_func = f;

    timer = 0;
    this->is_season = is_season;
    release_bonus_timer = 0;
    release_bonus_shown = -1.0f;
    release_bonus = 0.0f;
    return 0;
}

// FUNCTION: TH16 0x40d710
BombInf::~BombInf()
{
    delete_vm_and_clear(anm_id);
    delete_vm_and_clear(anm_id_60);
    delete_vm_and_clear(anm_id_64);
    delete_vm_and_clear(anm_id_bc);
    delete_vm_and_clear(anm_id_c0);
    delete_vm_and_clear(anm_id_c4);
    g_UpdateFuncRegistry->unregister_locked(on_tick_func);
    g_UpdateFuncRegistry->unregister_locked(on_draw_func);
    if (unk_70 != NULL)
    {
        free(unk_70);
        unk_70 = NULL;
    }
    if (unk_d0 != NULL)
    {
        free(unk_d0);
        unk_d0 = NULL;
    }
}

// Creates the bomb of the player's character and the release of their
// subseason. Returns the bomb, or NULL if either fails to start.
// FUNCTION: TH16 0x40d890
BombInf *BombInf::create()
{
    BombInf *bomb;
    switch (g_Globals.subshot + g_Globals.character)
    {
    default:
        bomb = new BombReimuAInf;
        break;
    case 1:
        bomb = new BombCirnoAInf;
        break;
    case 2:
        bomb = new BombAyaAInf;
        break;
    case 3:
        bomb = new BombMarisaAInf;
        break;
    }
    if (bomb->initialize(0) == 0)
    {
        g_MainBomb = bomb;

        BombInf *release;
        switch (g_Globals.subseason)
        {
        case 3:
            release = new BombMarisaSubInf;
            break;
        case 1:
            release = new BombCirnoSubInf;
            break;
        case 2:
            release = new BombAyaSubInf;
            break;
        case 4:
            release = new BombAllSubInf;
            break;
        default:
            release = new BombReimuSubInf;
            break;
        }
        if (release->initialize(1) != 0)
        {
            delete release;
        }
        else
        {
            g_SubseasonBomb = release;
            return bomb;
        }
    }
    delete bomb;
    return NULL;
}

// FUNCTION: TH16 0x40da60
int __fastcall BombInf::on_tick_callback(void *arg)
{
    return ((BombInf *)arg)->update();
}

// FUNCTION: TH16 0x40da70
int __fastcall BombInf::on_draw_callback(void *arg)
{
    BombInf *bomb = (BombInf *)arg;
    bomb->on_draw();
    bomb->draw();
    return 1;
}

// FUNCTION: TH16 0x40da90
void BombInf::destroy_all()
{
    switch (g_Globals.subshot + g_Globals.character)
    {
    default:
        delete (BombReimuAInf *)g_MainBomb;
        break;
    case 1:
        delete (BombCirnoAInf *)g_MainBomb;
        break;
    case 2:
        delete (BombAyaAInf *)g_MainBomb;
        break;
    case 3:
        delete (BombMarisaAInf *)g_MainBomb;
        break;
    }
    BombInf *release = g_SubseasonBomb;
    switch (g_Globals.subseason)
    {
    default:
        delete (BombReimuSubInf *)release;
        break;
    case 1:
        delete (BombCirnoSubInf *)release;
        break;
    case 2:
        delete (BombAyaSubInf *)release;
        break;
    case 3:
        delete (BombMarisaSubInf *)release;
        break;
    case 4:
        delete (BombAllSubInf *)release;
        break;
    }
    g_MainBomb = NULL;
    g_SubseasonBomb = NULL;
}

// TODO: the season level loop tests its counter where the original tests
// the pointer, and edi is saved late instead of in the prologue.
// FUNCTION: TH16 0x40db20
i32 BombInf::activate()
{
    if (in_use)
    {
        return -1;
    }
    in_use = 1;
    timer = 0;
    if (!is_season)
    {
        g_Globals.bombs--;
        if (g_Globals.bombs < 0)
        {
            g_Globals.bombs = 0;
        }
        else if (g_Globals.bombs > 8)
        {
            g_Globals.bombs = 8;
        }
        if (g_Gui != NULL)
        {
            g_Gui->update_bombs(g_Globals.bombs, g_Globals.bomb_fragments);
        }
    }
    if (!is_season && (g_Spellcard->flags & SPELLCARD_ACTIVE) && g_Spellcard->time.current >= 60)
    {
        started_during_spell = 1;
    }
    else
    {
        started_during_spell = 0;
    }
    if (is_season)
    {
        g_SoundManager.play_sound_at_position(77, g_Player->inner.pos.x);
    }
    else
    {
        g_SoundManager.play_sound_at_position(44, g_Player->inner.pos.x);
    }
    begin();
    if (is_season)
    {
        if (release_bonus_timer.current > 60)
        {
            release_bonus_timer.set_value(60);
            release_bonus_shown = release_bonus;
            release_bonus_pos = g_Player->inner.pos;
            release_bonus_pos.y -= 32.0f;
        }
        season_level = g_Globals.season_level();
        release_bonus = 0.0f;
        if (g_Globals.subseason == 1 || g_Globals.subseason == 4)
        {
            i32 power = g_Globals.season_power - g_Globals.season_level_deltas[season_level];
            g_Globals.season_power = power < 0 ? 0 : power;
        }
        else
        {
            g_Globals.season_power = 0;
            if (g_Globals.season_power > g_Globals.max_season_power)
            {
                g_Globals.season_power = g_Globals.max_season_power;
            }
        }
        g_Player->inner.repopulate_options();
        Gui::update_season_gauge();
        return 0;
    }
    g_EnemyManager->inner.can_still_capture_spell = 0;
    return 0;
}

// FUNCTION: TH16 0x40dd00
i32 BombInf::update()
{
    if (timer.current < 0)
    {
        timer.tick();
    }
    if (in_use)
    {
        if (on_tick() != 0)
        {
            in_use = 0;
            return 1;
        }
        timer++;
    }
    return 1;
}

// FUNCTION: TH16 0x40dda0
i32 BombInf::can_activate()
{
    if (!is_season)
    {
        if (g_Globals.bombs <= 0)
        {
            return 0;
        }
    }
    else if (g_Globals.season_level() <= 0 || timer.current < 0)
    {
        return 0;
    }
    if (g_MainBomb == NULL || g_MainBomb->in_use == 1)
    {
        return 0;
    }
    if (g_SubseasonBomb == NULL || g_SubseasonBomb->in_use == 1)
    {
        return 0;
    }
    if (g_Gui == NULL || g_Gui->msg != NULL)
    {
        return 0;
    }
    if (g_EnemyManager == NULL || g_EnemyManager->enemy_count_real == 0)
    {
        return 0;
    }
    return 1;
}

// TODO: the inlined decrement lacks the original's multiply by 1.0f (see
// ZunTimer::operator--).
// FUNCTION: TH16 0x40de30
void BombInf::draw()
{
    if (release_bonus_timer.current <= 0)
    {
        release_bonus_shown = -1.0f;
        return;
    }
    if (release_bonus_timer.current <= 60)
    {
        if (0.0f > release_bonus_shown)
        {
            release_bonus_shown = release_bonus;
            release_bonus_pos = g_Player->inner.pos;
            release_bonus_pos.y -= 32.0f;
        }
        u32 colors[7] = {0x60606060, 0xa0b0b080, 0xb0b8b880, 0xc0c0c080, 0xd0d0d080, 0xe0e0e080, 0xffffff30};
        AsciiInf *ascii = g_AsciiManager;
        ascii->color.d3d = colors[release_bonus_level];
        ascii->group = 1;
        ascii->font_id = 2;
        ascii->align_h = 0;
        ascii->align_v = 2;
        ascii->create_stringf(&release_bonus_pos, "+%d", (i32)release_bonus_shown / 10 * 10);
        g_AsciiManager->color.d3d = 0xffffffff;
        g_AsciiManager->color.a = 0xff;
        g_AsciiManager->font_id = 0;
        g_AsciiManager->group = 0;
        g_AsciiManager->align_h = 1;
        g_AsciiManager->align_v = 1;
    }
    release_bonus_timer.decrement(1.0f);
}

// FUNCTION: TH16 0x40e040
void BombInf::start_release_cooldown()
{
    release_bonus_shown = -1.0f;
    release_bonus_level = season_level;
    release_bonus_timer = 180;
    timer = -45;
}

// FUNCTION: TH16 0x40ebe0
i32 BombAyaAInf::on_draw()
{
    return 1;
}

// FUNCTION: TH16 0x40ebf0
i32 BombAyaAInf::method_c(i32 a, i32 b)
{
    return 0;
}

// FUNCTION: TH16 0x40ec00
void BombAyaAInf::method_14()
{
}

// FUNCTION: TH16 0x40f490
i32 BombCirnoAInf::on_draw()
{
    return 1;
}

// FUNCTION: TH16 0x40f4a0
i32 BombCirnoAInf::method_c(i32 a, i32 b)
{
    return 0;
}

// FUNCTION: TH16 0x40f4b0
void BombCirnoAInf::method_14()
{
}

// FUNCTION: TH16 0x40fe60
i32 BombMarisaAInf::on_draw()
{
    return 1;
}

// FUNCTION: TH16 0x40fe70
i32 BombMarisaAInf::method_c(i32 a, i32 b)
{
    return 0;
}

// FUNCTION: TH16 0x4100f0
void BombMarisaAInf::method_14()
{
}

// FUNCTION: TH16 0x411260
i32 BombReimuAInf::on_draw()
{
    return 1;
}

// FUNCTION: TH16 0x411270
i32 BombReimuAInf::method_c(i32 a, i32 b)
{
    return 0;
}
