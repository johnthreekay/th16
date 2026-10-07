// Stand-in for the Windows SDK's shlobj.h: the shell link interfaces the
// startup code uses to resolve a .lnk it was started from. The port has no
// shell links; CoCreateInstance fails and the game takes its fallback path.
#pragma once

#include <windows.h>

typedef WCHAR OLECHAR;
typedef OLECHAR *LPOLESTR;
typedef const OLECHAR *LPCOLESTR;

struct IPersistFile : public IUnknown
{
    virtual HRESULT GetClassID(CLSID *pClassID) = 0;
    virtual HRESULT IsDirty() = 0;
    virtual HRESULT Load(LPCOLESTR pszFileName, DWORD dwMode) = 0;
    virtual HRESULT Save(LPCOLESTR pszFileName, BOOL fRemember) = 0;
    virtual HRESULT SaveCompleted(LPCOLESTR pszFileName) = 0;
    virtual HRESULT GetCurFile(LPOLESTR *ppszFileName) = 0;
};

struct IShellLinkA : public IUnknown
{
    virtual HRESULT GetPath(LPSTR pszFile, int cch, WIN32_FIND_DATAA *pfd, DWORD fFlags) = 0;
    virtual HRESULT SetPath(LPCSTR pszFile) = 0;
    virtual HRESULT Resolve(HWND hwnd, DWORD fFlags) = 0;
};

#define CSIDL_APPDATA 0x001a
#define CSIDL_LOCAL_APPDATA 0x001c
#define CSIDL_PERSONAL 0x0005
#define SHGFP_TYPE_CURRENT 0

extern "C" {
HRESULT SHGetFolderPathA(HWND hwnd, int csidl, HANDLE hToken, DWORD dwFlags, LPSTR pszPath);
BOOL SHGetSpecialFolderPathA(HWND hwnd, LPSTR pszPath, int csidl, BOOL fCreate);
}
