// Minimal reader for ELS vehicle configuration files. Element and attribute names are matched
// case-insensitively, as ELS does. Not a general XML parser.
#pragma once
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace xml
{
    inline bool SameNoCase(const char* a, const char* b, size_t n)
    {
        for (size_t i = 0; i < n; ++i)
            if (tolower(static_cast<unsigned char>(a[i])) != tolower(static_cast<unsigned char>(b[i]))) return false;
        return true;
    }

    // Blanks out comments in place so commented-out elements are not found.
    inline void StripComments(char* text)
    {
        for (char* p = strstr(text, "<!--"); p != nullptr; p = strstr(p, "<!--"))
        {
            const char* end = strstr(p + 4, "-->");
            const size_t length = end != nullptr ? static_cast<size_t>(end + 3 - p) : strlen(p);
            memset(p, ' ', length);
            p += length;
        }
    }

    // Reads a file of up to 4 MB with comments removed. The caller frees the result.
    inline char* ReadFile(const char* path, size_t* length)
    {
        FILE* f = nullptr;
        if (fopen_s(&f, path, "rb") != 0 || f == nullptr) return nullptr;
        fseek(f, 0, SEEK_END);
        const long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        char* text = size > 0 && size <= 4 * 1024 * 1024 ? static_cast<char*>(malloc(static_cast<size_t>(size) + 1)) : nullptr;
        if (text == nullptr)
        {
            fclose(f);
            return nullptr;
        }
        const size_t read = fread(text, 1, static_cast<size_t>(size), f);
        fclose(f);
        text[read] = '\0';
        StripComments(text);
        *length = read;
        return text;
    }

    struct Element
    {
        const char* attributes;     // text after the element name in the opening tag
        const char* tagEnd;         // the '>' that closes the opening tag
        const char* body;
        const char* bodyEnd;        // equal to body for a self-closing element
    };

    // Finds the first element called name in [from, to). An unclosed element extends to `to`.
    inline bool Find(const char* from, const char* to, const char* name, Element* out)
    {
        const size_t n = strlen(name);
        for (const char* p = from; p + n + 1 < to; ++p)
        {
            if (*p != '<' || !SameNoCase(p + 1, name, n)) continue;
            const char after = p[1 + n];
            if (!isspace(static_cast<unsigned char>(after)) && after != '>' && after != '/') continue;

            const char* tagEnd = static_cast<const char*>(memchr(p, '>', static_cast<size_t>(to - p)));
            if (tagEnd == nullptr) return false;
            out->attributes = p + 1 + n;
            out->tagEnd = tagEnd;
            out->body = out->bodyEnd = tagEnd + 1;
            if (tagEnd[-1] == '/') return true;

            out->bodyEnd = to;
            for (const char* q = out->body; q + n + 2 < to; ++q)
            {
                if (q[0] == '<' && q[1] == '/' && SameNoCase(q + 2, name, n))
                {
                    out->bodyEnd = q;
                    break;
                }
            }
            return true;
        }
        return false;
    }

    inline bool Find(const Element& parent, const char* name, Element* out)
    {
        return Find(parent.body, parent.bodyEnd, name, out);
    }

    // Copies the value of an attribute of the element's opening tag, truncated to fit.
    inline bool Attribute(const Element& e, const char* name, char* out, size_t size)
    {
        const size_t n = strlen(name);
        for (const char* p = e.attributes + 1; p + n < e.tagEnd; ++p)
        {
            if (!isspace(static_cast<unsigned char>(p[-1])) || !SameNoCase(p, name, n)) continue;
            const char* q = p + n;
            while (q < e.tagEnd && isspace(static_cast<unsigned char>(*q))) ++q;
            if (q >= e.tagEnd || *q != '=') continue;
            ++q;
            while (q < e.tagEnd && isspace(static_cast<unsigned char>(*q))) ++q;
            if (q >= e.tagEnd || (*q != '"' && *q != '\'')) continue;

            const char quote = *q++;
            const char* end = static_cast<const char*>(memchr(q, quote, static_cast<size_t>(e.tagEnd - q)));
            if (end == nullptr) return false;
            const size_t length = std::min(static_cast<size_t>(end - q), size - 1);
            memcpy(out, q, length);
            out[length] = '\0';
            return true;
        }
        return false;
    }

    inline bool HasAttribute(const Element& e, const char* name)
    {
        char unused[2];
        return Attribute(e, name, unused, sizeof(unused));
    }

    inline bool HasText(const Element& e)
    {
        for (const char* p = e.body; p < e.bodyEnd; ++p)
            if (!isspace(static_cast<unsigned char>(*p))) return true;
        return false;
    }
}
