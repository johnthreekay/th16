// The enemy-specific ECL instructions (300 and up). Instruction names are
// ExpHP's (th-re-data labels.json).
#include "Enemy.h"
#include "AnmManager.h"
#include "CriticalSections.h"
#include "EffectManager.h"
#include "EnemyManager.h"
#include "Globals.h"
#include "Gui.h"
#include "PopupManager.h"
#include "SoundManager.h"
#include "BulletManager.h"
#include "Camera.h"
#include "Fog.h"
#include "GameThread.h"
#include "Laser.h"
#include "Player.h"
#include "Stage.h"
#include "Supervisor.h"
#include "ZunAngle.h"
#include "ZunMath.h"

// create_vm, inserted at the front of the world list like create_vm_front
// (0x426160, which is a copy with pos and rotation folded). LTCG inlined
// this into the anmPlay instructions.
static __forceinline AnmId create_vm_front_at(AnmLoaded *file, i32 script, Float3 *pos, f32 rotation)
{
    ENTER_CS(CS_ANM_MANAGER);
    file->vm_count++;
    AnmVm *vm = g_AnmManager->allocate_vm();
    file->copy_vm(vm, script);
    vm->flags_hi |= ANM_VM_CREATED_BY_GAME;
    if (pos == NULL)
    {
        vm->entity_pos = D3DXVECTOR3(0.0f, 0.0f, 0.0f);
    }
    else
    {
        vm->entity_pos = *pos;
    }
    vm->rotation.z = rotation;
    vm->run();
    vm->mode_of_create_child = 2;
    AnmId id;
    id = g_AnmManager->insert_in_world_list_front(vm);
    LEAVE_CS(CS_ANM_MANAGER);
    return id;
}

// pi - angle, wrapped: the direction mirrored about the vertical axis, for
// mirrored enemies (flags_low 0x80000).
static inline f32 mirror_angle(f32 angle)
{
    return normalize_angle(ZUN_PI / 2 - normalize_angle(angle - ZUN_PI / 2));
}

// FUNCTION: TH16 0x41dcb0
int EnemyData::ecl_run_over_300()
{
    EclRawInstr *instr = full->context.current_context->current_instr();
    switch ((i16)instr->opcode)
    {
    // enmCreateF and its variants: only without a boss.
    case 309:
    case 310:
    case 311:
    case 312:
        if (g_EnemyManager->get_boss(0) != NULL)
        {
            break;
        }
    // enmCreate, enmCreateA, enmCreateM, enmCreateAM, enmCreate321.
    case 300:
    case 301:
    case 304:
    case 305:
    case 321:
        ecl_enm_create();
        break;
    // anmSelect(file)
    case 302:
        selected_anm_index = get_int_arg(0);
        break;
    // moveSetMirror(mirrored)
    case 424:
        ((EnemyFlagsLow *)&flags_low)->mirrored = get_int_arg(0);
        break;
    // lifeHide(hide)
    case 631:
        g_EnemyManager->inner.boss_bit = get_int_arg(0);
        break;
    // zIndex(layer)
    case 552:
        anm_layers = get_int_arg(0);
        break;
    // hitSound(sound)
    case 553:
        hit_sound = get_int_arg(0);
        break;
    // scoreAdd(points)
    case 638:
    {
        i32 points = get_int_arg(0);
        g_Globals.add_to_score(points);
        g_PopupManager->generate_small_score_popup(&final_pos.pos, points, -1);
        break;
    }
    // anmSetSprite(slot, script)
    case 303:
        ecl_anm_set_sprite();
        break;
    // anmMove(slot, x, y): the offset of a slot's VM.
    case 320:
    {
        i32 slot = get_int_arg(0);
        anm_pos_array[slot].x = get_float_arg(1);
        anm_pos_array[slot].y = get_float_arg(2);
        anm_pos_array[slot].z = 0.0f;
        break;
    }
    // The instructions that change one of the enemy's VMs.
    case 319:
    case 325:
    case 326:
    case 327:
    case 328:
    case 329:
    case 330:
    case 331:
    case 332:
    case 333:
    case 335:
    case 336:
    case 337:
        ecl_anm_vm_instr();
        break;
    // unknown566: dies now.
    case 566:
        if (full->die())
        {
            return 1;
        }
        break;
    // enm323(file, script): the death animation.
    case 323:
        death_anm_index = get_int_arg(0);
        death_anm_script = get_int_arg(1);
        break;
    // die: the death sound and animation without dying.
    case 561:
        if (death_sound >= 0)
        {
            g_SoundManager.play_sound_at_position(death_sound, final_pos.pos.x);
        }
        if (death_anm_script >= 0)
        {
            f32 angle = -ZUN_PI / 2;
            Float3 *pos = &final_pos.pos;
            if (!(0.04f > (last_damage_pos.x - pos->x) * (last_damage_pos.x - pos->x) +
                              (last_damage_pos.y - pos->y) * (last_damage_pos.y - pos->y)))
            {
                angle = zun_atan2f(pos->y - last_damage_pos.y, pos->x - last_damage_pos.x);
            }
            g_EnemyManager->anim_statement_anms[death_anm_index]->create_vm(death_anm_script, pos, angle, 3, 0);
        }
        break;
    // enm322(index, value)
    case 322:
        unk_224[get_int_arg(0)] = get_int_arg(1);
        break;
    // stageLogo
    case 554:
        show_stage_logo();
        break;
    // anmPlayPos(file, script, x, y, rotation)
    case 338:
    {
        Float3 pos = final_pos.pos + Float3(get_float_arg(2), get_float_arg(3), 0.0f);
        g_EffectManager->track(create_vm_front_at(g_EnemyManager->anim_statement_anms[get_int_arg(0)], get_int_arg(1),
                                                  &pos, get_float_arg(4)));
        break;
    }
    // anmPlay(file, script)
    case 307:
        g_EffectManager->track(create_vm_front_at(g_EnemyManager->anim_statement_anms[get_int_arg(0)], get_int_arg(1),
                                                  &final_pos.pos, 0.0f));
        break;
    // anmPlayHigh(file, script)
    case 314:
        g_EffectManager->track(
            g_EnemyManager->anim_statement_anms[get_int_arg(0)]->create_vm(get_int_arg(1), &final_pos.pos, 0.0f, -1, 0));
        break;
    // anmPlayAbs(file, script)
    case 308:
    {
        i32 file = get_int_arg(0);
        g_EffectManager->track(g_EnemyManager->anim_statement_anms[file]->create_vm_front(get_int_arg(1), -1, 0));
        break;
    }
    // anm339(file, script, frames): anmPlayAbs, then runs the VM for some
    // frames at once.
    case 339:
    {
        i32 file = get_int_arg(0);
        AnmId id = g_EnemyManager->anim_statement_anms[file]->create_vm_front(get_int_arg(1), -1, 0);
        g_EffectManager->track(id);
        AnmVm *vm = id.find_or_clear();
        for (i32 i = 0; i < get_int_arg(2); i++)
        {
            vm->run();
        }
        break;
    }
    // anmPlayRotate(file, script, rotation)
    case 315:
    {
        AnmId id = create_vm_front_at(g_EnemyManager->anim_statement_anms[get_int_arg(0)], get_int_arg(1),
                                      &final_pos.pos, 0.0f);
        AnmVm *vm = id.find_or_clear();
        if (vm != NULL)
        {
            vm->entity_pos = final_pos.pos;
            vm->rotation.z = get_float_arg(2);
            vm->flags_lo |= ANM_VM_ROTATION_CHANGED;
        }
        g_EffectManager->track(id);
        break;
    }
    // anm334(effect)
    case 334:
        g_EffectManager->create_tracked(get_int_arg(0), &final_pos.pos, 0);
        break;
    // anmSetMain(slot, script)
    case 306:
    {
        i32 slot = get_int_arg(0);
        i32 script = get_int_arg(1);
        delete_vm_and_clear(anm_ids[slot]);
        anm_ids[slot] =
            g_EnemyManager->anim_statement_anms[selected_anm_index]->create_vm_front(script, anm_layers + 7, 0);
        AnmVm *vm = anm_ids[slot].find_or_clear();
        if (slot == 0)
        {
            final_sprite_size.x = vm->scale.y * vm->sprite_size.y;
            final_sprite_size.y = vm->scale.x * vm->sprite_size.x;
        }
        if (flags_low & 0x20)
        {
            anm_ids[slot].clear_flag_lo_2_tree();
        }
        if (slot == 0)
        {
            flags_low |= 0x100000;
            anm_slot_0_script = anm_set_main = script;
            unk_274 = 0;
            anm_slot_0_anm_index = selected_anm_index;
        }
        break;
    }
    // anmReset: back to the main script in slot 0.
    case 318:
        delete_vm_and_clear(anm_ids[0]);
        anm_ids[0] =
            g_EnemyManager->anim_statement_anms[selected_anm_index]->create_vm_front(anm_set_main, anm_layers + 7, 0);
        flags_low &= ~0x100000;
        anm_slot_0_script = anm_set_main;
        unk_274 = 0;
        anm_slot_0_anm_index = selected_anm_index;
        break;
    // anmSelectedPlay(slot)
    case 313:
    {
        i32 slot = get_int_arg(0);
        delete_vm_and_clear(anm_ids[slot]);
        anm_ids[slot] = g_EnemyManager->anim_statement_anms[selected_anm_index]->create_vm_front(anm_set_main + 5,
                                                                                                 anm_layers + 7, 0);
        AnmVm *vm = anm_ids[slot].find_or_clear();
        if (slot == 0)
        {
            final_sprite_size.x = vm->scale.y * vm->sprite_size.y;
            final_sprite_size.y = vm->scale.x * vm->sprite_size.x;
        }
        if (flags_low & 0x20)
        {
            anm_ids[slot].clear_flag_lo_2_tree();
        }
        break;
    }
    // anm316(slot, n): script n + 5 of the main set, or the main script
    // itself for negative n.
    case 316:
    {
        i32 slot = get_int_arg(0);
        i32 n = get_int_arg(1);
        delete_vm_and_clear(anm_ids[slot]);
        anm_ids[slot] = g_EnemyManager->anim_statement_anms[selected_anm_index]->create_vm_front(
            n >= 0 ? anm_set_main + n + 5 : anm_set_main, anm_layers + 7, 0);
        AnmVm *vm = anm_ids[slot].find_or_clear();
        if (slot == 0)
        {
            final_sprite_size.x = vm->scale.y * vm->sprite_size.y;
            final_sprite_size.y = vm->scale.x * vm->sprite_size.x;
        }
        if (flags_low & 0x20)
        {
            anm_ids[slot].clear_flag_lo_2_tree();
        }
        break;
    }
    // anmSwitch(slot, interrupt)
    case 317:
    {
        i32 slot = get_int_arg(0);
        AnmManager::interrupt_tree(anm_ids[slot], (i16)get_int_arg(1));
        break;
    }
    // moveReset: folds rel_pos into abs_pos and stops all movement.
    case 427:
        abs_pos.pos += rel_pos.pos;
        rel_pos.pos = g_zero_vec;
        rel_pos.speed = 0.0f;
        abs_pos.speed = 0.0f;
        rel_pos.velocity = g_zero_vec;
        abs_pos.velocity = g_zero_vec;
        rel_pos.flags &= ~0xf;
        abs_pos.flags &= ~0xf;
        abs_pos_i.end_time = 0;
        rel_pos_i.end_time = 0;
        abs_angle_i.end_time = 0;
        rel_angle_i.end_time = 0;
        abs_speed_i.end_time = 0;
        rel_speed_i.end_time = 0;
        abs_radial_dist_i.end_time = 0;
        rel_radial_dist_i.end_time = 0;
        abs_ellipse_i.end_time = 0;
        rel_ellipse_i.end_time = 0;
        break;
    // movePos(x, y), movePosRel(x, y): -999999 keeps a coordinate.
    case 400:
    case 402:
    {
        PosVel *pos_vel = instr->opcode == 400 ? &abs_pos : &rel_pos;
        f32 x = get_float_arg(0);
        f32 y = get_float_arg(1);
        if (x > -999999.0)
        {
            pos_vel->pos.x = x;
        }
        if (y > -999999.0)
        {
            pos_vel->pos.y = y;
        }
        pos_vel->flags &= ~0xf;
        final_pos.pos = rel_pos.pos + abs_pos.pos;
        update_final_pos();
        break;
    }
    // movePos3d(x, y, z), movePos3dRel(x, y, z): moves by the offset.
    case 416:
    case 417:
    {
        PosVel *pos_vel = instr->opcode == 416 ? &abs_pos : &rel_pos;
        f32 x = get_float_arg(0);
        f32 y = get_float_arg(1);
        f32 z = get_float_arg(2);
        pos_vel->pos.x += x;
        pos_vel->pos.y += y;
        pos_vel->pos.z += z;
        update_final_pos();
        break;
    }
    // movePosTime(time, mode, x, y) and movePosRelTime; 436/437 move by an
    // offset (mirrored with the enemy).
    case 401:
    case 403:
    case 436:
    case 437:
    {
        PosVel *pos_vel = instr->opcode == 401 || instr->opcode == 436 ? &abs_pos : &rel_pos;
        InterpStrange1 *interp = instr->opcode == 401 || instr->opcode == 436 ? &abs_pos_i : &rel_pos_i;
        f32 x = get_float_arg(2);
        f32 y = get_float_arg(3);
        if (get_int_arg(0) <= 0)
        {
            interp->end_time = 0;
            break;
        }
        if (instr->opcode == 436 || instr->opcode == 437)
        {
            x = flags_low & 0x80000 ? pos_vel->pos.x - x : pos_vel->pos.x + x;
            y = pos_vel->pos.y + y;
        }
        interp->end_time = get_int_arg(0);
        interp->bezier_1 = g_zero_vec;
        interp->bezier_2 = g_zero_vec;
        interp->method_for_3d = get_int_arg(1);
        interp->flag_1d &= ~1;
        interp->initial = pos_vel->pos;
        interp->goal = Float3(x > -999999.0 ? x : pos_vel->pos.x, y > -999999.0 ? y : pos_vel->pos.y, 0.0f);
        interp->reset_timer();
        pos_vel->flags &= ~0xf;
        break;
    }
    // moveCurve(time, mode_x, mode_y, x, y) and its variants: separate
    // modes for x and y.
    case 434:
    case 435:
    case 438:
    case 439:
    {
        PosVel *pos_vel = instr->opcode == 434 || instr->opcode == 438 ? &abs_pos : &rel_pos;
        InterpStrange1 *interp = instr->opcode == 434 || instr->opcode == 438 ? &abs_pos_i : &rel_pos_i;
        f32 x = get_float_arg(3);
        f32 y = get_float_arg(4);
        if (get_int_arg(0) <= 0)
        {
            interp->end_time = 0;
            break;
        }
        if (instr->opcode == 438 || instr->opcode == 439)
        {
            x = flags_low & 0x80000 ? pos_vel->pos.x - x : pos_vel->pos.x + x;
            y = pos_vel->pos.y + y;
        }
        interp->end_time = get_int_arg(0);
        interp->bezier_1 = g_zero_vec;
        interp->bezier_2 = g_zero_vec;
        interp->method_for_1d = get_int_arg(1);
        interp->flag_1d |= 1;
        interp->move_curve_mode = get_int_arg(2);
        interp->flag_1d |= 1;
        interp->initial = pos_vel->pos;
        interp->goal = Float3(x > -999999.0 ? x : pos_vel->pos.x, y > -999999.0 ? y : pos_vel->pos.y, 0.0f);
        interp->reset_timer();
        pos_vel->flags &= ~0xf;
        break;
    }
    // moveBezier(time, b1x, b1y, x, y, b2x, b2y), moveBezierRel
    case 425:
    case 426:
    {
        PosVel *pos_vel = instr->opcode == 425 ? &abs_pos : &rel_pos;
        InterpStrange1 *interp = instr->opcode == 425 ? &abs_pos_i : &rel_pos_i;
        f32 x = get_float_arg(3);
        f32 y = get_float_arg(4);
        Float3 bezier_1;
        bezier_1.x = get_float_arg(1);
        bezier_1.y = get_float_arg(2);
        bezier_1.z = 0.0f;
        Float3 bezier_2;
        bezier_2.x = get_float_arg(5);
        bezier_2.y = get_float_arg(6);
        bezier_2.z = 0.0f;
        interp->end_time = get_int_arg(0);
        interp->bezier_1 = bezier_1;
        interp->bezier_2 = bezier_2;
        interp->method_for_3d = INTERP_BEZIER;
        interp->flag_1d &= ~1;
        interp->initial = pos_vel->pos;
        interp->goal = Float3(x > -999999.0 ? x : pos_vel->pos.x, y > -999999.0 ? y : pos_vel->pos.y, 0.0f);
        interp->reset_timer();
        pos_vel->flags &= ~0xf;
        break;
    }
    // 404, 406, 428, 430: moveVel(angle, speed) and the relative and
    // no-mirror (428, 430) forms. -999999 keeps the current value.
    case 404:
    case 406:
    case 428:
    case 430:
    {
        PosVel *pv = instr->opcode == 404 || instr->opcode == 428 ? &abs_pos : &rel_pos;
        f32 angle = get_float_arg(0);
        f32 speed = get_float_arg(1);
        if (angle > -999999.0)
        {
            if ((flags_low & 0x80000) && (instr->opcode == 404 || instr->opcode == 406))
            {
                angle = mirror_angle(angle);
            }
            pv->set_angle(angle);
        }
        if (speed > -999999.0)
        {
            pv->speed = speed;
        }
        pv->flags &= ~0xf;
        break;
    }
    // 405, 407, 429, 431: moveVelTime(time, mode, angle, speed). With mode 7
    // (constant velocity) angle and speed are per-frame changes instead.
    case 405:
    case 407:
    case 429:
    case 431:
    {
        PosVel *pv = instr->opcode == 405 || instr->opcode == 429 ? &abs_pos : &rel_pos;
        InterpFloat *angle_i = instr->opcode == 405 || instr->opcode == 429 ? &abs_angle_i : &rel_angle_i;
        InterpFloat *speed_i = instr->opcode == 405 || instr->opcode == 429 ? &abs_speed_i : &rel_speed_i;
        f32 angle = get_float_arg(2);
        f32 speed = get_float_arg(3);
        if (get_int_arg(0) <= 0)
        {
            angle_i->end_time = 0;
            speed_i->end_time = 0;
            break;
        }
        i32 mode = get_int_arg(1);
        if (mode != INTERP_CONSTANT_VELOCITY)
        {
            if (angle > -999999.0)
            {
                if ((flags_low & 0x80000) && (instr->opcode == 405 || instr->opcode == 407))
                {
                    angle = mirror_angle(angle);
                }
            }
            else
            {
                angle = pv->angle.value;
            }
            speed = speed > -999999.0 ? speed : pv->speed;
        }
        else
        {
            if (angle > -999999.0)
            {
                if ((flags_low & 0x80000) && (instr->opcode == 405 || instr->opcode == 407))
                {
                    angle = -angle;
                }
            }
            else
            {
                angle = 0.0f;
            }
            speed = speed > -999999.0 ? speed : 0.0f;
        }
        f32 cur_speed = pv->speed;
        f32 cur_angle = pv->angle.value;
        // Turn the short way round.
        if (zun_fabsf(cur_angle - angle) >= ZUN_PI)
        {
            if (angle > cur_angle)
            {
                cur_angle += ZUN_2PI;
            }
            else
            {
                angle += ZUN_2PI;
            }
        }
        angle_i->start(get_int_arg(0), mode, cur_angle, angle);
        speed_i->start(get_int_arg(0), mode, cur_speed, speed);
        pv->flags &= ~0xf;
        break;
    }
    // 440, 442: moveAngle(angle). Mirrored whenever the flag is set.
    case 440:
    case 442:
    {
        PosVel *pv = instr->opcode == 440 ? &abs_pos : &rel_pos;
        f32 angle = get_float_arg(0);
        if (flags_low & 0x80000)
        {
            angle = mirror_angle(angle);
        }
        pv->set_angle(angle);
        pv->flags &= ~0xf;
        break;
    }
    // 441, 443: moveAngleTime(time, mode, angle).
    case 441:
    case 443:
    {
        PosVel *pv = instr->opcode == 441 ? &abs_pos : &rel_pos;
        InterpFloat *angle_i = instr->opcode == 441 ? &abs_angle_i : &rel_angle_i;
        f32 angle = get_float_arg(2);
        if (get_int_arg(0) <= 0)
        {
            angle_i->end_time = 0;
            break;
        }
        i32 mode = get_int_arg(1);
        if (mode != INTERP_CONSTANT_VELOCITY)
        {
            if (angle > -999999.0)
            {
                if ((flags_low & 0x80000) && (instr->opcode == 441 || instr->opcode == 443))
                {
                    angle = mirror_angle(angle);
                }
            }
            else
            {
                angle = pv->angle.value;
            }
        }
        else
        {
            if (angle > -999999.0)
            {
                if ((flags_low & 0x80000) && (instr->opcode == 441 || instr->opcode == 443))
                {
                    angle = -angle;
                }
            }
            else
            {
                angle = 0.0f;
            }
        }
        f32 initial = pv->angle.value;
        // The goal as an offset from the current angle, the short way round.
        f32 delta;
        if (angle - initial > ZUN_PI)
        {
            delta = angle - (initial + ZUN_2PI);
        }
        else if (initial - angle > ZUN_PI)
        {
            delta = angle - (initial - ZUN_2PI);
        }
        else
        {
            delta = angle - initial;
        }
        angle_i->start(get_int_arg(0), mode, initial, delta + initial);
        pv->flags &= ~0xf;
        break;
    }
    // 444, 446: moveSpeed(speed).
    case 444:
    case 446:
    {
        PosVel *pv = instr->opcode == 444 ? &abs_pos : &rel_pos;
        f32 speed = get_float_arg(0);
        if (speed > -999999.0)
        {
            pv->speed = speed;
        }
        pv->flags &= ~0xf;
        break;
    }
    // 445, 447: moveSpeedTime(time, mode, speed).
    case 445:
    case 447:
    {
        PosVel *pv = instr->opcode == 445 ? &abs_pos : &rel_pos;
        InterpFloat *speed_i = instr->opcode == 445 ? &abs_speed_i : &rel_speed_i;
        f32 speed = get_float_arg(2);
        f32 initial = pv->speed;
        if (get_int_arg(0) <= 0)
        {
            speed_i->end_time = 0;
            break;
        }
        i32 mode = get_int_arg(1);
        if (mode != INTERP_CONSTANT_VELOCITY)
        {
            speed = speed > -999999.0 ? speed : pv->speed;
        }
        else
        {
            speed = speed > -999999.0 ? speed : 0.0f;
        }
        speed_i->start(get_int_arg(0), mode, initial, speed);
        pv->flags &= ~0xf;
        break;
    }
    // 408, 410: moveCircle(angle, angular speed, radius, radial speed),
    // around the current position unless already circling.
    case 408:
    case 410:
    {
        PosVel *pv = instr->opcode == 408 ? &abs_pos : &rel_pos;
        f32 angle = get_float_arg(0);
        f32 speed = get_float_arg(1);
        f32 radius = get_float_arg(2);
        f32 radial_speed = get_float_arg(3);
        if ((pv->flags & 0xf) != POSVEL_MODE_CIRCLE)
        {
            pv->velocity = pv->pos;
        }
        if (angle > -999999.0)
        {
            pv->set_angle(angle);
        }
        if (speed > -999999.0)
        {
            pv->speed = speed;
        }
        if (radius > -999999.0)
        {
            pv->radial_dist = radius;
        }
        if (radial_speed > -999999.0)
        {
            pv->radial_speed = radial_speed;
        }
        pv->flags = pv->flags & ~0xf | POSVEL_MODE_CIRCLE;
        pv->step_from_center();
        update_final_pos();
        break;
    }
    // 409, 411: moveCircleTime(time, mode, angular speed, radius, radial
    // speed).
    case 409:
    case 411:
    {
        PosVel *pv = instr->opcode == 409 ? &abs_pos : &rel_pos;
        InterpFloat *speed_i = instr->opcode == 409 ? &abs_speed_i : &rel_speed_i;
        InterpFloat2 *radial_i = instr->opcode == 409 ? &abs_radial_dist_i : &rel_radial_dist_i;
        f32 speed = get_float_arg(2);
        f32 radius = get_float_arg(3);
        f32 radial_speed = get_float_arg(4);
        speed = speed > -999999.0 ? speed : pv->speed;
        f32 initial_speed = pv->speed;
        D3DXVECTOR2 radial_goal(radius > -999999.0 ? radius : pv->radial_dist,
                                radial_speed > -999999.0 ? radial_speed : pv->radial_speed);
        D3DXVECTOR2 radial_initial(pv->radial_dist, pv->radial_speed);
        i32 time = get_int_arg(0);
        i32 mode = get_int_arg(1);
        if (time <= 0)
        {
            speed_i->end_time = 0;
            radial_i->end_time = 0;
            break;
        }
        speed_i->start(time, mode, initial_speed, speed);
        radial_i->start(time, mode, &radial_initial, &radial_goal);
        pv->flags = pv->flags & ~0xf | POSVEL_MODE_CIRCLE;
        pv->step_from_center();
        update_final_pos();
        break;
    }
    // 418, 419 (ExpHP: moveAdd): sets the center of circular and elliptic
    // movement.
    case 418:
    case 419:
    {
        PosVel *pv = instr->opcode == 418 ? &abs_pos : &rel_pos;
        f32 x = get_float_arg(0);
        f32 y = get_float_arg(1);
        if (x > -999999.0)
        {
            pv->velocity.x = x;
        }
        if (y > -999999.0)
        {
            pv->velocity.y = y;
        }
        pv->step_from_center();
        update_final_pos();
    }
    // No break in the original: 418 and 419 go on into moveEllipse.
    // 420, 422: moveEllipse(angle, angular speed, radius, radial speed,
    // ellipse angle, ellipse ratio).
    case 420:
    case 422:
    {
        PosVel *pv = instr->opcode == 420 ? &abs_pos : &rel_pos;
        f32 angle = get_float_arg(0);
        f32 speed = get_float_arg(1);
        f32 radius = get_float_arg(2);
        f32 radial_speed = get_float_arg(3);
        f32 ellipse_angle = get_float_arg(4);
        f32 ellipse_ratio = get_float_arg(5);
        if ((pv->flags & 0xf) != POSVEL_MODE_CIRCLE)
        {
            pv->velocity = pv->pos;
        }
        if (angle > -999999.0)
        {
            pv->set_angle(angle);
        }
        if (speed > -999999.0)
        {
            pv->speed = speed;
        }
        if (radius > -999999.0)
        {
            pv->radial_dist = radius;
        }
        if (radial_speed > -999999.0)
        {
            pv->radial_speed = radial_speed;
        }
        if (ellipse_angle > -999999.0)
        {
            pv->set_ellipse_angle(ellipse_angle);
        }
        if (ellipse_ratio > -999999.0)
        {
            pv->ellipse_ratio = ellipse_ratio;
        }
        pv->flags = pv->flags & ~0xf | POSVEL_MODE_ELLIPSE;
        pv->step_from_center();
        update_final_pos();
        break;
    }
    // 421, 423: moveEllipseTime(time, mode, angular speed, radius, radial
    // speed, ellipse angle, ellipse ratio). Always restarts around the
    // current position.
    case 421:
    case 423:
    {
        PosVel *pv = instr->opcode == 421 ? &abs_pos : &rel_pos;
        InterpFloat *speed_i = instr->opcode == 421 ? &abs_speed_i : &rel_speed_i;
        InterpFloat2 *radial_i = instr->opcode == 421 ? &abs_radial_dist_i : &rel_radial_dist_i;
        InterpFloat2 *ellipse_i = instr->opcode == 421 ? &abs_ellipse_i : &rel_ellipse_i;
        f32 speed = get_float_arg(2);
        f32 radius = get_float_arg(3);
        f32 radial_speed = get_float_arg(4);
        f32 ellipse_angle = get_float_arg(5);
        f32 ellipse_ratio = get_float_arg(6);
        speed = speed > -999999.0 ? speed : pv->speed;
        f32 initial_speed = pv->speed;
        D3DXVECTOR2 radial_goal(radius > -999999.0 ? radius : pv->radial_dist,
                                radial_speed > -999999.0 ? radial_speed : pv->radial_speed);
        D3DXVECTOR2 radial_initial(pv->radial_dist, pv->radial_speed);
        D3DXVECTOR2 ellipse_goal(ellipse_angle > -999999.0 ? ellipse_angle : pv->ellipse_angle.value,
                                 ellipse_ratio > -999999.0 ? ellipse_ratio : pv->ellipse_ratio);
        D3DXVECTOR2 ellipse_initial(pv->ellipse_angle.value, pv->ellipse_ratio);
        i32 time = get_int_arg(0);
        i32 mode = get_int_arg(1);
        if (time <= 0)
        {
            speed_i->end_time = 0;
            radial_i->end_time = 0;
            ellipse_i->end_time = 0;
            break;
        }
        speed_i->start(time, mode, initial_speed, speed);
        radial_i->start(time, mode, &radial_initial, &radial_goal);
        ellipse_i->start(time, mode, &ellipse_initial, &ellipse_goal);
        pv->velocity = pv->pos;
        pv->flags = pv->flags & ~0xf | POSVEL_MODE_ELLIPSE;
        pv->step_from_center();
        update_final_pos();
        break;
    }
    // 414: moveBoss(): jumps to the boss.
    case 414:
        abs_pos.pos = g_EnemyManager->get_boss(0)->enemy.final_pos.pos;
        break;
    // 415: moveBossRel().
    case 415:
        rel_pos.pos = g_EnemyManager->get_boss(0)->enemy.final_pos.pos;
        break;
    // 432: moveEnm(enemy id): jumps to another enemy.
    case 432:
        abs_pos.pos = g_EnemyManager->find_enemy_by_id(get_int_arg(0))->enemy.final_pos.pos;
        break;
    // 433: moveEnmRel(enemy id).
    case 433:
        rel_pos.pos = g_EnemyManager->find_enemy_by_id(get_int_arg(0))->enemy.final_pos.pos;
        break;
    // 504: moveLimit(x, y, width, height): keeps final_pos inside the
    // rectangle (see update_final_pos).
    case 504:
        flags_low |= 0x20000;
        move_limit_center.x = get_float_arg(0);
        move_limit_center.y = get_float_arg(1);
        move_limit_size.x = get_float_arg(2);
        move_limit_size.y = get_float_arg(3);
        break;
    // 505: moveLimitReset().
    case 505:
        flags_low &= ~0x20000;
        break;
    // 526: etProtectRange(radius), kept squared.
    case 526:
    {
        f32 range = get_float_arg(0);
        et_protect_range = range * range;
        break;
    }

    // @@FRAG_C@@
    // @@FRAG_D@@
    // laserOffset(id, x, y)
    case 704:
    {
        LaserDataInf *laser = g_LaserManager->find_by_id(get_int_arg(0), 0);
        Float3 offset;
        offset.x = get_float_arg(1);
        offset.y = get_float_arg(2);
        offset.z = 0.0f;
        if (laser != NULL)
        {
            laser->position = offset;
        }
        break;
    }
    // laserTrajectory(id, x, y): infinite lasers only.
    case 705:
    {
        LaserDataInf *laser = g_LaserManager->find_by_id(get_int_arg(0), 0);
        Float3 trajectory;
        trajectory.x = get_float_arg(1);
        trajectory.y = get_float_arg(2);
        trajectory.z = 0.0f;
        if (laser != NULL)
        {
            ((LaserInfiniteInf *)laser)->inner.trajectory = trajectory;
        }
        break;
    }
    // laserStLength(id, length)
    case 706:
    {
        LaserDataInf *laser = g_LaserManager->find_by_id(get_int_arg(0), 0);
        if (laser != NULL)
        {
            laser->length = get_float_arg(1);
        }
        break;
    }
    // laserStWidth(id, width)
    case 707:
    {
        LaserDataInf *laser = g_LaserManager->find_by_id(get_int_arg(0), 0);
        if (laser != NULL)
        {
            laser->width = get_float_arg(1);
        }
        break;
    }
    // laserStAngle(id, angle)
    case 708:
    {
        LaserDataInf *laser = g_LaserManager->find_by_id(get_int_arg(0), 0);
        if (laser != NULL)
        {
            laser->angle = get_float_arg(1);
        }
        break;
    }
    // laserStRotation(id, rotation): infinite lasers only.
    case 709:
    {
        LaserDataInf *laser = g_LaserManager->find_by_id(get_int_arg(0), 0);
        if (laser != NULL)
        {
            ((LaserInfiniteInf *)laser)->inner.laser_st_rotation = get_float_arg(1);
        }
        break;
    }
    // unknown714(id, value): laser method_8.
    case 714:
    {
        LaserDataInf *laser = g_LaserManager->find_by_id(get_int_arg(0), 0);
        if (laser != NULL)
        {
            laser->method_8(get_int_arg(1));
        }
        break;
    }
    // etCancel(radius): cancels bullets and lasers around the enemy into
    // items.
    case 615:
    {
        f32 radius = get_float_arg(0);
        g_BulletManager->cancel_radius(&final_pos.pos, radius, 1);
        g_LaserManager->cancel_in_radius(&final_pos.pos, radius, 1, 1);
        break;
    }
    // hitboxRect(w, h): a rectangular bullet-cancelling hitbox, rotated like
    // the enemy's main sprite.
    case 712:
    {
        Float3 size;
        size.x = get_float_arg(0);
        size.y = get_float_arg(1);
        g_BulletManager->cancel_rectangle_as_bomb(&final_pos.pos, &size, anm_ids[0].find_or_clear()->rotation.z, 1);
        break;
    }
    // etClear(radius): the same without items.
    case 616:
    {
        f32 radius = get_float_arg(0);
        g_BulletManager->cancel_radius(&final_pos.pos, radius, 0);
        g_LaserManager->cancel_in_radius(&final_pos.pos, radius, 0, 1);
        break;
    }
    // etCancel2(radius): etCancel that also takes protected bullets.
    case 635:
    {
        f32 radius = get_float_arg(0);
        g_BulletManager->cancel_radius_as_bomb(&final_pos.pos, radius, 1);
        g_LaserManager->cancel_in_radius(&final_pos.pos, radius, 1, 1);
        break;
    }
    // etClear2(radius)
    case 636:
    {
        f32 radius = get_float_arg(0);
        g_BulletManager->cancel_radius_as_bomb(&final_pos.pos, radius, 0);
        g_LaserManager->cancel_in_radius(&final_pos.pos, radius, 0, 1);
        break;
    }
    // setChapter(chapter)
    case 524:
    {
        i32 chapter = get_int_arg(0);
        g_Globals.chapter = chapter;
        g_GameThread->chapter = chapter;
        own_chapter = chapter;
        unk_452c = 0;
        break;
    }
    // rankF3(var, a, b, c): only two tiers survive in the binary.
    case 529:
        if (g_Globals.rank >= 512)
        {
            *get_float_arg_ptr(0) = get_float_arg(2);
        }
        else
        {
            *get_float_arg_ptr(0) = get_float_arg(0);
        }
        break;
    // rankF5(var, a, b, c, d, e)
    case 530:
        if (g_Globals.rank >= 600)
        {
            *get_float_arg_ptr(0) = get_float_arg(4);
        }
        else if (g_Globals.rank >= 200)
        {
            *get_float_arg_ptr(0) = get_float_arg(3);
        }
        else if (g_Globals.rank >= -200)
        {
            *get_float_arg_ptr(0) = get_float_arg(2);
        }
        else if (g_Globals.rank >= -400)
        {
            *get_float_arg_ptr(0) = get_float_arg(1);
        }
        else
        {
            *get_float_arg_ptr(0) = get_float_arg(0);
        }
        break;
    // rankF2(var, low, high): low at rank -1024, high at rank 1024.
    case 531:
    {
        f32 low = get_float_arg(1);
        f32 high = get_float_arg(2);
        *get_float_arg_ptr(0) = (high - low) * (g_Globals.rank + 1024.0f) / 2048.0f + low;
        break;
    }
    // rankI3(var, a, b, c)
    case 532:
        if (g_Globals.rank >= 512)
        {
            *get_int_arg_ptr(0) = get_int_arg(2);
        }
        else
        {
            *get_int_arg_ptr(0) = get_int_arg(0);
        }
        break;
    // rankI5(var, a, b, c, d, e)
    case 533:
        if (g_Globals.rank >= 600)
        {
            *get_int_arg_ptr(0) = get_int_arg(4);
        }
        else if (g_Globals.rank >= 200)
        {
            *get_int_arg_ptr(0) = get_int_arg(3);
        }
        else if (g_Globals.rank >= -200)
        {
            *get_int_arg_ptr(0) = get_int_arg(2);
        }
        else if (g_Globals.rank >= -400)
        {
            *get_int_arg_ptr(0) = get_int_arg(1);
        }
        else
        {
            *get_int_arg_ptr(0) = get_int_arg(0);
        }
        break;
    // rankI2(var, low, high)
    case 534:
    {
        i32 low = get_int_arg(1);
        i32 high = get_int_arg(2);
        *get_int_arg_ptr(0) = (high - low) * (g_Globals.rank + 1024) / 2048 + low;
        break;
    }
    // stars(count): the boss's star count on the HUD.
    case 540:
        g_Gui->boss_star_count = get_int_arg(0);
        break;
    // reset: cancels every laser.
    case 545:
        g_LaserManager->cancel_all();
        break;
    // bombShield(on, anm_script)
    case 546:
        ((EnemyFlagsLow *)&flags_low)->bombshield = get_int_arg(0);
        bombshield_on_anm_main = get_int_arg(1);
        flags_low &= ~0x20000001;
        bombshield_off_anm_main = anm_set_main;
        break;
    // unknown559(limit): the enemy limit.
    case 559:
        g_EnemyManager->inner.enemy_limit = get_int_arg(0);
        break;
    // gameSpeed(speed)
    case 547:
        g_game_speed = get_float_arg(0);
        break;
    // angleToPlayer(var, x, y)
    case 623:
        *get_float_arg_ptr(0) =
            zun_atan2f(g_Player->inner.pos.y - get_float_arg(2), g_Player->inner.pos.x - get_float_arg(1));
        break;
    // diffWait(easy, normal, hard, lunatic): waits the given number of
    // frames for the current difficulty.
    case 548:
    {
        i32 easy = get_int_arg(0);
        i32 normal = get_int_arg(1);
        i32 hard = get_int_arg(2);
        i32 lunatic = get_int_arg(3);
        i32 wait;
        if (g_Globals.difficulty == DIFFICULTY_EASY)
        {
            wait = easy;
        }
        else if (g_Globals.difficulty == DIFFICULTY_NORMAL)
        {
            wait = normal;
        }
        else if (g_Globals.difficulty == DIFFICULTY_HARD)
        {
            wait = hard;
        }
        else
        {
            wait = lunatic;
        }
        full->context.current_context->time -= wait;
        break;
    }
    // fog(radius, color): replaces the enemy's fog; radius 0 removes it.
    case 629:
        if (fog.fog_ptr != NULL)
        {
            delete (Fog *)fog.fog_ptr;
        }
        fog.fog_ptr = NULL;
        fog.fog_radius = get_float_arg(0);
        fog.unk_c = 16.0f;
        fog.fog_color = get_int_arg(1);
        *(ZunAngle *)&fog.unk_14 = 0.0f;
        *(ZunAngle *)&fog.unk_18 = 0.0f;
        if (fog.fog_radius > 0.0f)
        {
            fog.fog_ptr = new Fog(0, 0x11, 0);
        }
        break;
    // callSTD(label): jumps the stage script.
    case 630:
        g_Stage->jump_to_label(get_int_arg(0));
        break;
    // unknown557(time, method, color, begin, end): interpolates the stage
    // fog (camera sky) towards the given distances and color.
    case 557:
    {
        i32 time = get_int_arg(0);
        i32 method = get_int_arg(1);
        ZunColor color;
        color.d3d = get_int_arg(2);
        CameraSky sky(get_float_arg(3), get_float_arg(4), color.b, color.g, color.r, color.a);
        g_Stage->inner.set_sky_interp(time, method, &sky);
        break;
    }
    // unknown549(flag)
    case 549:
        ((EnemyFlagsLow *)&flags_low)->unk_31 = get_int_arg(0);
        break;
    // unknown550(value)
    case 550:
        unk_278 = get_int_arg(0);
        break;
    // enmAlive(var, id)
    case 555:
        *get_int_arg_ptr(0) = g_EnemyManager->is_enemy_alive(get_int_arg(1));
        break;
    // enmPos(var_x, var_y, id)
    case 801:
    {
        EnemyInf *enemy = g_EnemyManager->find_enemy_by_id(get_int_arg(2));
        *get_float_arg_ptr(0) = enemy->enemy.final_pos.pos.x;
        *get_float_arg_ptr(1) = enemy->enemy.final_pos.pos.y;
        break;
    }
    // unknown802(index): makes the other bosses jump to their interrupt
    // index's subroutine, if it has a life threshold.
    case 802:
    {
        i32 index = get_int_arg(0);
        for (i32 i = 0; i < 3; i++)
        {
            EnemyInf *boss = g_EnemyManager->get_boss(i);
            if (i != own_boss_id && boss != NULL && boss->enemy.interrupts[index].life >= 0)
            {
                char *sub = boss->enemy.interrupts[index].sub_for_set_next;
                if (sub != NULL)
                {
                    boss->free_all_async();
                    boss->reset_run_context();
                    boss->load_sub_by_name(sub);
                }
            }
        }
        break;
    }
    // enmCall(id, sub): restarts another enemy at the named subroutine.
    case 800:
    {
        EnemyInf *enemy = g_EnemyManager->find_enemy_by_id(get_int_arg(0));
        if (enemy != NULL)
        {
            enemy->free_all_async();
            enemy->reset_run_context();
            enemy->load_sub_by_name((const char *)&instr->args[2]);
        }
        break;
    }
    // enm324(var_x, var_y, id): enmPos that falls back to this enemy.
    case 324:
    {
        EnemyInf *enemy = g_EnemyManager->find_enemy_by_id(get_int_arg(2));
        *get_float_arg_ptr(0) = enemy != NULL ? enemy->enemy.final_pos.pos.x : final_pos.pos.x;
        *get_float_arg_ptr(1) = enemy != NULL ? enemy->enemy.final_pos.pos.y : final_pos.pos.y;
        break;
    }
    // unknown560(x, y)
    case 560:
        g_BulletManager->ecl_unknown_560.x = get_float_arg(0);
        g_BulletManager->ecl_unknown_560.y = get_float_arg(1);
        break;
    // spec0(max_time, count, min_count): the season item drop.
    case 1000:
        drop_season.max_time = get_int_arg(0);
        drop_season.bonus_timer.set_value(drop_season.max_time);
        drops.extra_counts[15] = get_int_arg(1);
        drop_season.min_count = get_int_arg(2);
        break;
    // spec1(damage_per_drop)
    case 1001:
        drop_season.damage_per_season_drop = get_int_arg(0);
        drop_season.damage_accounted_for_season_drops = life.total_damage_including_ignored;
        break;
    // anm340(id): sets flag 0x2000000 on another enemy.
    case 340:
    {
        EnemyInf *enemy = g_EnemyManager->find_enemy_by_id(get_int_arg(0));
        if (enemy != NULL)
        {
            enemy->enemy.flags_low |= 0x2000000;
        }
        break;
    }

    }
    return 0;
}
