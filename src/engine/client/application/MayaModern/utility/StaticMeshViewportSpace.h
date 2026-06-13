#ifndef SWGMAYAEDITOR_STATICMESHVIEWPORTSPACE_H
#define SWGMAYAEDITOR_STATICMESHVIEWPORTSPACE_H

#include "Vector.h"

#include <maya/MPoint.h>
#include <maya/MVector.h>

/// Single source of truth for static mesh (.msh) Maya viewport <-> SWG engine space.
/// Import and export must use the same helpers so re-import round-trips and the Viewer matches Maya.
class StaticMeshViewportSpace
{
public:
    /// Direct Maya UV in file (viewport). Legacy round-trip uses complementary 1-V storage.
    enum class UvStorage
    {
        ViewportDirect,
        LegacyOneMinusV
    };

    static Vector positionMayaToEngine(const MVector& maya);
    static MVector positionEngineToMaya(const Vector& engine);

    static Vector normalMayaToEngine(const MVector& maya);

    static void uvMayaToEngine(float u, float v, UvStorage storage, float& outU, float& outV);
    static void uvEngineToMaya(float u, float v, UvStorage storage, float& outU, float& outV);

    /// Pick (v0,v1,v2) vs (v0,v2,v1) so Viewer front faces match Maya's shaded outward side.
    /// Uses both Maya-object and engine-space checks because -X mirroring can disagree in one space only.
    static bool swapTriangleCornersForViewport(
        const MPoint& p0m, const MPoint& p1m, const MPoint& p2m, const MVector& faceNormalMaya);

    static UvStorage uvStorageFromExportOptions(bool objExportDirectUv);
};

#endif
