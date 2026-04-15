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
$pluginPath = `substituteAllString $pluginPath "\\" "/"`;
int $plen = `size $pluginPath`;
if ($plen == 0) {
    error "SwgAssetDissector: SwgMayaEditor plugin path not found. Is the plugin loaded?";
}
int $lastSlash = 0;
int $i;
for ($i = $plen; $i >= 1; $i--) {
    string $c = `substring $pluginPath $i $i`;
    if ($c == "/" || $c == "\\") {
        $lastSlash = $i;
        break;
    }
}
string $dir = "";
if ($lastSlash > 0)
    $dir = `substring $pluginPath 1 $lastSlash`;
string $melPath = $dir + "swgAssetDissector.mel";
if (!`filetest -f $melPath`) {
    error ("SwgAssetDissector: MEL file not found: " + $melPath);
}
eval ("source \"" + $melPath + "\"");
swgAssetDissectorShow;
)mel";

    MStatus st = MGlobal::executeCommand(initMel, true);
    if (!st) {
        MGlobal::displayError("SwgAssetDissector: Failed to source MEL or show window. Check Script Editor for details.");
        return st;
    }
    return MS::kSuccess;
}
