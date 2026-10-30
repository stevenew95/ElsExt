#include "Log.h"

#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <share.h>

namespace
{
    std::mutex g_lock;
    FILE* g_file = nullptr;
    bool g_ownsFile = false;
}

namespace elslog
{
    void Open(const char* path)
    {
        std::lock_guard lock(g_lock);
        if (g_file != nullptr) return;
        if (path == nullptr)
        {
            g_file = stdout;
            return;
        }
        // Shared read access so the log can be viewed while the game is running.
        g_file = _fsopen(path, "w", _SH_DENYWR);
        g_ownsFile = g_file != nullptr;
    }

    void Close()
    {
        std::lock_guard lock(g_lock);
        if (g_ownsFile) fclose(g_file);
        g_file = nullptr;
        g_ownsFile = false;
    }

    void Line(const char* format, ...)
    {
        std::lock_guard lock(g_lock);
        if (g_file == nullptr) return;

        SYSTEMTIME t;
        GetLocalTime(&t);
        fprintf(g_file, "[%02u:%02u:%02u.%03u] ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
        va_list args;
        va_start(args, format);
        vfprintf(g_file, format, args);
        va_end(args);
        fputc('\n', g_file);
        fflush(g_file);
    }
}
