// Empty WinMain: linking it pulls in the same static CRT startup code as
// th16.exe, which scripts/sigscan.py then looks for in the original.
#include <windows.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
    return 0;
}
