#include "SwgLightsaberToolkit.h"

#include <maya/MArgList.h>
#include <maya/MGlobal.h>
#include <maya/MStatus.h>

void* SwgLightsaberToolkit::creator()
{
    return new SwgLightsaberToolkit();
}

MStatus SwgLightsaberToolkit::doIt(const MArgList&)
{
    const char* initMel = R"mel(
string $pluginPath = `pluginInfo -q -path SwgMayaEditor`;
$pluginPath = `substituteAllString $pluginPath "\\" "/"`;
int $plen = `size $pluginPath`;
if ($plen == 0) {
    error "swgLightsaberToolkit: SwgMayaEditor plugin path not found. Is the plugin loaded?";
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
string $melPath = $dir + "swgLightsaberToolkit.mel";
if (!`filetest -f $melPath`) {
    error ("swgLightsaberToolkit: MEL file not found: " + $melPath);
}
eval ("source \"" + $melPath + "\"");
swgLightsaberToolkitShow;
)mel";

    MStatus st = MGlobal::executeCommand(initMel, true);
    if (!st)
    {
        MGlobal::displayError(
            "swgLightsaberToolkit: Failed to source MEL or show window. Check Script Editor for details.");
        return st;
    }
    return MS::kSuccess;
}
