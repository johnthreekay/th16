// DirectInput 8 over SDL: the keyboard (256 bytes of DIK_* key states, bit
// 7 set while held) and one game controller (DIJOYSTATE2, the axis ranges
// set through DIPROP_RANGE). Supervisor::dx_direct_input_initialize
// creates them; Input.cpp and Supervisor::read_joypad poll them each frame.
// The states come from port_input.h; see there for the controller layout.
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include <dinput.h>

#include "port_input.h"
#include "port_platform.h"
#include "port_stub.h"

#define DIDF_ABSAXIS 0x00000001
#define DIDF_RELAXIS 0x00000002

// dinput8.lib's data formats. Only their identity matters here (which one
// the game passed to SetDataFormat), so the object tables are left out.
extern "C" const DIDATAFORMAT c_dfDIKeyboard = {sizeof(DIDATAFORMAT), sizeof(DIOBJECTDATAFORMAT), DIDF_RELAXIS, 256,
                                                0, NULL};
extern "C" const DIDATAFORMAT c_dfDIJoystick2 = {
    sizeof(DIDATAFORMAT), sizeof(DIOBJECTDATAFORMAT), DIDF_ABSAXIS, sizeof(DIJOYSTATE2), 0, NULL};

// The property ids (see dinput.h).
extern "C" const GUID TH16_DIPROP_RANGE = {4, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
extern "C" const GUID TH16_DIPROP_DEADZONE = {5, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
extern "C" const GUID TH16_DIPROP_AXISMODE = {2, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};

namespace
{

// The instance GUID handed out for the controller.
const GUID g_pad_instance = {0x74683136, 0x7064, 0x0001, {'t', 'h', '1', '6', 'p', 'o', 'r', 't'}};

// The axes in DIJOYSTATE2 order, their object GUIDs and offsets.
struct AxisInfo
{
    const GUID *guid;
    DWORD offset;
    const char *name;
};
const AxisInfo g_axes[8] = {
    {&GUID_XAxis, offsetof(DIJOYSTATE2, lX), "X Axis"},
    {&GUID_YAxis, offsetof(DIJOYSTATE2, lY), "Y Axis"},
    {&GUID_ZAxis, offsetof(DIJOYSTATE2, lZ), "Z Axis"},
    {&GUID_RxAxis, offsetof(DIJOYSTATE2, lRx), "X Rotation"},
    {&GUID_RyAxis, offsetof(DIJOYSTATE2, lRy), "Y Rotation"},
    {&GUID_RzAxis, offsetof(DIJOYSTATE2, lRz), "Z Rotation"},
    {&GUID_Slider, offsetof(DIJOYSTATE2, rglSlider[0]), "Slider"},
    {&GUID_Slider, offsetof(DIJOYSTATE2, rglSlider[1]), "Slider"},
};

bool same_guid(const GUID &a, const GUID &b)
{
    return memcmp(&a, &b, sizeof(GUID)) == 0;
}

struct PortKeyboard : public PortComObject<IDirectInputDevice8A>
{
    bool acquired = false;
    bool has_format = false;

    HRESULT GetCapabilities(LPDIDEVCAPS caps) override
    {
        DWORD size = caps->dwSize;
        memset(caps, 0, size < sizeof(DIDEVCAPS) ? size : sizeof(DIDEVCAPS));
        caps->dwSize = size;
        caps->dwFlags = DIDC_ATTACHED;
        caps->dwDevType = DI8DEVTYPE_KEYBOARD | (DI8DEVTYPEKEYBOARD_PCENH << 8);
        caps->dwButtons = 128;
        return DI_OK;
    }
    HRESULT EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA callback, LPVOID ref, DWORD flags) override
    {
        return DI_OK;
    }
    HRESULT GetProperty(REFGUID prop, LPDIPROPHEADER header) override
    {
        return DIERR_UNSUPPORTED;
    }
    HRESULT SetProperty(REFGUID prop, LPCDIPROPHEADER header) override
    {
        return DI_PROPNOEFFECT;
    }
    HRESULT Acquire() override
    {
        if (!has_format)
        {
            return DIERR_INVALIDPARAM;
        }
        HRESULT result = acquired ? S_FALSE : DI_OK;
        acquired = true;
        return result;
    }
    HRESULT Unacquire() override
    {
        HRESULT result = acquired ? DI_OK : DI_NOEFFECT_;
        acquired = false;
        return result;
    }
    HRESULT GetDeviceState(DWORD size, LPVOID data) override
    {
        if (!acquired)
        {
            return DIERR_NOTACQUIRED;
        }
        if (size != 256)
        {
            return DIERR_INVALIDPARAM;
        }
        port_input_get_dik_state((BYTE *)data);
        static const bool debug = getenv("TH16_DEBUG_EVENTS") != NULL;
        if (debug)
        {
            // Logs each change of the set of held keys.
            static BYTE last[256];
            if (memcmp(last, data, 256) != 0)
            {
                memcpy(last, data, 256);
                std::string held;
                for (int i = 0; i < 256; i++)
                {
                    if (last[i] & 0x80)
                    {
                        held += " " + std::to_string(i);
                    }
                }
                port_log("DirectInput keyboard: held%s", held.empty() ? " nothing" : held.c_str());
            }
        }
        return DI_OK;
    }
    HRESULT SetDataFormat(LPCDIDATAFORMAT format) override
    {
        if (format != &c_dfDIKeyboard || acquired)
        {
            return DIERR_INVALIDPARAM;
        }
        has_format = true;
        return DI_OK;
    }
    HRESULT SetEventNotification(HANDLE event) override
    {
        return DIERR_UNSUPPORTED;
    }
    HRESULT SetCooperativeLevel(HWND hwnd, DWORD flags) override
    {
        return DI_OK;
    }
    HRESULT Poll() override
    {
        // Keyboards need no polling.
        return acquired ? DI_NOEFFECT_ : DIERR_NOTACQUIRED;
    }

    static const HRESULT DI_NOEFFECT_ = S_FALSE;
};

struct PortJoystick : public PortComObject<IDirectInputDevice8A>
{
    bool acquired = false;
    bool has_format = false;
    LONG range_min[8];
    LONG range_max[8];
    DWORD deadzone[8] = {};

    PortJoystick()
    {
        // DirectInput's default range.
        for (int i = 0; i < 8; i++)
        {
            range_min[i] = 0;
            range_max[i] = 65535;
        }
    }

    // Which axes an object id or offset (DIPH_BYID or DIPH_BYOFFSET) names;
    // all of them for DIPH_DEVICE.
    bool axes_for(const DIPROPHEADER *header, bool *selected)
    {
        memset(selected, 0, 8 * sizeof(bool));
        switch (header->dwHow)
        {
        case DIPH_DEVICE:
            for (int i = 0; i < 8; i++)
            {
                selected[i] = true;
            }
            return true;
        case DIPH_BYID:
            if ((header->dwObj & DIDFT_AXIS) != 0 && DIDFT_GETINSTANCE(header->dwObj) < 8)
            {
                selected[DIDFT_GETINSTANCE(header->dwObj)] = true;
                return true;
            }
            return false;
        case DIPH_BYOFFSET:
            for (int i = 0; i < 8; i++)
            {
                if (g_axes[i].offset == header->dwObj)
                {
                    selected[i] = true;
                    return true;
                }
            }
            return false;
        }
        return false;
    }

    HRESULT GetCapabilities(LPDIDEVCAPS caps) override
    {
        PortPadState state;
        bool attached = port_input_read_pad(&state);
        DWORD size = caps->dwSize;
        memset(caps, 0, size < sizeof(DIDEVCAPS) ? size : sizeof(DIDEVCAPS));
        caps->dwSize = size;
        caps->dwFlags = (attached ? DIDC_ATTACHED : 0) | DIDC_POLLEDDEVICE;
        caps->dwDevType = DI8DEVTYPE_GAMEPAD | (DI8DEVTYPEGAMEPAD_STANDARD << 8);
        for (int i = 0; i < 8; i++)
        {
            caps->dwAxes += state.has_axis[i];
        }
        caps->dwButtons = state.button_count;
        caps->dwPOVs = state.has_pov ? 1 : 0;
        return DI_OK;
    }
    HRESULT EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA callback, LPVOID ref, DWORD flags) override
    {
        PortPadState state;
        port_input_read_pad(&state);
        DWORD wanted = flags & 0xff;
        DIDEVICEOBJECTINSTANCEA object;
        for (int i = 0; i < 8; i++)
        {
            if (!state.has_axis[i] || (wanted != DIDFT_ALL && !(wanted & DIDFT_AXIS)))
            {
                continue;
            }
            memset(&object, 0, sizeof(object));
            object.dwSize = sizeof(object);
            object.guidType = *g_axes[i].guid;
            object.dwOfs = g_axes[i].offset;
            object.dwType = DIDFT_ABSAXIS | DIDFT_MAKEINSTANCE(i);
            object.dwFlags = DIDOI_ASPECTPOSITION;
            strcpy(object.tszName, g_axes[i].name);
            if (callback(&object, ref) == DIENUM_STOP)
            {
                return DI_OK;
            }
        }
        if (state.has_pov && (wanted == DIDFT_ALL || (wanted & DIDFT_POV)))
        {
            memset(&object, 0, sizeof(object));
            object.dwSize = sizeof(object);
            object.guidType = GUID_POV;
            object.dwOfs = offsetof(DIJOYSTATE2, rgdwPOV[0]);
            object.dwType = DIDFT_POV | DIDFT_MAKEINSTANCE(0);
            strcpy(object.tszName, "Hat Switch");
            if (callback(&object, ref) == DIENUM_STOP)
            {
                return DI_OK;
            }
        }
        for (int i = 0; i < state.button_count; i++)
        {
            if (wanted != DIDFT_ALL && !(wanted & DIDFT_BUTTON))
            {
                break;
            }
            memset(&object, 0, sizeof(object));
            object.dwSize = sizeof(object);
            object.guidType = GUID_Key;
            object.dwOfs = (DWORD)(offsetof(DIJOYSTATE2, rgbButtons) + i);
            object.dwType = DIDFT_PSHBUTTON | DIDFT_MAKEINSTANCE(i);
            snprintf(object.tszName, sizeof(object.tszName), "Button %d", i);
            if (callback(&object, ref) == DIENUM_STOP)
            {
                return DI_OK;
            }
        }
        return DI_OK;
    }
    HRESULT GetProperty(REFGUID prop, LPDIPROPHEADER header) override
    {
        bool selected[8];
        if (same_guid(prop, DIPROP_RANGE) && axes_for(header, selected))
        {
            for (int i = 0; i < 8; i++)
            {
                if (selected[i])
                {
                    ((DIPROPRANGE *)header)->lMin = range_min[i];
                    ((DIPROPRANGE *)header)->lMax = range_max[i];
                    return DI_OK;
                }
            }
        }
        if (same_guid(prop, DIPROP_DEADZONE) && axes_for(header, selected))
        {
            for (int i = 0; i < 8; i++)
            {
                if (selected[i])
                {
                    ((DIPROPDWORD *)header)->dwData = deadzone[i];
                    return DI_OK;
                }
            }
        }
        return DIERR_UNSUPPORTED;
    }
    HRESULT SetProperty(REFGUID prop, LPCDIPROPHEADER header) override
    {
        bool selected[8];
        if (same_guid(prop, DIPROP_RANGE))
        {
            const DIPROPRANGE *range = (const DIPROPRANGE *)header;
            if (range->lMin >= range->lMax || !axes_for(header, selected))
            {
                return DIERR_INVALIDPARAM;
            }
            for (int i = 0; i < 8; i++)
            {
                if (selected[i])
                {
                    range_min[i] = range->lMin;
                    range_max[i] = range->lMax;
                }
            }
            return DI_OK;
        }
        if (same_guid(prop, DIPROP_DEADZONE))
        {
            const DIPROPDWORD *value = (const DIPROPDWORD *)header;
            if (value->dwData > 10000 || !axes_for(header, selected))
            {
                return DIERR_INVALIDPARAM;
            }
            for (int i = 0; i < 8; i++)
            {
                if (selected[i])
                {
                    deadzone[i] = value->dwData;
                }
            }
            return DI_OK;
        }
        return DIERR_UNSUPPORTED;
    }
    HRESULT Acquire() override
    {
        if (!has_format)
        {
            return DIERR_INVALIDPARAM;
        }
        HRESULT result = acquired ? S_FALSE : DI_OK;
        acquired = true;
        return result;
    }
    HRESULT Unacquire() override
    {
        HRESULT result = acquired ? DI_OK : S_FALSE;
        acquired = false;
        return result;
    }
    // Unplugged: DIERR_INPUTLOST, and the device has to be acquired again
    // (the game does, every frame, until the controller comes back).
    HRESULT Poll() override
    {
        if (!acquired)
        {
            return DIERR_NOTACQUIRED;
        }
        PortPadState state;
        if (!port_input_read_pad(&state))
        {
            acquired = false;
            return DIERR_INPUTLOST;
        }
        return DI_OK;
    }
    HRESULT GetDeviceState(DWORD size, LPVOID data) override
    {
        if (!acquired)
        {
            return DIERR_NOTACQUIRED;
        }
        if (size != sizeof(DIJOYSTATE2))
        {
            return DIERR_INVALIDPARAM;
        }
        PortPadState state;
        if (!port_input_read_pad(&state))
        {
            acquired = false;
            return DIERR_INPUTLOST;
        }
        DIJOYSTATE2 *js = (DIJOYSTATE2 *)data;
        memset(js, 0, sizeof(*js));
        for (int i = 0; i < 8; i++)
        {
            // -32768..32767 onto the range, with the dead zone (in
            // 1/10000 of the half range) around the centre.
            double value = (state.axes[i] + 32768) / 65535.0;
            double offset = value * 2.0 - 1.0;
            double dead = deadzone[i] / 10000.0;
            if (offset > -dead && offset < dead)
            {
                value = 0.5;
            }
            LONG scaled = (LONG)floor(range_min[i] + value * (double)(range_max[i] - range_min[i]) + 0.5);
            *(LONG *)((BYTE *)js + g_axes[i].offset) = scaled;
        }
        for (int i = 0; i < 4; i++)
        {
            js->rgdwPOV[i] = 0xffffffffu;
        }
        js->rgdwPOV[0] = state.pov;
        for (int i = 0; i < state.button_count; i++)
        {
            js->rgbButtons[i] = state.buttons[i] ? 0x80 : 0;
        }
        return DI_OK;
    }
    HRESULT SetDataFormat(LPCDIDATAFORMAT format) override
    {
        if (format != &c_dfDIJoystick2 || acquired)
        {
            return DIERR_INVALIDPARAM;
        }
        has_format = true;
        return DI_OK;
    }
    HRESULT SetEventNotification(HANDLE event) override
    {
        return DIERR_UNSUPPORTED;
    }
    HRESULT SetCooperativeLevel(HWND hwnd, DWORD flags) override
    {
        return DI_OK;
    }
};

struct PortDirectInput8 : public PortComObject<IDirectInput8A>
{
    HRESULT CreateDevice(REFGUID guid, IDirectInputDevice8A **device, LPUNKNOWN outer) override
    {
        if (same_guid(guid, GUID_SysKeyboard))
        {
            *device = new PortKeyboard();
            return DI_OK;
        }
        PortPadState state;
        if (same_guid(guid, g_pad_instance) && port_input_read_pad(&state))
        {
            *device = new PortJoystick();
            return DI_OK;
        }
        *device = NULL;
        return DIERR_DEVICENOTREG;
    }
    HRESULT EnumDevices(DWORD type, LPDIENUMDEVICESCALLBACKA callback, LPVOID ref, DWORD flags) override
    {
        if (type != DI8DEVCLASS_GAMECTRL && type != DI8DEVCLASS_ALL)
        {
            return DI_OK;
        }
        PortPadState state;
        if (!port_input_read_pad(&state))
        {
            return DI_OK;
        }
        DIDEVICEINSTANCEA instance;
        memset(&instance, 0, sizeof(instance));
        instance.dwSize = sizeof(instance);
        instance.guidInstance = g_pad_instance;
        instance.guidProduct = g_pad_instance;
        instance.dwDevType = DI8DEVTYPE_GAMEPAD | (DI8DEVTYPEGAMEPAD_STANDARD << 8);
        std::string name;
        if (!port_utf8_to_sjis(port_input_pad_name(), &name))
        {
            name = "Game controller";
        }
        strncpy(instance.tszInstanceName, name.c_str(), MAX_PATH - 1);
        strncpy(instance.tszProductName, name.c_str(), MAX_PATH - 1);
        callback(&instance, ref);
        return DI_OK;
    }
    HRESULT GetDeviceStatus(REFGUID guid) override
    {
        PortPadState state;
        if (same_guid(guid, GUID_SysKeyboard) || (same_guid(guid, g_pad_instance) && port_input_read_pad(&state)))
        {
            return DI_OK;
        }
        return DI_NOTATTACHED;
    }
};

} // namespace

extern "C" HRESULT DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut,
                                      LPUNKNOWN punkOuter)
{
    *ppvOut = new PortDirectInput8();
    return DI_OK;
}
