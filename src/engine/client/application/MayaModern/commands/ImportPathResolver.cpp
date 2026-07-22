#include "ImportPathResolver.h"
#include "SetDirectoryCommand.h"
#include "MayaUtility.h"
#include "ConfigFile.h"

#include <maya/MGlobal.h>

#include <cstdlib>
#include <string>
#include <vector>

#ifdef _WIN32
#include <string.h>
#endif

static void stripTrailingAppearance(std::string& base)
{
    while (base.size() >= 10)
    {
        const auto pos = base.rfind("appearance/");
        if (pos != std::string::npos && pos + 10 == base.size())
        {
            base.resize(pos);
            while (!base.empty() && (base.back() == '/' || base.back() == '\\'))
                base.pop_back();
            if (!base.empty())
                base += '/';
        }
        else
            break;
    }
}

namespace
{
    bool isAbsolutePath(const std::string& path)
    {
        if (path.empty()) return false;
        if (path.size() >= 2 && path[1] == ':') return true;
        if (path[0] == '/') return true;
        return false;
    }

    bool hasTreeFilePrefix(const std::string& path)
    {
        if (path.size() >= 11 && (path.compare(0, 11, "appearance/") == 0 || path.compare(0, 11, "appearance\\") == 0))
            return true;
        if (path.size() >= 7 && (path.compare(0, 7, "shader/") == 0 || path.compare(0, 7, "shader\\") == 0))
            return true;
        if (path.size() >= 7 && (path.compare(0, 7, "effect/") == 0 || path.compare(0, 7, "effect\\") == 0))
            return true;
        if (path.size() >= 8 && (path.compare(0, 8, "texture/") == 0 || path.compare(0, 8, "texture\\") == 0))
            return true;
        return false;
    }

}

std::string getImportDataRoot()
{
    const char* envDataRoot = getenv("TITAN_DATA_ROOT");
    if (envDataRoot && envDataRoot[0])
    {
        std::string base = envDataRoot;
        for (auto& c : base) if (c == '\\') c = '/';
        if (!base.empty() && base.back() != '/') base += '/';
        stripTrailingAppearance(base);
        return base;
    }

    const char* envExportRoot = getenv("TITAN_EXPORT_ROOT");
    if (envExportRoot && envExportRoot[0])
    {
        std::string base = envExportRoot;
        for (auto& c : base) if (c == '\\') c = '/';
        if (!base.empty() && base.back() != '/') base += '/';
        stripTrailingAppearance(base);
        return base;
    }

    const char* envDataRootLegacy = getenv("DATA_ROOT");
    if (envDataRootLegacy && envDataRootLegacy[0])
    {
        std::string base = envDataRootLegacy;
        for (auto& c : base) if (c == '\\') c = '/';
        if (!base.empty() && base.back() != '/') base += '/';
        stripTrailingAppearance(base);
        return base;
    }

    const char* dataRootDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::DATA_ROOT_DIR_INDEX);
    if (dataRootDir && dataRootDir[0])
    {
        std::string base = dataRootDir;
        for (auto& c : base) if (c == '\\') c = '/';
        if (!base.empty() && base.back() != '/') base += '/';
        stripTrailingAppearance(base);
        return base;
    }

    const char* appearanceWriteDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::APPEARANCE_WRITE_DIR_INDEX);
    if (appearanceWriteDir && appearanceWriteDir[0] && appearanceWriteDir[1] == ':')
    {
        std::string base = appearanceWriteDir;
        for (auto& c : base) if (c == '\\') c = '/';
        while (!base.empty() && (base.back() == '/' || base.back() == '\\'))
            base.pop_back();
        const auto lastSlash = base.find_last_of('/');
        if (lastSlash != std::string::npos)
            return base.substr(0, lastSlash + 1);
    }

    return std::string();
}

std::string resolveImportPath(const std::string& path)
{
    if (path.empty()) return path;
    std::string result = path;
    for (auto& c : result) if (c == '\\') c = '/';
    if (isAbsolutePath(result)) return result;

    std::string baseDir = getImportDataRoot();
    if (baseDir.empty())
    {
        std::string treePath = result;
        if (!hasTreeFilePrefix(treePath))
            treePath = "appearance/" + treePath;
        const std::string probed = resolveGameAssetPath(treePath);
        if (!probed.empty())
            return probed;
        return result;
    }

    std::string treePath = result;
    if (!hasTreeFilePrefix(treePath))
        treePath = "appearance/" + treePath;

    const std::string combined = baseDir + treePath;
    if (MayaUtility::fileExists(combined))
        return combined;

    const std::string probed = resolveGameAssetPath(treePath);
    if (!probed.empty())
        return probed;

    return combined;
}

namespace
{
    std::string normalizeTreeRelForward(std::string rel)
    {
        for (char& c : rel)
            if (c == '\\')
                c = '/';
        while (!rel.empty() && (rel[0] == '/' || rel[0] == '\\'))
            rel.erase(0, 1);
        return rel;
    }

    std::string ensureTrailingSlashForward(std::string base)
    {
        if (base.empty())
            return base;
        for (char& c : base)
            if (c == '\\')
                c = '/';
        if (base.back() != '/')
            base += '/';
        return base;
    }

    void appendGameRootCandidate(std::vector<std::string>& out, std::string base)
    {
        if (base.empty())
            return;
        base = ensureTrailingSlashForward(std::move(base));
        stripTrailingAppearance(base);

        auto pushUnique = [&](const std::string& candidate) {
            if (candidate.empty())
                return;
            for (const std::string& existing : out)
            {
                if (existing == candidate)
                    return;
            }
            out.push_back(candidate);
        };

        pushUnique(base);

        if (base.find("compiled/game/") == std::string::npos)
            pushUnique(base + "sys.client/compiled/game/");

        if (base.find("compiled/game/") == std::string::npos && base.find("sys.client/compiled/game/") == std::string::npos)
            pushUnique(base + "compiled/game/");
    }

    std::vector<std::string> gameAssetRootCandidates()
    {
        std::vector<std::string> roots;

        const char* envNames[] = { "TITAN_DATA_ROOT", "DATA_ROOT", "TITAN_EXPORT_ROOT", nullptr };
        for (const char** p = envNames; *p; ++p)
        {
            const char* env = getenv(*p);
            if (env && env[0])
                appendGameRootCandidate(roots, env);
        }

        const char* cfg = ConfigFile::getKeyString("SwgMayaEditor", "gameDataRoot", "");
        if (cfg && cfg[0])
            appendGameRootCandidate(roots, cfg);

        static const char* kDefaultProbeRoots[] = {
            "D:/titan/data/sku.0/sys.client/compiled/game/",
            "D:/titan/data/sku.0/",
            "D:/swg/data/sku.0/sys.client/compiled/game/",
        };
        for (const char* probe : kDefaultProbeRoots)
        {
            std::string test = probe;
            test += "effect/a_simple.eft";
            if (MayaUtility::fileExists(test))
                appendGameRootCandidate(roots, probe);
        }

        return roots;
    }
}

std::string resolveGameAssetPath(const std::string& treeRel)
{
    const std::string rel = normalizeTreeRelForward(treeRel);
    if (rel.empty())
        return std::string();

    for (const std::string& root : gameAssetRootCandidates())
    {
        const std::string candidate = root + rel;
        if (MayaUtility::fileExists(candidate))
            return candidate;
        std::string candidateBs = candidate;
        for (char& c : candidateBs)
            if (c == '/')
                c = '\\';
        if (candidateBs != candidate && MayaUtility::fileExists(candidateBs))
            return candidateBs;
    }

    const std::string base = getImportDataRoot();
    if (!base.empty())
    {
        const std::string candidate = base + rel;
        if (MayaUtility::fileExists(candidate))
            return candidate;
        std::string candidateBs = candidate;
        for (char& c : candidateBs)
            if (c == '/')
                c = '\\';
        if (candidateBs != candidate && MayaUtility::fileExists(candidateBs))
            return candidateBs;
    }

    return std::string();
}

std::string resolveWindowsMayaAbsolutePath(const std::string& path)
{
#ifndef _WIN32
    return path;
#else
    if (path.empty())
        return path;
    if (MayaUtility::fileExists(path))
        return path;
    const size_t n = path.size();
    if (n >= 2 && path[1] == ':')
        return path;
    if (n >= 2 && path[0] == '/' && path[1] == '/')
        return path;

    if (path[0] != '/')
        return path;

    auto tryDrivePrefix = [&path](char d) -> std::string {
        const bool letter = (d >= 'A' && d <= 'Z') || (d >= 'a' && d <= 'z');
        if (!letter)
            return {};
        std::string c;
        c += d;
        c += ':';
        c += path;
        return MayaUtility::fileExists(c) ? c : std::string{};
    };

    const char* tw = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::TEXTURE_WRITE_DIR_INDEX);
    if (tw && tw[0] && tw[1] == ':')
    {
        const std::string fix = tryDrivePrefix(tw[0]);
        if (!fix.empty())
            return fix;
    }
    const char* dr = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::DATA_ROOT_DIR_INDEX);
    if (dr && dr[0] && dr[1] == ':')
    {
        const std::string fix = tryDrivePrefix(dr[0]);
        if (!fix.empty())
            return fix;
    }

    std::string base = getImportDataRoot();
    if (!base.empty())
    {
        while (!base.empty() && (base.back() == '/' || base.back() == '\\'))
            base.pop_back();
        std::string rel = path;
        while (!rel.empty() && (rel[0] == '/' || rel[0] == '\\'))
            rel.erase(0, 1);
        if (!rel.empty())
        {
            const std::string candidate = base + '/' + rel;
            if (MayaUtility::fileExists(candidate))
                return candidate;
            std::string candBs = candidate;
            for (char& ch : candBs)
                if (ch == '/')
                    ch = '\\';
            if (candBs != candidate && MayaUtility::fileExists(candBs))
                return candBs;
        }
    }

    return path;
#endif
}

namespace
{
    std::string normalizeToBackslashDataRoot(std::string base)
    {
        if (base.empty())
            return base;
        for (auto& c : base)
        {
            if (c == '/')
                c = '\\';
        }
        if (base.back() != '\\')
            base += '\\';
        return base;
    }

    std::string fileBasenameOnly(const std::string& p)
    {
        const size_t a = p.find_last_of("/\\");
        return (a == std::string::npos) ? p : p.substr(a + 1);
    }
}

std::string getExportedStagingDirectory()
{
    std::string base = getImportDataRoot();
    if (base.empty())
        return {};
    return normalizeToBackslashDataRoot(std::move(base)) + "exported\\";
}

bool mirrorExportToDataRootExported(const std::string& srcAbsolutePath, const std::string& destBasename)
{
    if (srcAbsolutePath.empty() || destBasename.empty())
        return false;
    if (!MayaUtility::fileExists(srcAbsolutePath))
        return false;
    const std::string destDir = getExportedStagingDirectory();
    if (destDir.empty())
        return false;
    MayaUtility::createDirectory(destDir.c_str());
    const std::string safeName = fileBasenameOnly(destBasename);
    if (safeName.empty())
        return false;
    const std::string dst = destDir + safeName;
    std::string srcNorm = srcAbsolutePath;
    std::string dstNorm = dst;
    for (auto& c : srcNorm)
        if (c == '/') c = '\\';
    for (auto& c : dstNorm)
        if (c == '/') c = '\\';
#ifdef _WIN32
    if (_stricmp(srcNorm.c_str(), dstNorm.c_str()) == 0)
        return true;
#endif
    if (!MayaUtility::copyFile(srcAbsolutePath, dst))
        return false;
    MGlobal::displayInfo(MString("[export] mirrored to exported: ") + dst.c_str());
    return true;
}
