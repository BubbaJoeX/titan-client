#ifndef SWGMAYAEDITOR_IMPORTPATHRESOLVER_H
#define SWGMAYAEDITOR_IMPORTPATHRESOLVER_H

#include <string>

/// Base directory for resolving tree paths (TITAN_DATA_ROOT, TITAN_EXPORT_ROOT, then setBaseDir / cfg).
std::string getImportDataRoot();

std::string resolveImportPath(const std::string& path);

#endif
