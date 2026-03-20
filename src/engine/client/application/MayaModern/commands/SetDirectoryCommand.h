#ifndef SWGMAYAEDITOR_SETDIRECTORYCOMMAND_H
#define SWGMAYAEDITOR_SETDIRECTORYCOMMAND_H

#include <maya/MPxCommand.h>

class SetDirectoryCommand
{
public:
    static void install();
    static void remove();

    static int registerDirectory(const char* title);
    static int getDirectoryId(const char* title);

    static const char* getDirectoryString(int directoryId);
    static void setDirectoryString(int directoryId, const char* newDirString);

    static void* creator();

    // Directory indices (must match register order)
    enum DirectoryIndex
    {
        APPEARANCE_WRITE_DIR_INDEX = 0,
        SHADER_TEMPLATE_WRITE_DIR_INDEX,
        TEXTURE_WRITE_DIR_INDEX,
        ANIMATION_WRITE_DIR_INDEX,
        SKELETON_TEMPLATE_WRITE_DIR_INDEX,
        LOG_DIR_INDEX,
        SAT_WRITE_DIR_INDEX,
        APPEARANCE_REFERENCE_DIR_INDEX,
        SHADER_TEMPLATE_REFERENCE_DIR_INDEX,
        EFFECT_REFERENCE_DIR_INDEX,
        TEXTURE_REFERENCE_DIR_INDEX,
        SKELETON_TEMPLATE_REFERENCE_DIR_INDEX,
        DATA_ROOT_DIR_INDEX,
        DIRECTORY_COUNT
    };
};

#endif
