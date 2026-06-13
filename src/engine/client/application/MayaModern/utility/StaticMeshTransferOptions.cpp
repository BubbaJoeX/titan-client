#include "StaticMeshTransferOptions.h"
#include "ConfigFile.h"

#include <cctype>
#include <cstring>
#include <string>

namespace
{
    bool parseOptionBoolLastWin(const std::string& str, const char* keyEq, bool defaultWhenMissing, bool* outFound)
    {
        if (outFound)
            *outFound = false;
        const std::string key(keyEq);
        size_t scan = 0;
        bool found = false;
        bool value = defaultWhenMissing;
        while (scan < str.size())
        {
            const size_t pos = str.find(key, scan);
            if (pos == std::string::npos)
                break;
            size_t vpos = pos + key.size();
            while (vpos < str.size() && (str[vpos] == ' ' || str[vpos] == '\t'))
                ++vpos;
            if (vpos < str.size())
            {
                const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(str[vpos])));
                value = (c == '1' || c == 't' || c == 'y');
                found = true;
                if (outFound)
                    *outFound = true;
            }
            scan = pos + key.size();
        }
        return found ? value : defaultWhenMissing;
    }

    bool parseVisualHardpointsTokenized(const std::string& str, bool defaultWhenMissing)
    {
        const std::string keyEq = "visualHardpoints=";
        std::string normalized = str;
        for (char& ch : normalized)
        {
            if (ch == '\n' || ch == '\r')
                ch = ';';
        }
        bool foundKey = false;
        bool enabled = defaultWhenMissing;
        size_t start = 0;
        for (;;)
        {
            const size_t semi = normalized.find(';', start);
            std::string token = (semi == std::string::npos) ? normalized.substr(start) : normalized.substr(start, semi - start);
            size_t a = 0;
            size_t b = token.size();
            while (a < b && (token[a] == ' ' || token[a] == '\t'))
                ++a;
            while (b > a && (token[b - 1] == ' ' || token[b - 1] == '\t'))
                --b;
            if (a < b)
            {
                token = token.substr(a, b - a);
                if (token.size() >= keyEq.size() && token.compare(0, keyEq.size(), keyEq) == 0)
                {
                    size_t v = keyEq.size();
                    while (v < token.size() && (token[v] == ' ' || token[v] == '\t'))
                        ++v;
                    if (v < token.size())
                    {
                        const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(token[v])));
                        enabled = (c == '1' || c == 't' || c == 'y');
                        foundKey = true;
                    }
                }
            }
            if (semi == std::string::npos)
                break;
            start = semi + 1;
        }
        return foundKey ? enabled : defaultWhenMissing;
    }

    std::string normalizeOptionsString(const MString& options)
    {
        const char* cs = options.asChar();
        if (!cs || !cs[0])
            return std::string();
        return std::string(cs);
    }
}

StaticMeshTransferOptions StaticMeshTransferOptions::authoringDefaults()
{
    StaticMeshTransferOptions o;
    o.uvStorage = StaticMeshViewportSpace::UvStorage::LegacyOneMinusV;
    o.visualHardpoints = false;
    return o;
}

StaticMeshTransferOptions StaticMeshTransferOptions::fromSwgMshOptionsString(const MString& options)
{
    StaticMeshTransferOptions o = authoringDefaults();
    const std::string str = normalizeOptionsString(options);
    if (str.empty())
        return o;

    bool uvKeyFound = false;
    const bool directUv = parseOptionBoolLastWin(str, "objExportDirectUv=", false, &uvKeyFound);
    if (uvKeyFound)
        o.uvStorage = directUv ? StaticMeshViewportSpace::UvStorage::ViewportDirect
                               : StaticMeshViewportSpace::UvStorage::LegacyOneMinusV;
    else
        o.uvStorage = StaticMeshViewportSpace::UvStorage::LegacyOneMinusV;

    o.visualHardpoints = parseVisualHardpointsTokenized(str, false);

    // legacyTriangleFlip= is ignored (winding is automatic via StaticMeshViewportSpace).
    (void)parseOptionBoolLastWin(str, "legacyTriangleFlip=", false, nullptr);

    return o;
}

StaticMeshTransferOptions StaticMeshTransferOptions::fromExportCommandFlags(bool objExportDirectUvFlag)
{
    StaticMeshTransferOptions o = authoringDefaults();
    if (objExportDirectUvFlag)
    {
        o.uvStorage = StaticMeshViewportSpace::UvStorage::ViewportDirect;
        return o;
    }
    if (ConfigFile::getKeyBool("SwgMayaEditor", "staticMeshObjExportDirectUv", true))
        o.uvStorage = StaticMeshViewportSpace::UvStorage::ViewportDirect;
    else
        o.uvStorage = StaticMeshViewportSpace::UvStorage::LegacyOneMinusV;
    return o;
}

MString StaticMeshTransferOptions::toSwgMshOptionsString() const
{
    MString s("legacyTriangleFlip=1;objExportDirectUv=");
    s += usesViewportDirectUv() ? "1" : "0";
    s += ";visualHardpoints=";
    s += visualHardpoints ? "1" : "0";
    return s;
}

bool StaticMeshTransferOptions::swgMshOptionsKeySpecified(const MString& options, const char* keyEq)
{
    const char* cs = options.asChar();
    return cs != nullptr && std::strstr(cs, keyEq) != nullptr;
}
