#include "SwgAnimationBrowser.h"

#include <maya/MArgList.h>
#include <maya/MGlobal.h>
#include <maya/MStatus.h>

void* SwgAnimationBrowser::creator()
{
    return new SwgAnimationBrowser();
}

MStatus SwgAnimationBrowser::doIt(const MArgList&)
{
    const char* initMel = R"mel(
string $pluginPath = `pluginInfo -q -path SwgMayaEditor`;
$pluginPath = `substituteAllString $pluginPath "\\" "/"`;
int $plen = `size $pluginPath`;
if ($plen == 0) {
    error "swgAnimationBrowser: SwgMayaEditor plugin path not found. Is the plugin loaded?";
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
string $melPath = $dir + "swgAnimationBrowser.mel";
if (!`filetest -f $melPath`) {
    error ("swgAnimationBrowser: MEL file not found: " + $melPath);
}
// Paths use / only (above); quoted source so spaces e.g. Program Files do not break parsing.
eval ("source \"" + $melPath + "\"");
swgAnimationBrowserShow;
)mel";

    MStatus st = MGlobal::executeCommand(initMel, true);
    if (!st) {
        MGlobal::displayError(
            "swgAnimationBrowser: Failed to source MEL or show window. Check Script Editor for details.");
        return st;
    }
    return MS::kSuccess;
}
