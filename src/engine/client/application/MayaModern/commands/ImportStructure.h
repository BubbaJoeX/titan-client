#ifndef SWGMAYAEDITOR_IMPORTSTRUCTURE_H
#define SWGMAYAEDITOR_IMPORTSTRUCTURE_H

#include <maya/MPxCommand.h>

/// MEL: importStructure -i "appearance/building/foo" [-flr] [-shader "shader/a/b"]
/// Imports POB (if present), optional standalone FLR, shell mesh (same basename or appearance/mesh/<name>),
/// and optionally a shader template.
class ImportStructure : public MPxCommand
{
public:
	static void* creator();
	MStatus doIt(const MArgList& args) override;
};

#endif
