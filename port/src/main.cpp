// Entry point of the portable build: finds the game and save folders,
// starts SDL and hands over to the game's WinMain the way the Windows
// startup code would.
//
//   th16 [--game-dir DIR] [--save-dir DIR] [--thcrap DIR]
//        [--thcrap-config NAME] [--no-thcrap] [--replay FILE] [DIR]
//
// The game folder (th16.dat, thbgm.dat; never written to) is DIR or
// --game-dir, else $TH16_DATA_DIR, else the current directory if it has
// th16.dat, else the executable's directory. The save folder (th16.cfg,
// scoreth16.dat, replays, snapshots, log.txt) is --save-dir, else
// $TH16_SAVE_DIR, else SDL's preference path (~/.local/share/th16-port on
// Linux, ~/Library/Application Support/th16-port on macOS). With thcrap
// support (TH16_THCRAP), a thcrap folder's patch stack is loaded: see
// thcrap/thcrap.h and NOTES.md, "thcrap". --replay FILE runs the replay
// sync test (port_replay_test.h) instead of a normal game, without thcrap.
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include <SDL.h>

#include <windows.h>

#include "port_platform.h"
#include "port_replay_test.h"
#include "port_vfs.h"
#ifdef TH16_THCRAP
#include "thcrap/thcrap.h"
#endif

static std::string absolute(const std::string &path)
{
    char resolved[PATH_MAX];
    if (realpath(path.c_str(), resolved) != NULL)
    {
        return resolved;
    }
    if (!path.empty() && path[0] == '/')
    {
        return path;
    }
    char cwd[PATH_MAX];
    return getcwd(cwd, sizeof(cwd)) != NULL ? std::string(cwd) + "/" + path : path;
}

static bool has_game_data(const std::string &dir)
{
    std::string path = dir + "/th16.dat";
    return access(path.c_str(), R_OK) == 0;
}

static void usage(const char *program)
{
    fprintf(stderr,
            "usage: %s [--game-dir DIR] [--save-dir DIR] [--thcrap DIR] [--thcrap-config NAME]\n"
            "          [--no-thcrap] [DIR]\n"
            "  DIR, --game-dir  the game's folder (th16.dat); also $TH16_DATA_DIR\n"
            "  --save-dir       where settings, scores and replays go; also $TH16_SAVE_DIR\n"
            "  --thcrap         a thcrap folder whose patches to apply; also $TH16_THCRAP_DIR\n"
            "                   (default: ~/.local/share/thcrap if it exists)\n"
            "  --thcrap-config  its run configuration (a name in config/ or a path); also\n"
            "                   $TH16_THCRAP_CONFIG (default: the newest one)\n"
            "  --no-thcrap      no patches (also TH16_THCRAP=0)\n"
            "  --replay FILE    play FILE (a .rpy) and check that it stays in sync, then exit\n"
            "                   (0 in sync, 1 desync, 2 did not finish); see NOTES.md, Testing\n",
            program);
}

int main(int argc, char **argv)
{
    std::string game_dir;
    std::string save_dir;
    std::string thcrap_dir;
    std::string thcrap_config;
    bool no_thcrap = false;
    std::string replay_file;
    std::string command_line;
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if ((arg == "--game-dir" || arg == "--data-dir") && i + 1 < argc)
        {
            game_dir = argv[++i];
        }
        else if (arg == "--save-dir" && i + 1 < argc)
        {
            save_dir = argv[++i];
        }
        else if (arg == "--thcrap" && i + 1 < argc)
        {
            thcrap_dir = argv[++i];
        }
        else if (arg == "--thcrap-config" && i + 1 < argc)
        {
            thcrap_config = argv[++i];
        }
        else if (arg == "--replay" && i + 1 < argc)
        {
            replay_file = argv[++i];
            // The replays were recorded without patches, and binary hacks
            // can change gameplay.
            no_thcrap = true;
        }
        else if (arg == "--no-thcrap")
        {
            no_thcrap = true;
        }
        else if (arg == "--help" || arg == "-h")
        {
            usage(argv[0]);
            return 0;
        }
        else if (arg[0] != '-' && game_dir.empty())
        {
            game_dir = arg;
        }
        else
        {
            // Anything else is passed on, as Windows would.
            if (!command_line.empty())
            {
                command_line += ' ';
            }
            command_line += arg;
        }
    }
    if (game_dir.empty() && getenv("TH16_DATA_DIR") != NULL)
    {
        game_dir = getenv("TH16_DATA_DIR");
    }
    if (game_dir.empty())
    {
        if (has_game_data("."))
        {
            game_dir = ".";
        }
        else if (char *base = SDL_GetBasePath())
        {
            game_dir = base;
            SDL_free(base);
        }
        else
        {
            game_dir = ".";
        }
    }
    game_dir = absolute(game_dir);
    if (!has_game_data(game_dir))
    {
        fprintf(stderr, "[th16-port] th16.dat not found in %s\n", game_dir.c_str());
        usage(argv[0]);
        // The game reports the missing archive itself.
    }
    if (save_dir.empty() && getenv("TH16_SAVE_DIR") != NULL)
    {
        save_dir = getenv("TH16_SAVE_DIR");
    }
    if (save_dir.empty())
    {
        if (char *pref = SDL_GetPrefPath("", "th16-port"))
        {
            save_dir = pref;
            SDL_free(pref);
        }
        else
        {
            const char *home = getenv("HOME");
            save_dir = std::string(home != NULL ? home : ".") + "/.local/share/th16-port";
        }
    }
    save_dir = absolute(save_dir);
    if (save_dir == game_dir)
    {
        fprintf(stderr, "[th16-port] the save folder must not be the game folder (%s)\n", save_dir.c_str());
        return 1;
    }
    if (!port_vfs_init(game_dir, save_dir))
    {
        return 1;
    }
    port_log("game folder %s, save folder %s", game_dir.c_str(), save_dir.c_str());
    if (!replay_file.empty() && !port_replay_test_init(replay_file.c_str(), save_dir.c_str()))
    {
        return 2;
    }
#ifdef TH16_THCRAP
    if (!no_thcrap)
    {
        port_thcrap_init(thcrap_dir.c_str(), thcrap_config.c_str());
    }
#else
    if (!thcrap_dir.empty() || !thcrap_config.empty())
    {
        port_log("this build has no thcrap support (TH16_THCRAP); no patches");
    }
    (void)no_thcrap;
#endif


    // DirectInput reads the pad with DISCL_BACKGROUND.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    // Alt+Tab out of full screen keeps the window.
    SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO) != 0)
    {
        port_log("SDL video failed to start (%s); running without a window", SDL_GetError());
        SDL_Init(SDL_INIT_EVENTS);
    }
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0)
    {
        port_log("SDL game controllers failed to start: %s", SDL_GetError());
    }
    port_log("SDL started (%s)", SDL_WasInit(SDL_INIT_VIDEO) ? SDL_GetCurrentVideoDriver() : "no video");

    int result = WinMain(GetModuleHandleA(NULL), NULL, &command_line[0], SW_SHOWNORMAL);

    SDL_Quit();
    return result;
}
