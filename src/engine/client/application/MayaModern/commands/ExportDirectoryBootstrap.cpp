#include "ExportDirectoryBootstrap.h"
#include "SetDirectoryCommand.h"
#include "ImportPathResolver.h"
#include "MayaUtility.h"

#include <iostream>
#include <string>

namespace
{
    std::string ensureTrailingBackslash(std::string s)
    {
        if (!s.empty() && s.back() != '\\' && s.back() != '/')
            s += '\\';
        return s;
    }

    std::string normalizeSlashesToBackslash(std::string s)
    {
        for (char& c : s)
            if (c == '/')
                c = '\\';
        return s;
    }

    bool copyBootstrapIfMissing(const std::string& treeRel, const std::string& destAbs)
    {
        if (MayaUtility::fileExists(destAbs))
            return true;

        std::string rel = treeRel;
        for (char& c : rel)
            if (c == '\\')
                c = '/';

        std::string src = resolveGameAssetPath(rel);
        if (src.empty())
            src = resolveImportPath(rel);
        if (src.empty() || !MayaUtility::fileExists(src))
        {
            std::cerr << "[ExportDirectoryBootstrap] Bootstrap source not found for " << treeRel
                      << " (set TITAN_DATA_ROOT or SwgMayaEditor.cfg gameDataRoot)\n";
            return false;
        }
        const size_t slash = destAbs.find_last_of("/\\");
        if (slash != std::string::npos)
        {
            const std::string dir = destAbs.substr(0, slash);
            MayaUtility::createDirectory(dir.c_str());
        }
        if (!MayaUtility::copyFile(src, destAbs))
            return false;
        std::cerr << "[ExportDirectoryBootstrap] Copied " << treeRel << " -> " << destAbs << "\n";
        return true;
    }

    std::string normalizeTreeRel(std::string rel)
    {
        while (!rel.empty() && (rel[0] == '/' || rel[0] == '\\'))
            rel.erase(0, 1);
        for (char& c : rel)
            if (c == '/')
                c = '\\';
        return rel;
    }

    std::string absPathUnderDataRoot(const std::string& treeRel)
    {
        const char* dr = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::DATA_ROOT_DIR_INDEX);
        if (!dr || !dr[0])
            return std::string();
        return ensureTrailingBackslash(normalizeSlashesToBackslash(dr)) + normalizeTreeRel(treeRel);
    }
}

std::string ExportDirectoryBootstrap::dataRootFromAppearanceDir(const std::string& appearanceRootDir)
{
    std::string ar = appearanceRootDir;
    while (!ar.empty() && (ar.back() == '/' || ar.back() == '\\'))
        ar.pop_back();
    if (ar.empty())
        return {};

    const size_t pos = ar.size() >= 10 ? ar.rfind("appearance") : std::string::npos;
    if (pos != std::string::npos)
    {
        if (pos == 0 || ar[pos - 1] == '/' || ar[pos - 1] == '\\')
        {
            const size_t after = pos + 10;
            if (after >= ar.size() || ar[after] == '/' || ar[after] == '\\')
                return ensureTrailingBackslash(ar.substr(0, pos));
        }
    }
    return ensureTrailingBackslash(ar);
}

void ExportDirectoryBootstrap::applyExportDataRoot(const std::string& baseDirectory)
{
    if (baseDirectory.empty())
        return;

    const std::string base = ensureTrailingBackslash(normalizeSlashesToBackslash(baseDirectory));

    const std::string appearanceWriteDir = base + "appearance\\";
    const std::string shaderTemplateWriteDir = base + "shader\\";
    const std::string textureWriteDir = base + "texture\\";
    const std::string animationWriteDir = appearanceWriteDir + "animation\\";
    const std::string skeletonTemplateWriteDir = appearanceWriteDir + "skeleton\\";
    const std::string meshWriteDir = appearanceWriteDir + "mesh\\";
    const std::string logWriteDir = base + "log\\";
    const std::string satWriteDir = appearanceWriteDir;

    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::APPEARANCE_WRITE_DIR_INDEX, appearanceWriteDir.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::SHADER_TEMPLATE_WRITE_DIR_INDEX, shaderTemplateWriteDir.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::TEXTURE_WRITE_DIR_INDEX, textureWriteDir.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::ANIMATION_WRITE_DIR_INDEX, animationWriteDir.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::SKELETON_TEMPLATE_WRITE_DIR_INDEX, skeletonTemplateWriteDir.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::LOG_DIR_INDEX, logWriteDir.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::SAT_WRITE_DIR_INDEX, satWriteDir.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::DATA_ROOT_DIR_INDEX, base.c_str());

    const std::string shaderRef = "shader/";
    const std::string effectRef = "effect/";
    const std::string textureRef = "texture/";
    const std::string skeletonRef = "appearance/skeleton/";
    const std::string appearanceRef = "appearance/";

    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::APPEARANCE_REFERENCE_DIR_INDEX, appearanceRef.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::SHADER_TEMPLATE_REFERENCE_DIR_INDEX, shaderRef.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::EFFECT_REFERENCE_DIR_INDEX, effectRef.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::TEXTURE_REFERENCE_DIR_INDEX, textureRef.c_str());
    SetDirectoryCommand::setDirectoryString(SetDirectoryCommand::SKELETON_TEMPLATE_REFERENCE_DIR_INDEX, skeletonRef.c_str());

    MayaUtility::createDirectory(appearanceWriteDir.c_str());
    MayaUtility::createDirectory(shaderTemplateWriteDir.c_str());
    MayaUtility::createDirectory(textureWriteDir.c_str());
    MayaUtility::createDirectory(animationWriteDir.c_str());
    MayaUtility::createDirectory(skeletonTemplateWriteDir.c_str());
    MayaUtility::createDirectory(meshWriteDir.c_str());
    MayaUtility::createDirectory(logWriteDir.c_str());
    MayaUtility::createDirectory(satWriteDir.c_str());
    MayaUtility::createDirectory((base + "effect\\").c_str());
    MayaUtility::createDirectory((base + "exported\\").c_str());

    bootstrapExportPrototypeAssets(base);
}

void ExportDirectoryBootstrap::bootstrapExportPrototypeAssets(const std::string& baseDirectory)
{
    const std::string base = ensureTrailingBackslash(normalizeSlashesToBackslash(baseDirectory));
    copyBootstrapIfMissing("shader/defaultshader.sht", base + "shader\\defaultshader.sht");
    copyBootstrapIfMissing("effect/a_simple.eft", base + "effect\\a_simple.eft");
}

bool ExportDirectoryBootstrap::bootstrapEffectTreeFile(const std::string& effectTreeRel)
{
    if (effectTreeRel.empty())
        return false;
    std::string rel = effectTreeRel;
    for (char& c : rel)
        if (c == '\\')
            c = '/';
    while (!rel.empty() && (rel[0] == '/' || rel[0] == '\\'))
        rel.erase(0, 1);
    if (rel.compare(0, 7, "effect/") != 0)
        rel = std::string("effect/") + rel;
    const std::string destAbs = absPathUnderDataRoot(rel);
    if (destAbs.empty())
        return false;
    return copyBootstrapIfMissing(rel, destAbs);
}

bool ExportDirectoryBootstrap::bootstrapTextureTreeFile(const std::string& textureTreeRel)
{
    if (textureTreeRel.empty())
        return false;
    std::string rel = textureTreeRel;
    for (char& c : rel)
        if (c == '\\')
            c = '/';
    while (!rel.empty() && (rel[0] == '/' || rel[0] == '\\'))
        rel.erase(0, 1);
    if (rel.compare(0, 8, "texture/") != 0)
        rel = std::string("texture/") + rel;
    const std::string destAbs = absPathUnderDataRoot(rel);
    if (destAbs.empty())
        return false;
    return copyBootstrapIfMissing(rel, destAbs);
}

void ExportDirectoryBootstrap::syncFromAppearanceRoot(const std::string& appearanceRootDir)
{
    const std::string root = dataRootFromAppearanceDir(appearanceRootDir);
    if (root.empty())
        return;
    applyExportDataRoot(root);
    std::cerr << "[ExportDirectoryBootstrap] Synced shader/texture dirs to export root: " << root << "\n";
}
