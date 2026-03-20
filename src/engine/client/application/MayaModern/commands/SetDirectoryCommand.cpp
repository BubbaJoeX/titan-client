#include "SetDirectoryCommand.h"
#include "ConfigFile.h"
#include "Globals.h"

#include <string>
#include <vector>
#include <map>

namespace
{
    std::vector<std::string> s_directories;
    std::map<std::string, int> s_titleToId;
    bool s_installed = false;

    const char* const CONFIG_SECTION = "SwgMayaEditor";
    const char* const DIR_KEYS[] = {
        "appearanceWriteDir",
        "shaderTemplateWriteDir",
        "textureWriteDir",
        "animationWriteDir",
        "skeletonTemplateWriteDir",
        "logDir",
        "satWriteDir",
        "appearanceReferenceDir",
        "shaderTemplateReferenceDir",
        "effectReferenceDir",
        "textureReferenceDir",
        "skeletonTemplateReferenceDir",
        "dataRootDir"
    };
    const char* const DIR_DEFAULTS[] = {
        "\\exported\\appearance\\",
        "\\exported\\shader\\",
        "\\exported\\texture\\",
        "\\exported\\appearance\\animation\\",
        "\\exported\\appearance\\skeleton\\",
        "\\exported\\log\\",
        "\\exported\\appearance\\",
        "appearance/",
        "shader/",
        "effect/",
        "texture/",
        "appearance/skeleton/",
        ""
    };
}

void SetDirectoryCommand::install()
{
    if (s_installed) return;
    s_installed = true;
    s_directories.resize(DIRECTORY_COUNT);
    for (int i = 0; i < DIRECTORY_COUNT; ++i)
    {
        const char* val = ConfigFile::getKeyString(CONFIG_SECTION, DIR_KEYS[i], DIR_DEFAULTS[i]);
        s_directories[i] = val ? val : DIR_DEFAULTS[i];
    }
}

void SetDirectoryCommand::remove()
{
    s_installed = false;
    s_directories.clear();
    s_titleToId.clear();
}

int SetDirectoryCommand::registerDirectory(const char* title)
{
    int id = static_cast<int>(s_directories.size());
    s_directories.push_back("");
    if (title) s_titleToId[title] = id;
    return id;
}

int SetDirectoryCommand::getDirectoryId(const char* title)
{
    auto it = s_titleToId.find(title);
    return (it != s_titleToId.end()) ? it->second : -1;
}

const char* SetDirectoryCommand::getDirectoryString(int directoryId)
{
    if (directoryId >= 0 && directoryId < static_cast<int>(s_directories.size()))
        return s_directories[directoryId].c_str();
    return "";
}

void SetDirectoryCommand::setDirectoryString(int directoryId, const char* newDirString)
{
    if (directoryId >= 0 && directoryId < static_cast<int>(s_directories.size()))
        s_directories[directoryId] = newDirString ? newDirString : "";
}

void* SetDirectoryCommand::creator()
{
    return nullptr;
}
