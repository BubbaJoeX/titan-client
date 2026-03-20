#include "SwgAssetDissector.h"

#include <maya/MArgList.h>
#include <maya/MGlobal.h>
#include <maya/MString.h>
#include <maya/MStatus.h>

const char* SwgAssetDissector::s_windowName = "swgAssetDissectorWindow";

void* SwgAssetDissector::creator()
{
    return new SwgAssetDissector();
}

MStatus SwgAssetDissector::doIt(const MArgList&)
{
    // Get plugin path, source swgAssetDissector.mel (next to .mll), then show window
    const char* initMel = R"mel(
string $pluginPath = `pluginInfo -q -path SwgMayaEditor`;
if (size($pluginPath) == 0) {
    error "SwgAssetDissector: SwgMayaEditor plugin path not found. Is the plugin loaded?";
}
int $lastSlash = 0;
for ($i = size($pluginPath); $i >= 1; $i--) {
    string $c = substring $pluginPath $i $i;
    if ($c == "/" || $c == "\\") {
        $lastSlash = $i;
        break;
    }
}
string $dir = ($lastSlash > 0) ? (substring $pluginPath 1 $lastSlash) : "";
string $melPath = $dir + "swgAssetDissector.mel";
if (!fileExists -loc $melPath) {
    error ("SwgAssetDissector: MEL file not found: " + $melPath);
}
source $melPath;
swgAssetDissectorShow;
)mel";

    MStatus st = MGlobal::executeCommand(initMel, true);
    if (!st) {
        MGlobal::displayError("SwgAssetDissector: Failed to source MEL or show window. Check Script Editor for details.");
        return st;
    }
    return MS::kSuccess;
}
