// gdi32: the text renderer. TextHelper.cpp draws dialogue, spell card names
// and menu text with GDI into a DIB section (CreateDIBSection with a
// BITMAPV4HEADER in the texture's pixel format, selected into a memory DC),
// then copies the pixels into a D3D texture. The strings are Shift-JIS
// bytes and the fonts are created with SHIFTJIS_CHARSET (MS Gothic, or
// Meiryo when EnumFontFamiliesExA finds it); an implementation converts the
// text (Shift-JIS to UTF-8, see NOTES.md) and rasterises it into the DIB's
// memory with a font library.
//
// Stubs for now: CreateDIBSection fails, which TextHelper handles.
#include <windows.h>

#include "port_stub.h"

extern "C" {

HFONT CreateFontA(int cHeight, int cWidth, int cEscapement, int cOrientation, int cWeight, DWORD bItalic,
                  DWORD bUnderline, DWORD bStrikeOut, DWORD iCharSet, DWORD iOutPrecision, DWORD iClipPrecision,
                  DWORD iQuality, DWORD iPitchAndFamily, LPCSTR pszFaceName)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

HFONT CreateFontIndirectA(const LOGFONTA *lplf)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

HGDIOBJ SelectObject(HDC hdc, HGDIOBJ h)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

BOOL DeleteObject(HGDIOBJ ho)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

HGDIOBJ GetStockObject(int i)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

HDC CreateCompatibleDC(HDC hdc)
{
    PORT_UNIMPLEMENTED();
    return NULL;
}

BOOL DeleteDC(HDC hdc)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

int GetDeviceCaps(HDC hdc, int index)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

HBITMAP CreateDIBSection(HDC hdc, const BITMAPINFO *pbmi, UINT usage, void **ppvBits, HANDLE hSection, DWORD offset)
{
    PORT_UNIMPLEMENTED();
    if (ppvBits != NULL)
    {
        *ppvBits = NULL;
    }
    return NULL;
}

COLORREF SetTextColor(HDC hdc, COLORREF color)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

COLORREF SetBkColor(HDC hdc, COLORREF color)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

int SetBkMode(HDC hdc, int mode)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

BOOL TextOutA(HDC hdc, int x, int y, LPCSTR lpString, int c)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL GetTextExtentPoint32A(HDC hdc, LPCSTR lpString, int c, LPSIZE psizl)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

BOOL GetTextMetricsA(HDC hdc, LPTEXTMETRICA lptm)
{
    PORT_UNIMPLEMENTED();
    return FALSE;
}

int EnumFontFamiliesExA(HDC hdc, LPLOGFONTA lpLogfont, FONTENUMPROCA lpProc, LPARAM lParam, DWORD dwFlags)
{
    PORT_UNIMPLEMENTED();
    return 0;
}

BOOL GdiFlush(void)
{
    return TRUE;
}

} // extern "C"
