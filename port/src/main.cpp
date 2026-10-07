// Entry point of the portable build: hands over to the game's WinMain the
// way the Windows startup code would.
#include <string>

#include <windows.h>

int main(int argc, char **argv)
{
    // WinMain gets the command line without the program name, as one string.
    std::string command_line;
    for (int i = 1; i < argc; i++)
    {
        if (i > 1)
        {
            command_line += ' ';
        }
        command_line += argv[i];
    }
    return WinMain(GetModuleHandleA(NULL), NULL, &command_line[0], SW_SHOWNORMAL);
}
