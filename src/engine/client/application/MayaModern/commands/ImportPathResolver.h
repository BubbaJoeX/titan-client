#ifndef SWGMAYAEDITOR_IMPORTPATHRESOLVER_H
#define SWGMAYAEDITOR_IMPORTPATHRESOLVER_H

#include <string>

/// True when path already starts with a compiled/game tree folder (appearance/, shader/, clienteffect/, …).
bool hasTreeFilePrefix(const std::string& path);

/// Base directory for resolving tree paths (TITAN_DATA_ROOT, TITAN_EXPORT_ROOT, then setBaseDir / cfg).
std::string getImportDataRoot();

std::string resolveImportPath(const std::string& path);

/// Resolves effect/, texture/, shader/ paths against the original game asset tree (TITAN_DATA_ROOT,
/// sys.client/compiled/game/, etc.) even when setBaseDir points at an export folder. Returns an absolute
/// path when the file exists, otherwise empty.
std::string resolveGameAssetPath(const std::string& treeRel);

/// Absolute or tree path -> tree-relative (shader/foo.sht). Empty if no known tree prefix is found.
std::string toGameTreeRelativePath(const std::string& pathOrTree);

/// Resolve a tree or relative path to an existing file. Prefers compiled/game over TITAN_EXPORT_ROOT.
/// If the path has no tree prefix, appearance/ is prepended unless defaultPrefix is set.
std::string resolveTreeAssetPath(const std::string& treeRel, const std::string& anchorPath = std::string(),
    const char* defaultPrefix = "appearance/");

/// On Windows, Maya may resolve file textures to POSIX paths such as /exported/texture/foo.tga with no drive
/// letter, which breaks ``fopen`` / ``nvtt_export``. If the path does not exist but ``D:/exported/...`` style does
/// (drive taken from textureWriteDir / data root), returns that path.
std::string resolveWindowsMayaAbsolutePath(const std::string& path);

/// Staging folder under the data root: `<dataRoot>/exported/` (matches `setBaseDir` layout). Empty if no root configured.
std::string getExportedStagingDirectory();

/// Copies a file into `getExportedStagingDirectory()` as `destBasename` (basename only; path separators stripped). Creates the folder. Used to mirror ship bundles, `.mgn`, `.lsb`, etc. Returns false if skipped or copy failed.
bool mirrorExportToDataRootExported(const std::string& srcAbsolutePath, const std::string& destBasename);

#endif
