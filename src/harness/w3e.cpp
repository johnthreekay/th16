// Stand-in callers for wave 3 range E (0x44f710-0x4630f0) functions whose
// shape depends on how the rest of the game calls them.
#include "../AnmManager.h"
#include "../TextHelper.h"

// Like WinMain and the Window frame functions around a device reset.
void harness_w3e_device_reset()
{
    g_AnmManager->release_textures();
    g_AnmManager->create_d3d_textures_for_loaded_anms();
    g_AnmManager->release_textures();
    g_AnmManager->create_d3d_textures_for_loaded_anms();
}

// Like the frame function at 0x45b080.
void harness_w3e_screenshots()
{
    g_AnmManager->take_screenshots();
}

// Like Supervisor::teardown_everything (0x43b6e5).
void harness_w3e_teardown_text()
{
    g_TextHelper.release_buffer();
}

// Like the text VM drawing at 0x46da88 and draw_rtext (0x46dbf9, 0x46dd60).
void harness_w3e_draw_text(RECT *rect, i32 x, i32 size, D3DCOLOR color, D3DCOLOR shadow, const char *text,
                           IDirect3DTexture9 *texture, i32 font, i32 spacing)
{
    draw_text(rect, x, size, color, shadow, text, texture, font, spacing, 1);
    draw_text(rect, x * 2, size, color, shadow, text, texture, font, 0, 0);
    draw_text(rect + 1, x, size + 1, shadow, color, text, texture, font + 1, spacing * 2, spacing);
}
