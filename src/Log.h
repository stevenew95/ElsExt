#pragma once

namespace elslog
{
    // Opens the log, replacing an existing file. A null path writes to standard output.
    void Open(const char* path);
    void Close();

    // Writes one timestamped line. Safe to call from any thread, before Open and after Close.
    void Line(const char* format, ...);
}
