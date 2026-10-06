#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "CriticalSections.h"
#include "GameErrorContext.h"

// GLOBAL: TH16 0x4a30a8
GameErrorContext g_GameErrorContext;

// FUNCTION: TH16 0x4029d0
const char *GameErrorContext::log(const char *fmt, ...)
{
    char tmp[0x2000];
    size_t tmp_size;
    va_list args;

    ENTER_CS(CS_GAME_ERROR_CONTEXT);
    va_start(args, fmt);
    vsprintf(tmp, fmt, args);

    tmp_size = strlen(tmp);

    if (this->buffer_end + tmp_size < &this->buffer[sizeof(this->buffer) - 1])
    {
        strcpy(this->buffer_end, tmp);

        this->buffer_end += tmp_size;
        *this->buffer_end = '\0';
    }

    va_end(args);
    LEAVE_CS(CS_GAME_ERROR_CONTEXT);

    return fmt;
}

// FUNCTION: TH16 0x402aa0
const char *GameErrorContext::fatal(const char *fmt, ...)
{
    char tmp[0x200];
    size_t tmp_size;
    va_list args;

    ENTER_CS(CS_GAME_ERROR_CONTEXT);
    va_start(args, fmt);
    vsprintf(tmp, fmt, args);

    tmp_size = strlen(tmp);

    if (this->buffer_end + tmp_size < &this->buffer[sizeof(this->buffer) - 1])
    {
        strcpy(this->buffer_end, tmp);

        this->buffer_end += tmp_size;
        *this->buffer_end = '\0';
    }

    va_end(args);
    this->show_message_box = true;
    LEAVE_CS(CS_GAME_ERROR_CONTEXT);

    return fmt;
}
