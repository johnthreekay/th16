// EnemyData's per-frame fog update.
#include <math.h>

#include "Enemy.h"
#include "Fog.h"
#include "Supervisor.h"
#include "ZunMath.h"

// Grows the fog towards fog_radius, rebuilds its mesh around final_pos
// and turns the wave angles.
// FUNCTION: TH16 0x41cbd0
void EnemyData::update_fog()
{
    Fog *mesh = (Fog *)fog.fog_ptr;
    if (mesh == NULL)
    {
        return;
    }
    D3DXVECTOR3 *points = (D3DXVECTOR3 *)mesh->points;
    f32 radius = fog.cur_radius;
    // The angles are ZunAngles in ZUN's struct: copied as such.
    ZunAngle angle_x = *(ZunAngle *)&fog.wave_angle_x;
    ZunAngle angle_y = *(ZunAngle *)&fog.wave_angle_y;
    if (fog.fog_radius > radius)
    {
        fog.cur_radius = g_game_speed * 2.0f + radius;
    }
    Float3 pos = final_pos.pos;
    mesh->set_rect(pos.x - radius - 20.0f, pos.y - radius - 20.0f, radius + radius + 40.0f, radius + radius + 40.0f);
    f32 center_x = g_resolution_x * 0.5f + pos.x;
    f32 center_y = g_game_2d_origin_y + pos.y;
    D3DXVECTOR3 d;
    FogVertex *vertex = (FogVertex *)((Fog *)fog.fog_ptr)->vertices;
    for (i32 i = 0; i < ((Fog *)fog.fog_ptr)->strip_count; i++)
    {
        for (i32 j = 0; j < ((Fog *)fog.fog_ptr)->strip_points; j++)
        {
            d = D3DXVECTOR3(points->x - center_x, points->y - center_y, points->z - pos.z);
            f32 t = radius * radius - (d.x * d.x + d.y * d.y);
            if (t >= 0.0f)
            {
                t /= radius * radius;
                vertex->diffuse = fog.fog_color;
                ((u8 *)&vertex->diffuse)[3] = 0xff;
                ((u8 *)&vertex->diffuse)[2] = 255.0f - (255 - ((u8 *)&vertex->diffuse)[2]) * t;
                ((u8 *)&vertex->diffuse)[1] = 255.0f - (255 - ((u8 *)&vertex->diffuse)[1]) * t;
                ((u8 *)&vertex->diffuse)[0] = 255.0f - (255 - ((u8 *)&vertex->diffuse)[0]) * t;
                f32 scale = t * 32.0f;
                D3DXVec3Normalize(&d, &d);
                d *= scale;
                d.x += sinf(angle_x.value) * t * 8.0f;
                d.y += sinf(angle_y.value) * t * 8.0f;
                vertex->pos.x += d.x;
                vertex->pos.y += d.y;
                vertex->pos.z = 0.0f;
                points->z = 0.0f;
            }
            else
            {
                ((u8 *)&vertex->diffuse)[3] = 0;
            }
            angle_x.value = wrap_angle(angle_x.value + ZUN_PI / 32);
            angle_y.value = wrap_angle(angle_y.value - ZUN_PI / 64);
            if (points->x <= g_early_arcade_offset_x)
            {
                vertex->pos.x = g_early_arcade_offset_x + 1.0f;
                points->x = g_early_arcade_offset_x + 1.0f;
            }
            else if (points->x >= g_early_arcade_offset_x + 384.0f)
            {
                vertex->pos.x = g_early_arcade_offset_x + 384.0f - 1.0f;
                points->x = g_early_arcade_offset_x + 384.0f - 1.0f;
            }
            if (points->y <= g_game_2d_origin_y)
            {
                vertex->pos.y = g_game_2d_origin_y + 1.0f;
                points->y = g_game_2d_origin_y + 1.0f;
            }
            else if (points->y >= g_game_2d_origin_y + 448.0f)
            {
                vertex->pos.y = g_game_2d_origin_y + 448.0f - 1.0f;
                points->y = g_game_2d_origin_y + 448.0f - 1.0f;
            }
            vertex->uv.x = points->x / g_resolution_x;
            // Through D3DXVECTOR2's operator FLOAT*: the store may alias uv.x,
            // so its test comes after it, as in the original.
            vertex->uv[1] = points->y / g_resolution_y;
            if (0.0f > vertex->uv.x)
            {
                vertex->uv.x = 0.0f;
            }
            if (0.0f > vertex->uv.y)
            {
                vertex->uv.y = 0.0f;
            }
            points++;
            vertex++;
        }
    }
    fog.wave_angle_x = wrap_angle(g_game_speed * (ZUN_PI / 16) + fog.wave_angle_x);
    fog.wave_angle_y = wrap_angle(g_game_speed * (ZUN_PI / 32) + fog.wave_angle_y);
}
