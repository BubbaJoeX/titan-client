#include "ImportPathResolver.h"
#include "SetDirectoryCommand.h"
#include "MayaUtility.h"

#include <maya/MGlobal.h>

#include <cstdlib>
#include <string>

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
    if (baseDir.empty()) return result;

    std::string treePath = result;
    if (!hasTreeFilePrefix(treePath))
        treePath = "appearance/" + treePath;

    return baseDir + treePath;
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
