// Runs the vehicle-file reader against a game folder and prints the log to the console.
//
// Usage: vcftest <game folder>
#include <cstdio>
#include <cstring>

#include "../src/Log.h"
#include "../src/Vcf.h"

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        printf("usage: vcftest <game folder>\n");
        return 2;
    }
    char folder[260];
    const size_t length = strlen(argv[1]);
    const bool needsSlash = length > 0 && argv[1][length - 1] != '\\' && argv[1][length - 1] != '/';
    snprintf(folder, sizeof(folder), "%s%s", argv[1], needsSlash ? "\\" : "");

    elslog::Open(nullptr);
    vcf::Load(folder);
    elslog::Close();
    return 0;
}
