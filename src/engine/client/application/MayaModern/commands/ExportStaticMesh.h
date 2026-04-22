#ifndef SWGMAYAEDITOR_EXPORTSTATICMESH_H
#define SWGMAYAEDITOR_EXPORTSTATICMESH_H

#include <maya/MPxCommand.h>
#include <maya/MString.h>

class ExportStaticMesh : public MPxCommand
{
public:
    static void* creator();
    MStatus doIt(const MArgList& args) override;

    /// Performs mesh export. Used by MshTranslator and MEL exportStaticMesh.
    /// legacyTriangleFlip*: SwgMsh dialog / -legacyTriangleFlip (triangle index order).
    /// objExportDirectUv*: SwgMsh dialog / -objExportDirectUv — direct Maya V (viewport). When both off, 1-V for .msh re-import round-trip.
    /// rawMshExportOptions: pass the full options string from the file translator so keys in the string override SwgMayaEditor.cfg.
    /// Optional on shadingEngine: swgExportUvDirect, swgExportTriangleSwap — override globals per material (combined .msh + OBJ meshes).
    bool performExport(const class MDagPath& meshDagPath, const std::string& outputPath,
        std::string& outMeshPath, std::string& outAptPath, bool legacyTriangleFlipFromCmd = false,
        bool legacyTriangleFlipFromFileDialog = false, bool objExportDirectUvFromCmd = false,
        bool objExportDirectUvFromExportDialog = false, const MString& rawMshExportOptions = MString());
};

#endif
