// The COM basics the DirectX and shell interfaces build on: GUID, HRESULT,
// IUnknown and the COM library calls. In the SDK these come from
// objbase.h/unknwn.h through windows.h.
#pragma once

#include <stdint.h>
#include <string.h>

typedef int32_t HRESULT;

#define SUCCEEDED(hr) (((HRESULT)(hr)) >= 0)
#define FAILED(hr) (((HRESULT)(hr)) < 0)
#define MAKE_HRESULT(sev, fac, code) \
    ((HRESULT)(((uint32_t)(sev) << 31) | ((uint32_t)(fac) << 16) | ((uint32_t)(code))))

#define S_OK ((HRESULT)0)
#define S_FALSE ((HRESULT)1)
#define E_NOTIMPL ((HRESULT)0x80004001)
#define E_NOINTERFACE ((HRESULT)0x80004002)
#define E_POINTER ((HRESULT)0x80004003)
#define E_ABORT ((HRESULT)0x80004004)
#define E_FAIL ((HRESULT)0x80004005)
#define E_UNEXPECTED ((HRESULT)0x8000FFFF)
#define E_ACCESSDENIED ((HRESULT)0x80070005)
#define E_HANDLE ((HRESULT)0x80070006)
#define E_OUTOFMEMORY ((HRESULT)0x8007000E)
#define E_INVALIDARG ((HRESULT)0x80070057)
#define CO_E_NOTINITIALIZED ((HRESULT)0x800401F0)

typedef struct _GUID
{
    uint32_t Data1;
    uint16_t Data2;
    uint16_t Data3;
    uint8_t Data4[8];
} GUID;

typedef GUID IID;
typedef GUID CLSID;
typedef const GUID *LPCGUID;
typedef GUID *LPGUID;
typedef IID *LPIID;
typedef CLSID *LPCLSID;

#ifdef __cplusplus
#define REFGUID const GUID &
#define REFIID const IID &
#define REFCLSID const CLSID &

inline bool operator==(const GUID &a, const GUID &b)
{
    return memcmp(&a, &b, sizeof(GUID)) == 0;
}
inline bool operator!=(const GUID &a, const GUID &b)
{
    return !(a == b);
}
inline bool IsEqualGUID(const GUID &a, const GUID &b)
{
    return a == b;
}
#define IsEqualIID IsEqualGUID
#else
#define REFGUID const GUID *
#define REFIID const IID *
#define REFCLSID const CLSID *
#endif

// The SDK's DEFINE_GUID declares the GUID (and defines it with INITGUID).
// Here it only declares: the game defines the ids it uses itself
// (SupervisorSetup.cpp, DSUtil.cpp), and port/src/com_guids.cpp defines the
// rest.
#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) extern "C" const GUID name

DEFINE_GUID(GUID_NULL, 0x00000000, 0x0000, 0x0000, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00);

#define STDMETHODCALLTYPE
#define STDMETHOD(method) virtual HRESULT STDMETHODCALLTYPE method
#define STDMETHOD_(type, method) virtual type STDMETHODCALLTYPE method
#define PURE = 0

// IUnknown as a C++ abstract class. The platform layer implements the
// DirectX interfaces as classes deriving from these.
struct IUnknown
{
    virtual HRESULT QueryInterface(REFIID riid, void **ppvObject) = 0;
    virtual uint32_t AddRef() = 0;
    virtual uint32_t Release() = 0;
    virtual ~IUnknown() {}
};
typedef IUnknown *LPUNKNOWN;

#define CLSCTX_INPROC_SERVER 0x1
#define CLSCTX_INPROC_HANDLER 0x2
#define CLSCTX_LOCAL_SERVER 0x4
#define CLSCTX_ALL 0x17

#define STGM_READ 0x00000000
#define STGM_WRITE 0x00000001
#define STGM_READWRITE 0x00000002

extern "C" {
HRESULT CoInitialize(void *pvReserved);
void CoUninitialize(void);
HRESULT CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, uint32_t dwClsContext, REFIID riid, void **ppv);
}
