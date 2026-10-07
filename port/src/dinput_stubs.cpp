// DirectInput 8: the keyboard (256 bytes of DIK_* key states, bit 7 set
// while held) and one game controller (DIJOYSTATE2, axes set to -1000..1000
// through DIPROP_RANGE). Supervisor::dx_direct_input_initialize creates
// them; Input.cpp and Supervisor::read_joypad poll them each frame.
//
// Stubs for now: DirectInput8Create fails, which makes the game fall back
// to GetKeyboardState and the winmm joystick functions. The stub classes
// below are the skeleton for the SDL implementation.
#include <dinput.h>

#include "port_stub.h"

// dinput8.lib's data formats. Only their identity matters to an
// implementation (which one the game passed to SetDataFormat), so the
// object tables are left out.
#define DIDF_ABSAXIS 0x00000001
#define DIDF_RELAXIS 0x00000002
extern "C" const DIDATAFORMAT c_dfDIKeyboard = {sizeof(DIDATAFORMAT), sizeof(DIOBJECTDATAFORMAT), DIDF_RELAXIS, 256,
                                                0, NULL};
extern "C" const DIDATAFORMAT c_dfDIJoystick2 = {
    sizeof(DIDATAFORMAT), sizeof(DIOBJECTDATAFORMAT), DIDF_ABSAXIS, sizeof(DIJOYSTATE2), 0, NULL};

// The property ids (see dinput.h).
extern "C" const GUID TH16_DIPROP_RANGE = {4, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
extern "C" const GUID TH16_DIPROP_DEADZONE = {5, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};
extern "C" const GUID TH16_DIPROP_AXISMODE = {2, 0, 0, {0, 0, 0, 0, 0, 0, 0, 0}};

struct StubDirectInputDevice8 : public PortComObject<IDirectInputDevice8A>
{
    HRESULT GetCapabilities(LPDIDEVCAPS lpDIDevCaps) override
    {
        PORT_UNIMPLEMENTED();
        return DIERR_UNSUPPORTED;
    }
    HRESULT EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags) override
    {
        PORT_UNIMPLEMENTED();
        return DI_OK;
    }
    HRESULT GetProperty(REFGUID rguidProp, LPDIPROPHEADER pdiph) override
    {
        PORT_UNIMPLEMENTED();
        return DIERR_UNSUPPORTED;
    }
    HRESULT SetProperty(REFGUID rguidProp, LPCDIPROPHEADER pdiph) override
    {
        PORT_UNIMPLEMENTED();
        return DIERR_UNSUPPORTED;
    }
    HRESULT Acquire() override
    {
        PORT_UNIMPLEMENTED();
        return DI_OK;
    }
    HRESULT Unacquire() override
    {
        PORT_UNIMPLEMENTED();
        return DI_OK;
    }
    HRESULT GetDeviceState(DWORD cbData, LPVOID lpvData) override
    {
        PORT_UNIMPLEMENTED();
        memset(lpvData, 0, cbData);
        return DI_OK;
    }
    HRESULT SetDataFormat(LPCDIDATAFORMAT lpdf) override
    {
        PORT_UNIMPLEMENTED();
        return DI_OK;
    }
    HRESULT SetEventNotification(HANDLE hEvent) override
    {
        PORT_UNIMPLEMENTED();
        return DIERR_UNSUPPORTED;
    }
    HRESULT SetCooperativeLevel(HWND hwnd, DWORD dwFlags) override
    {
        PORT_UNIMPLEMENTED();
        return DI_OK;
    }
    HRESULT Poll() override
    {
        PORT_UNIMPLEMENTED();
        return DI_OK;
    }
};

struct StubDirectInput8 : public PortComObject<IDirectInput8A>
{
    HRESULT CreateDevice(REFGUID rguid, IDirectInputDevice8A **lplpDirectInputDevice, LPUNKNOWN pUnkOuter) override
    {
        PORT_UNIMPLEMENTED();
        *lplpDirectInputDevice = new StubDirectInputDevice8();
        return DI_OK;
    }
    HRESULT EnumDevices(DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags) override
    {
        PORT_UNIMPLEMENTED();
        return DI_OK;
    }
    HRESULT GetDeviceStatus(REFGUID rguidInstance) override
    {
        PORT_UNIMPLEMENTED();
        return DI_NOTATTACHED;
    }
};

extern "C" HRESULT DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut,
                                      LPUNKNOWN punkOuter)
{
    PORT_UNIMPLEMENTED();
    *ppvOut = NULL;
    return DIERR_UNSUPPORTED;
}

// Keeps the skeleton compiled while DirectInput8Create does not use it.
IDirectInput8A *port_new_stub_dinput()
{
    return new StubDirectInput8();
}
