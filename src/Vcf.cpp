#include "Vcf.h"
#include "ElsExt.h"
#include "Log.h"
#include "Xml.h"

#include <windows.h>
#include <cstdarg>
#include <initializer_list>

namespace
{
    constexpr int kMaxModels = 512;
    constexpr int kMaxReportedProblems = 12;
    constexpr size_t kMaxAudioStringLength = 63;    // ELS copies AudioString into 64 bytes unchecked
    constexpr int kMaxScanPoolPatterns = 300;       // and a scan pool into room for 300 patterns

    vcf::Model g_models[kMaxModels];
    int g_modelCount = 0;

    class Report
    {
    public:
        explicit Report(const char* file) : file_(file) {}

        void Add(const char* format, ...)
        {
            if (count_++ == 0) elslog::Line("  %s:", file_);
            if (count_ > kMaxReportedProblems) return;
            char text[256];
            va_list args;
            va_start(args, format);
            vsnprintf(text, sizeof(text), format, args);
            va_end(args);
            elslog::Line("    %s", text);
        }

        int Finish() const
        {
            if (count_ > kMaxReportedProblems) elslog::Line("    and %d more", count_ - kMaxReportedProblems);
            return count_;
        }

    private:
        const char* file_;
        int count_ = 0;
    };

    bool Section(Report& r, const xml::Element& root, const char* name, xml::Element* out)
    {
        if (xml::Find(root, name, out)) return true;
        r.Add("no <%s> section", name);
        return false;
    }

    void RequireText(Report& r, const xml::Element& section, const char* sectionName, const char* name)
    {
        xml::Element e;
        if (!xml::Find(section, name, &e)) r.Add("<%s> has no <%s>", sectionName, name);
        else if (!xml::HasText(e)) r.Add("<%s><%s> is empty", sectionName, name);
    }

    bool RequireAttributes(Report& r, const xml::Element& section, const char* sectionName, const char* name,
                           std::initializer_list<const char*> attributes, xml::Element* found = nullptr)
    {
        xml::Element e;
        if (!xml::Find(section, name, &e))
        {
            r.Add("<%s> has no <%s>", sectionName, name);
            return false;
        }
        for (const char* attribute : attributes)
            if (!xml::HasAttribute(e, attribute)) r.Add("<%s><%s> has no %s attribute", sectionName, name, attribute);
        if (found != nullptr) *found = e;
        return true;
    }

    void CheckLightGroup(Report& r, const xml::Element& root, const char* group, const char* groupAttribute,
                         std::initializer_list<const char*> presets)
    {
        xml::Element g;
        if (!Section(r, root, group, &g)) return;
        for (const char* attribute : { "LightingFormat", groupAttribute })
            if (attribute != nullptr && !xml::HasAttribute(g, attribute)) r.Add("<%s> has no %s attribute", group, attribute);

        char path[48];
        xml::Element patterns;
        snprintf(path, sizeof(path), "%s><PresetPatterns", group);
        if (!xml::Find(g, "PresetPatterns", &patterns)) r.Add("<%s> has no <PresetPatterns>", group);
        else for (const char* name : presets) RequireAttributes(r, patterns, path, name, { "Enabled", "Pattern" });

        snprintf(path, sizeof(path), "%s><ForcedPatterns", group);
        if (!xml::Find(g, "ForcedPatterns", &patterns)) r.Add("<%s> has no <ForcedPatterns>", group);
        else for (const char* name : { "MainHorn", "SrnTone1", "SrnTone2", "SrnTone3", "SrnTone4", "PanicMde", "OutOfVeh" })
            RequireAttributes(r, patterns, path, name, { "Enabled", "Pattern" });

        xml::Element pool;
        if (RequireAttributes(r, g, group, "ScanPatternCustomPool", { "Enabled", "Sequential" }, &pool))
        {
            int count = 0;
            xml::Element pattern;
            for (const char* at = pool.body; xml::Find(at, pool.bodyEnd, "Pattern", &pattern); at = pattern.tagEnd) ++count;
            if (count > kMaxScanPoolPatterns) r.Add("<%s><ScanPatternCustomPool> has more than %d patterns", group, kMaxScanPoolPatterns);
        }
    }

    void CheckRequiredSettings(Report& r, const xml::Element& root)
    {
        xml::Element s;
        if (Section(r, root, "INTERFACE", &s))
            for (const char* name : { "LstgActivationType", "DefaultSirenMode", "InfoPanelHeaderColor", "InfoPanelButtonLightColor" })
                RequireText(r, s, "INTERFACE", name);

        if (Section(r, root, "EOVERRIDE", &s))
        {
            for (int i = 1; i <= 12; ++i)
            {
                char name[8];
                snprintf(name, sizeof(name), "Extra%02d", i);
                if (i <= 9) RequireAttributes(r, s, "EOVERRIDE", name, { "IsElsControlled", "AllowEnvLight", "Color", "OffsetX", "OffsetY", "OffsetZ" });
                else RequireAttributes(r, s, "EOVERRIDE", name, { "IsElsControlled" });
            }
        }

        if (Section(r, root, "MISC", &s))
        {
            for (const char* name : { "VehicleIsSlicktop", "ArrowboardType", "UseSteadyBurnLights", "DfltSirenLtsActivateAtLstg" })
                RequireText(r, s, "MISC", name);
            RequireAttributes(r, s, "MISC", "Takedowns", { "AllowUse", "Mirrored" });
            RequireAttributes(r, s, "MISC", "SceneLights", { "AllowUse", "IlluminateSidesOnly" });
        }

        if (Section(r, root, "CRUISE", &s))
        {
            RequireText(r, s, "CRUISE", "DisableAtLstg3");
            RequireAttributes(r, s, "CRUISE", "UseExtras", { "Extra1", "Extra2", "Extra3", "Extra4" });
        }

        if (Section(r, root, "ACORONAS", &s))
            for (const char* name : { "Headlights", "TailLights", "IndicatorsF", "IndicatorsB", "ReverseLights" })
                RequireAttributes(r, s, "ACORONAS", name, { "DfltPattern", "ColorL", "ColorR" });

        if (Section(r, root, "SOUNDS", &s))
        {
            for (const char* name : { "MainHorn", "ManTone1", "ManTone2", "SrnTone1", "SrnTone2", "SrnTone3", "SrnTone4", "AuxSiren", "PanicMde" })
            {
                const bool horn = strcmp(name, "MainHorn") == 0;
                xml::Element e;
                if (!RequireAttributes(r, s, "SOUNDS", name, { horn ? "InterruptsSiren" : "AllowUse", "AudioString" }, &e)) continue;
                char audio[256];
                if (xml::Attribute(e, "AudioString", audio, sizeof(audio)) && strlen(audio) > kMaxAudioStringLength)
                    r.Add("<SOUNDS><%s> AudioString is longer than %zu characters", name, kMaxAudioStringLength);
            }
        }

        CheckLightGroup(r, root, "WRNL", nullptr, { "Lstg3" });
        CheckLightGroup(r, root, "PRML", "ExtrasActiveAtLstg2", { "Lstg2", "Lstg3" });
        CheckLightGroup(r, root, "SECL", "DisableAtLstg3", { "Lstg1", "Lstg2", "Lstg3" });
    }

    bool IsTrue(const char* text)
    {
        return _stricmp(text, "true") == 0 || _stricmp(text, "yes") == 0 || _stricmp(text, "on") == 0;
    }

    bool Enabled(const xml::Element& e)
    {
        char text[16];
        return !xml::Attribute(e, "Enabled", text, sizeof(text)) || IsTrue(text);
    }

    int32_t PatternAttribute(const xml::Element& e, const char* name)
    {
        char text[8];
        if (!xml::Attribute(e, name, text, sizeof(text))) return -1;
        const size_t length = strlen(text);
        if (length == 0 || length > 3) return -1;
        for (size_t i = 0; i < length; ++i)
            if (!isdigit(static_cast<unsigned char>(text[i]))) return -1;
        return atoi(text);
    }

    bool NumberAttribute(const xml::Element& e, const char* name, double min, double max, double* out)
    {
        char text[32];
        if (!xml::Attribute(e, name, text, sizeof(text))) return false;
        char* end = nullptr;
        const double value = strtod(text, &end);
        if (end == text || value < min || value > max) return false;
        *out = value;
        return true;
    }

    void CopyTrimmedLower(const char* text, char* out, size_t size)
    {
        while (isspace(static_cast<unsigned char>(*text))) ++text;
        size_t length = strlen(text);
        while (length > 0 && isspace(static_cast<unsigned char>(text[length - 1]))) --length;
        length = std::min(length, size - 1);
        for (size_t i = 0; i < length; ++i) out[i] = static_cast<char>(tolower(static_cast<unsigned char>(text[i])));
        out[length] = '\0';
    }

    bool ReadExtension(const xml::Element& root, vcf::Model& m)
    {
        xml::Element ext;
        if (!xml::Find(root, "EXT", &ext)) return false;

        xml::Element e;
        double number = 0.0;
        if (xml::Find(ext, "Park", &e) && Enabled(e))
        {
            m.park[ELSEXT_PRML] = PatternAttribute(e, "Prml");
            m.park[ELSEXT_SECL] = PatternAttribute(e, "Secl");
            m.park[ELSEXT_WRNL] = PatternAttribute(e, "Wrnl");
            if (NumberAttribute(e, "Delay", 0.0, 600.0, &number)) m.parkDelayMs = static_cast<int32_t>(number * 1000.0);
        }
        if (xml::Find(ext, "Advisor", &e))
        {
            m.advisor[0] = PatternAttribute(e, "Left");
            m.advisor[1] = PatternAttribute(e, "Right");
            m.advisor[2] = PatternAttribute(e, "Both");
        }
        if (xml::Find(ext, "Sync", &e) && Enabled(e))
        {
            m.sync = true;
            char group[sizeof(m.syncGroup)];
            if (xml::Attribute(e, "Group", group, sizeof(group))) CopyTrimmedLower(group, m.syncGroup, sizeof(m.syncGroup));
            if (NumberAttribute(e, "Radius", 1.0, 1000.0, &number)) m.syncRadius = static_cast<float>(number);
        }
        return m.HasPark() || m.HasAdvisor() || m.sync;
    }

    void AddModel(vcf::Model& m)
    {
        if (g_modelCount >= kMaxModels) return;
        m.hash = vcf::HashKey(m.name);
        g_models[g_modelCount++] = m;

        char park[48] = "no park";
        if (m.HasPark()) snprintf(park, sizeof(park), "park %d/%d/%d", m.park[0], m.park[1], m.park[2]);
        char sync[96] = "no sync";
        if (m.sync) snprintf(sync, sizeof(sync), "sync group '%s'", m.syncGroup[0] != '\0' ? m.syncGroup : "(model)");
        char advisor[48] = "";
        if (m.HasAdvisor()) snprintf(advisor, sizeof(advisor), ", advisor %d/%d/%d", m.advisor[0], m.advisor[1], m.advisor[2]);
        elslog::Line("  %s: %s, %s%s", m.name, park, sync, advisor);
    }

    // Returns the number of problems found in the file.
    int ReadVehicleFile(const char* path, const char* fileName)
    {
        Report report(fileName);
        size_t length = 0;
        char* text = xml::ReadFile(path, &length);
        if (text == nullptr)
        {
            report.Add("could not be read");
            return report.Finish();
        }

        xml::Element root;
        if (!xml::Find(text, text + length, "vcfroot", &root))
        {
            report.Add("has no <vcfroot> element");
        }
        else
        {
            CheckRequiredSettings(report, root);
            vcf::Model model;
            const char* dot = strrchr(fileName, '.');
            const size_t nameLength = dot != nullptr ? static_cast<size_t>(dot - fileName) : strlen(fileName);
            if (nameLength > 0 && nameLength < sizeof(model.name) && ReadExtension(root, model))
            {
                memcpy(model.name, fileName, nameLength);
                AddModel(model);
            }
        }
        free(text);
        return report.Finish();
    }

    // The VcfContainerFolder entry of ELS.ini, which may sit in any section.
    void ReadVcfFolderName(const char* gameFolder, char* out, size_t size)
    {
        strcpy_s(out, size, "pack_default");
        char path[MAX_PATH];
        if (snprintf(path, sizeof(path), "%sELS.ini", gameFolder) >= static_cast<int>(sizeof(path))) return;
        FILE* f = nullptr;
        if (fopen_s(&f, path, "r") != 0 || f == nullptr) return;

        constexpr char kKey[] = "VcfContainerFolder";
        char line[512];
        while (fgets(line, sizeof(line), f) != nullptr)
        {
            const char* p = line;
            while (isspace(static_cast<unsigned char>(*p))) ++p;
            if (!xml::SameNoCase(p, kKey, sizeof(kKey) - 1)) continue;
            p += sizeof(kKey) - 1;
            while (isspace(static_cast<unsigned char>(*p))) ++p;
            if (*p++ != '=') continue;
            while (isspace(static_cast<unsigned char>(*p))) ++p;
            size_t length = strcspn(p, "\r\n;/");
            while (length > 0 && isspace(static_cast<unsigned char>(p[length - 1]))) --length;
            if (length > 0 && length < size)
            {
                memcpy(out, p, length);
                out[length] = '\0';
            }
            break;
        }
        fclose(f);
    }
}

namespace vcf
{
    void Load(const char* gameFolder)
    {
        g_modelCount = 0;
        char folderName[64];
        ReadVcfFolderName(gameFolder, folderName, sizeof(folderName));

        char folder[MAX_PATH];
        char pattern[MAX_PATH];
        if (snprintf(folder, sizeof(folder), "%sELS\\%s\\", gameFolder, folderName) >= static_cast<int>(sizeof(folder)) ||
            snprintf(pattern, sizeof(pattern), "%s*.xml", folder) >= static_cast<int>(sizeof(pattern)))
            return;

        WIN32_FIND_DATAA found;
        HANDLE search = FindFirstFileA(pattern, &found);
        if (search == INVALID_HANDLE_VALUE)
        {
            elslog::Line("No vehicle files found in ELS\\%s.", folderName);
            return;
        }

        elslog::Line("Reading vehicle files in ELS\\%s.", folderName);
        int files = 0;
        int incomplete = 0;
        do
        {
            if (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
            char path[MAX_PATH];
            if (snprintf(path, sizeof(path), "%s%s", folder, found.cFileName) >= static_cast<int>(sizeof(path))) continue;
            ++files;
            if (ReadVehicleFile(path, found.cFileName) > 0) ++incomplete;
        } while (FindNextFileA(search, &found));
        FindClose(search);

        elslog::Line("Read %d vehicle files: %d with an <EXT> block, %d with missing settings.", files, g_modelCount, incomplete);
        if (incomplete > 0)
            elslog::Line("ELS reads these settings without checking for them and may fail while loading the files listed above.");
    }

    const Model* Find(uint32_t modelHash)
    {
        for (int i = 0; i < g_modelCount; ++i)
            if (g_models[i].hash == modelHash) return &g_models[i];
        return nullptr;
    }

    uint32_t HashKey(const char* text)
    {
        uint32_t hash = 0;
        for (const char* p = text; *p != '\0'; ++p)
        {
            hash += static_cast<uint32_t>(tolower(static_cast<unsigned char>(*p)));
            hash += hash << 10;
            hash ^= hash >> 6;
        }
        hash += hash << 3;
        hash ^= hash >> 11;
        hash += hash << 15;
        return hash;
    }
}
