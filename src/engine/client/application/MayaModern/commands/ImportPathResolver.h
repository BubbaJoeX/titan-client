#ifndef SWGMAYAEDITOR_IMPORTPATHRESOLVER_H
#define SWGMAYAEDITOR_IMPORTPATHRESOLVER_H

#include <string>

/// Base directory for resolving tree paths (TITAN_DATA_ROOT, TITAN_EXPORT_ROOT, then setBaseDir / cfg).
std::string getImportDataRoot();

std::string resolveImportPath(const std::string& path);

/// Staging folder under the data root: `<dataRoot>/exported/` (matches `setBaseDir` layout). Empty if no root configured.
std::string getExportedStagingDirectory();

/// Copies a file into `getExportedStagingDirectory()` as `destBasename` (basename only; path separators stripped). Creates the folder. Used to mirror ship bundles, `.mgn`, `.lsb`, etc. Returns false if skipped or copy failed.
bool mirrorExportToDataRootExported(const std::string& srcAbsolutePath, const std::string& destBasename);

#endif
