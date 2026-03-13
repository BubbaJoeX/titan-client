// ======================================================================
//
// ExportArgs.cpp
// copyright 2003 Sony Online Entertainment
//
// ======================================================================

#include "FirstMayaExporter.h"
#include "ExportArgs.h"

#include <string>

// ======================================================================

// @NOTE: this sould all be lowercase because the comparison code converts the string to lower case
//		before doing the compare

const MString      ExportArgs::cs_allArgName                 = "-all";
const MString      ExportArgs::cs_animationStateGraphArgName = "-asg";
const MString      ExportArgs::cs_authorArgName              = "-author";
const MString      ExportArgs::cs_filenameArgName            = "-filename";
const MString      ExportArgs::cs_frameArgName               = "-frame";
const MString      ExportArgs::cs_ignoreBlendTargetsArgName  = "-ignoreblendtargets";
const MString      ExportArgs::cs_ignoreShadersArgName       = "-ignoreshaders";
const MString      ExportArgs::cs_ignoreTexturesArgName      = "-ignoretextures";
const MString      ExportArgs::cs_interactiveArgName         = "-interactive";
const MString      ExportArgs::cs_lockArgName                = "-lock";
const MString      ExportArgs::cs_meshGeneratorArgName       = "-mesh";
const MString      ExportArgs::cs_nameArgName                = "-name";
const MString      ExportArgs::cs_nodeArgName                = "-node";
const MString      ExportArgs::cs_noRevertOnFailArgName      = "-norevertonfail";
const MString      ExportArgs::cs_outputDirArgName           = "-outputdir";
const MString      ExportArgs::cs_outputFileNameArgName      = "-outputfile";
const MString      ExportArgs::cs_partOfOtherExportArgName   = "-partofotherexport";
const MString      ExportArgs::cs_skeletonArgName            = "-skeleton";
const MString      ExportArgs::cs_unlockArgName              = "-unlock";
const MString      ExportArgs::cs_warningsArgName            = "-warnings";
const MString      ExportArgs::cs_silentArgName              = "-silent";
const MString      ExportArgs::cs_showViewerAfterExport      = "-showviewerafterexport";
const MString      ExportArgs::cs_disableCompression         = "-nocompress";
const MString      ExportArgs::cs_fixPobCrc                  = "-fixpobcrc";


// ======================================================================
