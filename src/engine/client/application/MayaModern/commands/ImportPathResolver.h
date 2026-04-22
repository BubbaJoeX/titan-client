#ifndef SWGMAYAEDITOR_IMPORTPATHRESOLVER_H
#define SWGMAYAEDITOR_IMPORTPATHRESOLVER_H

#include <string>

/// Base directory for resolving tree paths (TITAN_DATA_ROOT, TITAN_EXPORT_ROOT, then setBaseDir / cfg).
std::string getImportDataRoot();

std::string resolveImportPath(const std::string& path);

/// On Windows, Maya may resolve file textures to POSIX paths such as /exported/texture/foo.tga with no drive
/// letter, which breaks ``fopen`` / ``nvtt_export``. If the path does not exist but ``D:/exported/...`` style does
/// (drive taken from textureWriteDir / data root), returns that path.
std::string resolveWindowsMayaAbsolutePath(const std::string& path);

/// Staging folder under the data root: `<dataRoot>/exported/` (matches `setBaseDir` layout). Empty if no root configured.
std::string getExportedStagingDirectory();

/// Copies a file into `getExportedStagingDirectory()` as `destBasename` (basename only; path separators stripped). Creates the folder. Used to mirror ship bundles, `.mgn`, `.lsb`, etc. Returns false if skipped or copy failed.
bool mirrorExportToDataRootExported(const std::string& srcAbsolutePath, const std::string& destBasename);

#endif
