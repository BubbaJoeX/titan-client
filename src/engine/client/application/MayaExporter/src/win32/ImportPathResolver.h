// ======================================================================
//
// ImportPathResolver.h
//
// ======================================================================

#ifndef INCLUDED_ImportPathResolver_H
#define INCLUDED_ImportPathResolver_H

// ======================================================================

#include <string>

// ======================================================================

/**
 * Resolve a user-provided import path to a full filesystem path.
 * Allows short paths like "appearance/skeleton/all_b.skt" instead of
 * full drive paths like "D:/titan/data/sku.0/sys.client/compiled/game/appearance/skeleton/all_b.skt".
 *
 * Resolution order for base directory:
 * - TITAN_DATA_ROOT environment variable (compiled game data)
 * - TITAN_EXPORT_ROOT environment variable (exported data)
 * - Parent of appearance write directory (if it's an absolute path)
 *
 * If the path is already absolute (has drive letter or starts with /), returns as-is.
 * If the path doesn't start with appearance/, shader/, effect/, or texture/, prepends "appearance/".
 */
std::string resolveImportPath(const std::string &path);

// ======================================================================

#endif
