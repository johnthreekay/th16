// Keyboard and game controller state from SDL: see port_input.h.
//
// Key states follow SDL key events (the message pump forwards them), not
// SDL_GetKeyboardState, so that events pushed with SDL_PushEvent count too.
// DIK_* codes are scan codes, as SDL's are: a key keeps its code whatever
// the layout. Virtual keys for letters and digits follow the layout, as on
// Windows.
#include <string.h>

#include <SDL.h>

#include <dinput.h>
#include <windows.h>

#include "port_input.h"
#include "port_platform.h"

namespace
{

bool g_scancode_down[SDL_NUM_SCANCODES];
// The keycode each held scancode produced when it went down.
SDL_Keycode g_scancode_keycode[SDL_NUM_SCANCODES];

struct ScancodeMap
{
    SDL_Scancode scancode;
    uint8_t dik;
    uint8_t vk;
};

// VK_OEM_* and the left/right modifier virtual keys.
enum
{
    VK_LSHIFT_ = 0xa0,
    VK_RSHIFT_ = 0xa1,
    VK_LCONTROL_ = 0xa2,
    VK_RCONTROL_ = 0xa3,
    VK_LMENU_ = 0xa4,
    VK_RMENU_ = 0xa5,
};

const ScancodeMap g_scancode_map[] = {
    {SDL_SCANCODE_A, DIK_A, 'A'},
    {SDL_SCANCODE_B, DIK_B, 'B'},
    {SDL_SCANCODE_C, DIK_C, 'C'},
    {SDL_SCANCODE_D, DIK_D, 'D'},
    {SDL_SCANCODE_E, DIK_E, 'E'},
    {SDL_SCANCODE_F, DIK_F, 'F'},
    {SDL_SCANCODE_G, DIK_G, 'G'},
    {SDL_SCANCODE_H, DIK_H, 'H'},
    {SDL_SCANCODE_I, DIK_I, 'I'},
    {SDL_SCANCODE_J, DIK_J, 'J'},
    {SDL_SCANCODE_K, DIK_K, 'K'},
    {SDL_SCANCODE_L, DIK_L, 'L'},
    {SDL_SCANCODE_M, DIK_M, 'M'},
    {SDL_SCANCODE_N, DIK_N, 'N'},
    {SDL_SCANCODE_O, DIK_O, 'O'},
    {SDL_SCANCODE_P, DIK_P, 'P'},
    {SDL_SCANCODE_Q, DIK_Q, 'Q'},
    {SDL_SCANCODE_R, DIK_R, 'R'},
    {SDL_SCANCODE_S, DIK_S, 'S'},
    {SDL_SCANCODE_T, DIK_T, 'T'},
    {SDL_SCANCODE_U, DIK_U, 'U'},
    {SDL_SCANCODE_V, DIK_V, 'V'},
    {SDL_SCANCODE_W, DIK_W, 'W'},
    {SDL_SCANCODE_X, DIK_X, 'X'},
    {SDL_SCANCODE_Y, DIK_Y, 'Y'},
    {SDL_SCANCODE_Z, DIK_Z, 'Z'},
    {SDL_SCANCODE_1, DIK_1, '1'},
    {SDL_SCANCODE_2, DIK_2, '2'},
    {SDL_SCANCODE_3, DIK_3, '3'},
    {SDL_SCANCODE_4, DIK_4, '4'},
    {SDL_SCANCODE_5, DIK_5, '5'},
    {SDL_SCANCODE_6, DIK_6, '6'},
    {SDL_SCANCODE_7, DIK_7, '7'},
    {SDL_SCANCODE_8, DIK_8, '8'},
    {SDL_SCANCODE_9, DIK_9, '9'},
    {SDL_SCANCODE_0, DIK_0, '0'},
    {SDL_SCANCODE_RETURN, DIK_RETURN, VK_RETURN},
    {SDL_SCANCODE_ESCAPE, DIK_ESCAPE, VK_ESCAPE},
    {SDL_SCANCODE_BACKSPACE, DIK_BACK, VK_BACK},
    {SDL_SCANCODE_TAB, DIK_TAB, VK_TAB},
    {SDL_SCANCODE_SPACE, DIK_SPACE, VK_SPACE},
    {SDL_SCANCODE_MINUS, DIK_MINUS, 0xbd},
    {SDL_SCANCODE_EQUALS, DIK_EQUALS, 0xbb},
    {SDL_SCANCODE_LEFTBRACKET, DIK_LBRACKET, 0xdb},
    {SDL_SCANCODE_RIGHTBRACKET, DIK_RBRACKET, 0xdd},
    {SDL_SCANCODE_BACKSLASH, DIK_BACKSLASH, 0xdc},
    {SDL_SCANCODE_NONUSHASH, DIK_BACKSLASH, 0xdc},
    {SDL_SCANCODE_SEMICOLON, DIK_SEMICOLON, 0xba},
    {SDL_SCANCODE_APOSTROPHE, DIK_APOSTROPHE, 0xde},
    {SDL_SCANCODE_GRAVE, DIK_GRAVE, 0xc0},
    {SDL_SCANCODE_COMMA, DIK_COMMA, 0xbc},
    {SDL_SCANCODE_PERIOD, DIK_PERIOD, 0xbe},
    {SDL_SCANCODE_SLASH, DIK_SLASH, 0xbf},
    {SDL_SCANCODE_CAPSLOCK, DIK_CAPITAL, 0x14},
    {SDL_SCANCODE_F1, DIK_F1, VK_F1},
    {SDL_SCANCODE_F2, DIK_F2, VK_F2},
    {SDL_SCANCODE_F3, DIK_F3, VK_F3},
    {SDL_SCANCODE_F4, DIK_F4, VK_F4},
    {SDL_SCANCODE_F5, DIK_F5, VK_F5},
    {SDL_SCANCODE_F6, DIK_F6, VK_F6},
    {SDL_SCANCODE_F7, DIK_F7, VK_F7},
    {SDL_SCANCODE_F8, DIK_F8, VK_F8},
    {SDL_SCANCODE_F9, DIK_F9, VK_F9},
    {SDL_SCANCODE_F10, DIK_F10, VK_F10},
    {SDL_SCANCODE_F11, DIK_F11, VK_F11},
    {SDL_SCANCODE_F12, DIK_F12, VK_F12},
    {SDL_SCANCODE_F13, 0x64, 0x7c},
    {SDL_SCANCODE_F14, 0x65, 0x7d},
    {SDL_SCANCODE_F15, 0x66, 0x7e},
    {SDL_SCANCODE_PRINTSCREEN, DIK_SYSRQ, 0x2c},
    {SDL_SCANCODE_SCROLLLOCK, DIK_SCROLL, 0x91},
    {SDL_SCANCODE_PAUSE, DIK_PAUSE, VK_PAUSE},
    {SDL_SCANCODE_INSERT, DIK_INSERT, VK_INSERT},
    {SDL_SCANCODE_HOME, DIK_HOME, VK_HOME},
    {SDL_SCANCODE_PAGEUP, DIK_PRIOR, VK_PRIOR},
    {SDL_SCANCODE_DELETE, DIK_DELETE, VK_DELETE},
    {SDL_SCANCODE_END, DIK_END, VK_END},
    {SDL_SCANCODE_PAGEDOWN, DIK_NEXT, VK_NEXT},
    {SDL_SCANCODE_RIGHT, DIK_RIGHT, VK_RIGHT},
    {SDL_SCANCODE_LEFT, DIK_LEFT, VK_LEFT},
    {SDL_SCANCODE_DOWN, DIK_DOWN, VK_DOWN},
    {SDL_SCANCODE_UP, DIK_UP, VK_UP},
    {SDL_SCANCODE_NUMLOCKCLEAR, DIK_NUMLOCK, 0x90},
    {SDL_SCANCODE_KP_DIVIDE, DIK_DIVIDE, 0x6f},
    {SDL_SCANCODE_KP_MULTIPLY, DIK_MULTIPLY, 0x6a},
    {SDL_SCANCODE_KP_MINUS, DIK_SUBTRACT, 0x6d},
    {SDL_SCANCODE_KP_PLUS, DIK_ADD, 0x6b},
    {SDL_SCANCODE_KP_ENTER, DIK_NUMPADENTER, VK_RETURN},
    {SDL_SCANCODE_KP_1, DIK_NUMPAD1, VK_NUMPAD1},
    {SDL_SCANCODE_KP_2, DIK_NUMPAD2, VK_NUMPAD2},
    {SDL_SCANCODE_KP_3, DIK_NUMPAD3, VK_NUMPAD3},
    {SDL_SCANCODE_KP_4, DIK_NUMPAD4, VK_NUMPAD4},
    {SDL_SCANCODE_KP_5, DIK_NUMPAD5, VK_NUMPAD5},
    {SDL_SCANCODE_KP_6, DIK_NUMPAD6, VK_NUMPAD6},
    {SDL_SCANCODE_KP_7, DIK_NUMPAD7, VK_NUMPAD7},
    {SDL_SCANCODE_KP_8, DIK_NUMPAD8, VK_NUMPAD8},
    {SDL_SCANCODE_KP_9, DIK_NUMPAD9, VK_NUMPAD9},
    {SDL_SCANCODE_KP_0, DIK_NUMPAD0, VK_NUMPAD0},
    {SDL_SCANCODE_KP_PERIOD, DIK_DECIMAL, 0x6e},
    {SDL_SCANCODE_NONUSBACKSLASH, 0x56, 0xe2},
    {SDL_SCANCODE_APPLICATION, DIK_APPS, 0x5d},
    {SDL_SCANCODE_INTERNATIONAL1, 0x73, 0xc1},
    {SDL_SCANCODE_INTERNATIONAL2, 0x70, 0x15},
    {SDL_SCANCODE_INTERNATIONAL3, 0x7d, 0xdc},
    {SDL_SCANCODE_INTERNATIONAL4, 0x79, 0x1c},
    {SDL_SCANCODE_INTERNATIONAL5, 0x7b, 0x1d},
    {SDL_SCANCODE_LCTRL, DIK_LCONTROL, VK_LCONTROL_},
    {SDL_SCANCODE_LSHIFT, DIK_LSHIFT, VK_LSHIFT_},
    {SDL_SCANCODE_LALT, DIK_LMENU, VK_LMENU_},
    {SDL_SCANCODE_LGUI, DIK_LWIN, 0x5b},
    {SDL_SCANCODE_RCTRL, DIK_RCONTROL, VK_RCONTROL_},
    {SDL_SCANCODE_RSHIFT, DIK_RSHIFT, VK_RSHIFT_},
    {SDL_SCANCODE_RALT, DIK_RMENU, VK_RMENU_},
    {SDL_SCANCODE_RGUI, DIK_RWIN, 0x5c},
};

uint8_t g_dik_of[SDL_NUM_SCANCODES];
uint8_t g_vk_of[SDL_NUM_SCANCODES];

void build_tables()
{
    static bool built = false;
    if (built)
    {
        return;
    }
    built = true;
    for (const ScancodeMap &entry : g_scancode_map)
    {
        g_dik_of[entry.scancode] = entry.dik;
        g_vk_of[entry.scancode] = entry.vk;
    }
}

// ---------------------------------------------------------------------------
// The game controller

SDL_GameController *g_controller;
SDL_Joystick *g_joystick;
SDL_JoystickID g_joystick_id = -1;
char g_pad_name[MAX_PATH];

void close_pad()
{
    if (g_controller != NULL)
    {
        SDL_GameControllerClose(g_controller);
    }
    else if (g_joystick != NULL)
    {
        SDL_JoystickClose(g_joystick);
    }
    g_controller = NULL;
    g_joystick = NULL;
    g_joystick_id = -1;
    g_pad_name[0] = '\0';
}

bool open_pad()
{
    if (g_joystick != NULL)
    {
        return true;
    }
    if (!SDL_WasInit(SDL_INIT_JOYSTICK))
    {
        return false;
    }
    int count = SDL_NumJoysticks();
    for (int i = 0; i < count; i++)
    {
        if (SDL_IsGameController(i))
        {
            g_controller = SDL_GameControllerOpen(i);
            if (g_controller != NULL)
            {
                g_joystick = SDL_GameControllerGetJoystick(g_controller);
            }
        }
        else
        {
            g_joystick = SDL_JoystickOpen(i);
        }
        if (g_joystick != NULL)
        {
            g_joystick_id = SDL_JoystickInstanceID(g_joystick);
            const char *name =
                g_controller != NULL ? SDL_GameControllerName(g_controller) : SDL_JoystickName(g_joystick);
            strncpy(g_pad_name, name != NULL ? name : "Game controller", sizeof(g_pad_name) - 1);
            port_log("game controller: %s%s", g_pad_name, g_controller != NULL ? "" : " (no SDL mapping)");
            return true;
        }
    }
    return false;
}

DWORD pov_from_directions(bool up, bool right, bool down, bool left)
{
    if (up && !down)
    {
        return right ? 4500 : left ? 31500 : 0;
    }
    if (down && !up)
    {
        return right ? 13500 : left ? 22500 : 18000;
    }
    if (right && !left)
    {
        return 9000;
    }
    if (left && !right)
    {
        return 27000;
    }
    return 0xffffffffu;
}

void fold_dpad_into_axes(PortPadState *state, bool up, bool right, bool down, bool left)
{
    if (left && !right)
    {
        state->axes[0] = -32768;
    }
    if (right && !left)
    {
        state->axes[0] = 32767;
    }
    if (up && !down)
    {
        state->axes[1] = -32768;
    }
    if (down && !up)
    {
        state->axes[1] = 32767;
    }
}

} // namespace

void port_input_handle_event(const SDL_Event &event)
{
    build_tables();
    switch (event.type)
    {
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        if (event.key.keysym.scancode < SDL_NUM_SCANCODES)
        {
            g_scancode_down[event.key.keysym.scancode] = event.type == SDL_KEYDOWN;
            g_scancode_keycode[event.key.keysym.scancode] = event.key.keysym.sym;
        }
        break;
    case SDL_JOYDEVICEREMOVED:
        if (event.jdevice.which == g_joystick_id)
        {
            port_log("game controller removed: %s", g_pad_name);
            close_pad();
        }
        break;
    }
}

void port_input_release_all_keys(void)
{
    memset(g_scancode_down, 0, sizeof(g_scancode_down));
}

void port_input_get_dik_state(BYTE *keys)
{
    build_tables();
    memset(keys, 0, 256);
    for (int i = 0; i < SDL_NUM_SCANCODES; i++)
    {
        if (g_scancode_down[i] && g_dik_of[i] != 0)
        {
            keys[g_dik_of[i]] = 0x80;
        }
    }
}

int port_input_vk_from_sdl(int scancode, int keycode)
{
    build_tables();
    // Letters and digits follow the keyboard layout.
    if (keycode >= 'a' && keycode <= 'z')
    {
        return keycode - 'a' + 'A';
    }
    if (keycode >= '0' && keycode <= '9' && !(scancode >= SDL_SCANCODE_KP_1 && scancode <= SDL_SCANCODE_KP_0))
    {
        return keycode;
    }
    return scancode >= 0 && scancode < SDL_NUM_SCANCODES ? g_vk_of[scancode] : 0;
}

void port_input_get_vk_state(BYTE *keys)
{
    memset(keys, 0, 256);
    for (int i = 0; i < SDL_NUM_SCANCODES; i++)
    {
        if (!g_scancode_down[i])
        {
            continue;
        }
        int vk = port_input_vk_from_sdl(i, g_scancode_keycode[i]);
        if (vk == 0)
        {
            continue;
        }
        keys[vk] = 0x80;
        // The generic modifier keys are held when either side is.
        if (vk == VK_LSHIFT_ || vk == VK_RSHIFT_)
        {
            keys[VK_SHIFT] = 0x80;
        }
        else if (vk == VK_LCONTROL_ || vk == VK_RCONTROL_)
        {
            keys[VK_CONTROL] = 0x80;
        }
        else if (vk == VK_LMENU_ || vk == VK_RMENU_)
        {
            keys[VK_MENU] = 0x80;
        }
    }
}

bool port_input_read_pad(PortPadState *state)
{
    memset(state, 0, sizeof(*state));
    state->pov = 0xffffffffu;
    if (!open_pad())
    {
        return false;
    }
    if (!SDL_JoystickGetAttached(g_joystick))
    {
        close_pad();
        return false;
    }
    if (g_controller != NULL)
    {
        static const SDL_GameControllerButton buttons[10] = {
            SDL_CONTROLLER_BUTTON_A,         SDL_CONTROLLER_BUTTON_B,         SDL_CONTROLLER_BUTTON_X,
            SDL_CONTROLLER_BUTTON_Y,         SDL_CONTROLLER_BUTTON_LEFTSHOULDER,
            SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START,
            SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK,
        };
        for (int i = 0; i < 10; i++)
        {
            state->buttons[i] = SDL_GameControllerGetButton(g_controller, buttons[i]);
        }
        state->button_count = 10;
        state->axes[0] = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTX);
        state->axes[1] = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_LEFTY);
        // XInput pads through DirectInput: both triggers on one axis.
        int z = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERLEFT) -
                SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        state->axes[2] = z < -32768 ? -32768 : z > 32767 ? 32767 : z;
        state->axes[3] = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTX);
        state->axes[4] = SDL_GameControllerGetAxis(g_controller, SDL_CONTROLLER_AXIS_RIGHTY);
        state->has_axis[0] = state->has_axis[1] = state->has_axis[2] = state->has_axis[3] = state->has_axis[4] = 1;
        bool up = SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_UP);
        bool down = SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        bool left = SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        bool right = SDL_GameControllerGetButton(g_controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        state->pov = pov_from_directions(up, right, down, left);
        state->has_pov = true;
        fold_dpad_into_axes(state, up, right, down, left);
        return true;
    }
    int axes = SDL_JoystickNumAxes(g_joystick);
    for (int i = 0; i < axes && i < 8; i++)
    {
        state->axes[i] = SDL_JoystickGetAxis(g_joystick, i);
        state->has_axis[i] = 1;
    }
    int buttons = SDL_JoystickNumButtons(g_joystick);
    state->button_count = buttons > 32 ? 32 : buttons;
    for (int i = 0; i < state->button_count; i++)
    {
        state->buttons[i] = SDL_JoystickGetButton(g_joystick, i);
    }
    if (SDL_JoystickNumHats(g_joystick) > 0)
    {
        Uint8 hat = SDL_JoystickGetHat(g_joystick, 0);
        bool up = hat & SDL_HAT_UP, right = hat & SDL_HAT_RIGHT, down = hat & SDL_HAT_DOWN, left = hat & SDL_HAT_LEFT;
        state->pov = pov_from_directions(up, right, down, left);
        state->has_pov = true;
        fold_dpad_into_axes(state, up, right, down, left);
    }
    return true;
}

const char *port_input_pad_name(void)
{
    return g_pad_name;
}
