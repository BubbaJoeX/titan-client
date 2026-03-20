#include "ImportPathResolver.h"
#include "SetDirectoryCommand.h"

#include <cstdlib>
#include <string>

namespace
{
    void stripTrailingAppearance(std::string& base)
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

    bool isAbsolutePath(const std::string& path)
    {
        if (path.empty()) return false;
        if (path.size() >= 2 && path[1] == ':') return true;
        if (path[0] == '/') return true;
        return false;
    }

    bool hasTreeFilePrefix(const std::string& path)
    {
        if (path.size() >= 10 && (path.compare(0, 10, "appearance/") == 0 || path.compare(0, 10, "appearance\\") == 0))
            return true;
        if (path.size() >= 7 && (path.compare(0, 7, "shader/") == 0 || path.compare(0, 7, "shader\\") == 0))
            return true;
        if (path.size() >= 7 && (path.compare(0, 7, "effect/") == 0 || path.compare(0, 7, "effect\\") == 0))
            return true;
        if (path.size() >= 8 && (path.compare(0, 8, "texture/") == 0 || path.compare(0, 8, "texture\\") == 0))
            return true;
        return false;
    }

    std::string getImportBaseDir()
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
}

std::string resolveImportPath(const std::string& path)
{
    if (path.empty()) return path;
    std::string result = path;
    for (auto& c : result) if (c == '\\') c = '/';
    if (isAbsolutePath(result)) return result;

    std::string baseDir = getImportBaseDir();
    if (baseDir.empty()) return result;

    std::string treePath = result;
    if (!hasTreeFilePrefix(treePath))
        treePath = "appearance/" + treePath;

    return baseDir + treePath;
}
