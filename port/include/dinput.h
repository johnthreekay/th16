// Stand-in for the DirectX SDK's dinput.h (DirectInput 8, ANSI) in the
// portable build: structures with the SDK's layouts, constants with its
// values, key codes (DIK_*, keyboard scan codes) and the two interfaces as
// abstract classes. port/src/dinput_*.cpp implements them.
#pragma once

#include <windows.h>

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0800
#endif

#define DI_OK S_OK
#define DI_NOTATTACHED S_FALSE
#define DI_PROPNOEFFECT S_FALSE
#define DIERR_INPUTLOST ((HRESULT)0x8007001E)
#define DIERR_NOTACQUIRED ((HRESULT)0x8007000C)
#define DIERR_INVALIDPARAM E_INVALIDARG
#define DIERR_OTHERAPPHASPRIO E_ACCESSDENIED
#define DIERR_DEVICENOTREG ((HRESULT)0x80040154)
#define DIERR_UNSUPPORTED E_NOTIMPL

#define DIENUM_STOP 0
#define DIENUM_CONTINUE 1

#define DI8DEVCLASS_ALL 0
#define DI8DEVCLASS_DEVICE 1
#define DI8DEVCLASS_POINTER 2
#define DI8DEVCLASS_KEYBOARD 3
#define DI8DEVCLASS_GAMECTRL 4

#define DIEDFL_ALLDEVICES 0x00000000
#define DIEDFL_ATTACHEDONLY 0x00000001

#define DIDFT_ALL 0x00000000
#define DIDFT_RELAXIS 0x00000001
#define DIDFT_ABSAXIS 0x00000002
#define DIDFT_AXIS 0x00000003
#define DIDFT_PSHBUTTON 0x00000004
#define DIDFT_TGLBUTTON 0x00000008
#define DIDFT_BUTTON 0x0000000C
#define DIDFT_POV 0x00000010

#define DISCL_EXCLUSIVE 0x00000001
#define DISCL_NONEXCLUSIVE 0x00000002
#define DISCL_FOREGROUND 0x00000004
#define DISCL_BACKGROUND 0x00000008
#define DISCL_NOWINKEY 0x00000010

#define DIPH_DEVICE 0
#define DIPH_BYOFFSET 1
#define DIPH_BYID 2
#define DIPH_BYUSAGE 3

// In the SDK the property ids are small integers cast to GUID references
// (MAKEDIPROP). Here they are real objects (port/src/dinput_*.cpp) that the
// implementation recognises by address.
extern "C" const GUID TH16_DIPROP_RANGE;
extern "C" const GUID TH16_DIPROP_DEADZONE;
extern "C" const GUID TH16_DIPROP_AXISMODE;
#define DIPROP_RANGE TH16_DIPROP_RANGE
#define DIPROP_DEADZONE TH16_DIPROP_DEADZONE
#define DIPROP_AXISMODE TH16_DIPROP_AXISMODE

typedef struct DIPROPHEADER
{
    DWORD dwSize;
    DWORD dwHeaderSize;
    DWORD dwObj;
    DWORD dwHow;
} DIPROPHEADER, *LPDIPROPHEADER;
typedef const DIPROPHEADER *LPCDIPROPHEADER;

typedef struct DIPROPRANGE
{
    DIPROPHEADER diph;
    LONG lMin;
    LONG lMax;
} DIPROPRANGE, *LPDIPROPRANGE;

typedef struct DIPROPDWORD
{
    DIPROPHEADER diph;
    DWORD dwData;
} DIPROPDWORD, *LPDIPROPDWORD;

// 0x2c bytes; Supervisor embeds one.
typedef struct DIDEVCAPS
{
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwDevType;
    DWORD dwAxes;
    DWORD dwButtons;
    DWORD dwPOVs;
    DWORD dwFFSamplePeriod;
    DWORD dwFFMinTimeResolution;
    DWORD dwFirmwareRevision;
    DWORD dwHardwareRevision;
    DWORD dwFFDriverVersion;
} DIDEVCAPS, *LPDIDEVCAPS;

typedef struct DIJOYSTATE2
{
    LONG lX;
    LONG lY;
    LONG lZ;
    LONG lRx;
    LONG lRy;
    LONG lRz;
    LONG rglSlider[2];
    DWORD rgdwPOV[4];
    BYTE rgbButtons[128];
    LONG lVX;
    LONG lVY;
    LONG lVZ;
    LONG lVRx;
    LONG lVRy;
    LONG lVRz;
    LONG rglVSlider[2];
    LONG lAX;
    LONG lAY;
    LONG lAZ;
    LONG lARx;
    LONG lARy;
    LONG lARz;
    LONG rglASlider[2];
    LONG lFX;
    LONG lFY;
    LONG lFZ;
    LONG lFRx;
    LONG lFRy;
    LONG lFRz;
    LONG rglFSlider[2];
} DIJOYSTATE2, *LPDIJOYSTATE2;

typedef struct DIDEVICEINSTANCEA
{
    DWORD dwSize;
    GUID guidInstance;
    GUID guidProduct;
    DWORD dwDevType;
    CHAR tszInstanceName[MAX_PATH];
    CHAR tszProductName[MAX_PATH];
    GUID guidFFDriver;
    WORD wUsagePage;
    WORD wUsage;
} DIDEVICEINSTANCEA, *LPDIDEVICEINSTANCEA;
typedef const DIDEVICEINSTANCEA *LPCDIDEVICEINSTANCEA;

typedef struct DIDEVICEOBJECTINSTANCEA
{
    DWORD dwSize;
    GUID guidType;
    DWORD dwOfs;
    DWORD dwType;
    DWORD dwFlags;
    CHAR tszName[MAX_PATH];
    DWORD dwFFMaxForce;
    DWORD dwFFForceResolution;
    WORD wCollectionNumber;
    WORD wDesignatorIndex;
    WORD wUsagePage;
    WORD wUsage;
    DWORD dwDimension;
    WORD wExponent;
    WORD wReportId;
} DIDEVICEOBJECTINSTANCEA, *LPDIDEVICEOBJECTINSTANCEA;
typedef const DIDEVICEOBJECTINSTANCEA *LPCDIDEVICEOBJECTINSTANCEA;

typedef struct _DIOBJECTDATAFORMAT
{
    const GUID *pguid;
    DWORD dwOfs;
    DWORD dwType;
    DWORD dwFlags;
} DIOBJECTDATAFORMAT, *LPDIOBJECTDATAFORMAT;
typedef const DIOBJECTDATAFORMAT *LPCDIOBJECTDATAFORMAT;

typedef struct _DIDATAFORMAT
{
    DWORD dwSize;
    DWORD dwObjSize;
    DWORD dwFlags;
    DWORD dwDataSize;
    DWORD dwNumObjs;
    LPDIOBJECTDATAFORMAT rgodf;
} DIDATAFORMAT, *LPDIDATAFORMAT;
typedef const DIDATAFORMAT *LPCDIDATAFORMAT;

typedef BOOL(CALLBACK *LPDIENUMDEVICESCALLBACKA)(LPCDIDEVICEINSTANCEA, LPVOID);
typedef BOOL(CALLBACK *LPDIENUMDEVICEOBJECTSCALLBACKA)(LPCDIDEVICEOBJECTINSTANCEA, LPVOID);

// The data formats from dinput8.lib (port/src/dinput_*.cpp).
extern "C" const DIDATAFORMAT c_dfDIKeyboard;
extern "C" const DIDATAFORMAT c_dfDIJoystick2;

// Device and object ids. The game defines the ones it uses itself
// (SupervisorSetup.cpp).
DEFINE_GUID(IID_IDirectInput8A, 0xbf798030, 0x483a, 0x4da2, 0xaa, 0x99, 0x5d, 0x64, 0xed, 0x36, 0x97, 0x00);
DEFINE_GUID(GUID_XAxis, 0xa36d02e0, 0xc9f3, 0x11cf, 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
DEFINE_GUID(GUID_YAxis, 0xa36d02e1, 0xc9f3, 0x11cf, 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
DEFINE_GUID(GUID_ZAxis, 0xa36d02e2, 0xc9f3, 0x11cf, 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
DEFINE_GUID(GUID_RxAxis, 0xa36d02f4, 0xc9f3, 0x11cf, 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
DEFINE_GUID(GUID_RyAxis, 0xa36d02f5, 0xc9f3, 0x11cf, 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
DEFINE_GUID(GUID_RzAxis, 0xa36d02e3, 0xc9f3, 0x11cf, 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
DEFINE_GUID(GUID_Slider, 0xa36d02e4, 0xc9f3, 0x11cf, 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
DEFINE_GUID(GUID_Key, 0x55728220, 0xd33c, 0x11cf, 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
DEFINE_GUID(GUID_POV, 0xa36d02f2, 0xc9f3, 0x11cf, 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);
DEFINE_GUID(GUID_SysKeyboard, 0x6f1d2b61, 0xd5a0, 0x11cf, 0xbf, 0xc7, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);

struct IDirectInputDevice8A : public IUnknown
{
    virtual HRESULT GetCapabilities(LPDIDEVCAPS lpDIDevCaps) = 0;
    virtual HRESULT EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags) = 0;
    virtual HRESULT GetProperty(REFGUID rguidProp, LPDIPROPHEADER pdiph) = 0;
    virtual HRESULT SetProperty(REFGUID rguidProp, LPCDIPROPHEADER pdiph) = 0;
    virtual HRESULT Acquire() = 0;
    virtual HRESULT Unacquire() = 0;
    virtual HRESULT GetDeviceState(DWORD cbData, LPVOID lpvData) = 0;
    virtual HRESULT SetDataFormat(LPCDIDATAFORMAT lpdf) = 0;
    virtual HRESULT SetEventNotification(HANDLE hEvent) = 0;
    virtual HRESULT SetCooperativeLevel(HWND hwnd, DWORD dwFlags) = 0;
    virtual HRESULT Poll() = 0;
};

struct IDirectInput8A : public IUnknown
{
    virtual HRESULT CreateDevice(REFGUID rguid, IDirectInputDevice8A **lplpDirectInputDevice,
                                 LPUNKNOWN pUnkOuter) = 0;
    virtual HRESULT EnumDevices(DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef,
                                DWORD dwFlags) = 0;
    virtual HRESULT GetDeviceStatus(REFGUID rguidInstance) = 0;
};

typedef IDirectInput8A *LPDIRECTINPUT8A, *LPDIRECTINPUT8;
typedef IDirectInputDevice8A *LPDIRECTINPUTDEVICE8A, *LPDIRECTINPUTDEVICE8;
#define IDirectInput8 IDirectInput8A
#define IDirectInputDevice8 IDirectInputDevice8A
#define IID_IDirectInput8 IID_IDirectInput8A
#define DIDEVICEINSTANCE DIDEVICEINSTANCEA
#define LPCDIDEVICEINSTANCE LPCDIDEVICEINSTANCEA
#define DIDEVICEOBJECTINSTANCE DIDEVICEOBJECTINSTANCEA
#define LPCDIDEVICEOBJECTINSTANCE LPCDIDEVICEOBJECTINSTANCEA

extern "C" HRESULT DirectInput8Create(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID *ppvOut,
                                      LPUNKNOWN punkOuter);

// Keyboard scan codes.
#define DIK_ESCAPE 0x01
#define DIK_1 0x02
#define DIK_2 0x03
#define DIK_3 0x04
#define DIK_4 0x05
#define DIK_5 0x06
#define DIK_6 0x07
#define DIK_7 0x08
#define DIK_8 0x09
#define DIK_9 0x0A
#define DIK_0 0x0B
#define DIK_MINUS 0x0C
#define DIK_EQUALS 0x0D
#define DIK_BACK 0x0E
#define DIK_TAB 0x0F
#define DIK_Q 0x10
#define DIK_W 0x11
#define DIK_E 0x12
#define DIK_R 0x13
#define DIK_T 0x14
#define DIK_Y 0x15
#define DIK_U 0x16
#define DIK_I 0x17
#define DIK_O 0x18
#define DIK_P 0x19
#define DIK_LBRACKET 0x1A
#define DIK_RBRACKET 0x1B
#define DIK_RETURN 0x1C
#define DIK_LCONTROL 0x1D
#define DIK_A 0x1E
#define DIK_S 0x1F
#define DIK_D 0x20
#define DIK_F 0x21
#define DIK_G 0x22
#define DIK_H 0x23
#define DIK_J 0x24
#define DIK_K 0x25
#define DIK_L 0x26
#define DIK_SEMICOLON 0x27
#define DIK_APOSTROPHE 0x28
#define DIK_GRAVE 0x29
#define DIK_LSHIFT 0x2A
#define DIK_BACKSLASH 0x2B
#define DIK_Z 0x2C
#define DIK_X 0x2D
#define DIK_C 0x2E
#define DIK_V 0x2F
#define DIK_B 0x30
#define DIK_N 0x31
#define DIK_M 0x32
#define DIK_COMMA 0x33
#define DIK_PERIOD 0x34
#define DIK_SLASH 0x35
#define DIK_RSHIFT 0x36
#define DIK_MULTIPLY 0x37
#define DIK_LMENU 0x38
#define DIK_SPACE 0x39
#define DIK_CAPITAL 0x3A
#define DIK_F1 0x3B
#define DIK_F2 0x3C
#define DIK_F3 0x3D
#define DIK_F4 0x3E
#define DIK_F5 0x3F
#define DIK_F6 0x40
#define DIK_F7 0x41
#define DIK_F8 0x42
#define DIK_F9 0x43
#define DIK_F10 0x44
#define DIK_NUMLOCK 0x45
#define DIK_SCROLL 0x46
#define DIK_NUMPAD7 0x47
#define DIK_NUMPAD8 0x48
#define DIK_NUMPAD9 0x49
#define DIK_SUBTRACT 0x4A
#define DIK_NUMPAD4 0x4B
#define DIK_NUMPAD5 0x4C
#define DIK_NUMPAD6 0x4D
#define DIK_ADD 0x4E
#define DIK_NUMPAD1 0x4F
#define DIK_NUMPAD2 0x50
#define DIK_NUMPAD3 0x51
#define DIK_NUMPAD0 0x52
#define DIK_DECIMAL 0x53
#define DIK_F11 0x57
#define DIK_F12 0x58
#define DIK_NUMPADENTER 0x9C
#define DIK_RCONTROL 0x9D
#define DIK_DIVIDE 0xB5
#define DIK_SYSRQ 0xB7
#define DIK_RMENU 0xB8
#define DIK_PAUSE 0xC5
#define DIK_HOME 0xC7
#define DIK_UP 0xC8
#define DIK_PRIOR 0xC9
#define DIK_LEFT 0xCB
#define DIK_RIGHT 0xCD
#define DIK_END 0xCF
#define DIK_DOWN 0xD0
#define DIK_NEXT 0xD1
#define DIK_INSERT 0xD2
#define DIK_DELETE 0xD3
#define DIK_LWIN 0xDB
#define DIK_RWIN 0xDC
#define DIK_APPS 0xDD
