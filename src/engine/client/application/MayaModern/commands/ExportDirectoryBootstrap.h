#ifndef SWGMAYAEDITOR_EXPORTDIRECTORYBOOTSTRAP_H
#define SWGMAYAEDITOR_EXPORTDIRECTORYBOOTSTRAP_H

#include <string>

/// Configures SetDirectoryCommand write dirs (shader/, texture/, appearance/, …) from one export root.
/// Used by setBaseDir and SwgMsh export so mesh, shader, and texture always land on the same drive.
class ExportDirectoryBootstrap
{
public:
    /// e.g. D:/exported/appearance/ -> D:/exported/
    static std::string dataRootFromAppearanceDir(const std::string& appearanceRootDir);

    /// setBaseDir / performExport: set all write dirs and create folders under baseDirectory.
    static void applyExportDataRoot(const std::string& baseDirectory);

    /// Copy shader/defaultshader.sht and effect/a_simple.eft from game data when missing under export root.
    static void bootstrapExportPrototypeAssets(const std::string& baseDirectory);

    /// Copy effect/<name>.eft from game data into export effect/ when missing (Viewer parity).
    static bool bootstrapEffectTreeFile(const std::string& effectTreeRel);

    /// Copy texture/<name>.dds from game data into export texture/ when missing (prototype env/spec slots).
    static bool bootstrapTextureTreeFile(const std::string& textureTreeRel);

    /// Derive data root from appearanceRootDir and applyExportDataRoot.
    static void syncFromAppearanceRoot(const std::string& appearanceRootDir);
};

#endif
