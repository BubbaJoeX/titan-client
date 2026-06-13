#ifndef SWGMAYAEDITOR_STATICMESHTRANSFEROPTIONS_H
#define SWGMAYAEDITOR_STATICMESHTRANSFEROPTIONS_H

#include "StaticMeshViewportSpace.h"

#include <maya/MString.h>

/// Single options object for SwgMsh import, export, and File > Export dialog.
/// Parsed once; no duplicate semicolon-string logic in msh.cpp / ExportStaticMesh.
struct StaticMeshTransferOptions
{
    /// Maya viewport UVs when re-importing .msh into Maya (objExportDirectUv=1). Exported .msh always uses legacy 1-V for Viewer.
    StaticMeshViewportSpace::UvStorage uvStorage = StaticMeshViewportSpace::UvStorage::LegacyOneMinusV;

    /// Import-only: create hp_* cube helpers. Export dialog passes this through for the next import.
    bool visualHardpoints = false;

    bool usesViewportDirectUv() const
    {
        return uvStorage == StaticMeshViewportSpace::UvStorage::ViewportDirect;
    }

    /// Default authoring: viewport UV, attribute-only hardpoints, automatic winding (StaticMeshViewportSpace).
    static StaticMeshTransferOptions authoringDefaults();

    /// Parse `legacyTriangleFlip=…;objExportDirectUv=…;visualHardpoints=…` (SwgMsh translator / import).
    static StaticMeshTransferOptions fromSwgMshOptionsString(const MString& options);

    /// MEL `exportStaticMesh` command flags + SwgMayaEditor.cfg when no options string is present.
    static StaticMeshTransferOptions fromExportCommandFlags(bool objExportDirectUvFlag);

    /// Serialize for import sidecars / logging (legacyTriangleFlip kept for old scripts; ignored on read).
    MString toSwgMshOptionsString() const;

    /// True when options string contains key (e.g. objExportDirectUv=).
    static bool swgMshOptionsKeySpecified(const MString& options, const char* keyEq);
};

#endif
