#include "StaticMeshViewportSpace.h"
#include "MayaConversions.h"

#include <cmath>

namespace
{
    constexpr double kDegenerateAreaSq = 1e-18;

    bool mayaSpaceNeedsSwap(const MPoint& p0m, const MPoint& p1m, const MPoint& p2m, const MVector& faceNormalMaya)
    {
        const MVector e1 = p1m - p0m;
        const MVector e2 = p2m - p0m;
        const MVector g = e1 ^ e2;
        if (g.length() * g.length() < kDegenerateAreaSq)
            return false;
        return (g.normal() * faceNormalMaya) < 0.0;
    }

    bool engineSpaceNeedsSwap(const Vector& p0, const Vector& p1, const Vector& p2, const Vector& faceNormalEngine)
    {
        const Vector e1 = p1 - p0;
        const Vector e2 = p2 - p0;
        const Vector g = e1.cross(e2);
        if (g.magnitudeSquared() < static_cast<real>(kDegenerateAreaSq))
            return false;
        return g.dot(faceNormalEngine) < static_cast<real>(0);
    }
}

Vector StaticMeshViewportSpace::positionMayaToEngine(const MVector& maya)
{
    return MayaConversions::convertVector(maya);
}

MVector StaticMeshViewportSpace::positionEngineToMaya(const Vector& engine)
{
    return MVector(-static_cast<double>(engine.x), static_cast<double>(engine.y), static_cast<double>(engine.z));
}

Vector StaticMeshViewportSpace::normalMayaToEngine(const MVector& maya)
{
    return MayaConversions::convertVector(maya);
}

void StaticMeshViewportSpace::uvMayaToEngine(float u, float v, UvStorage storage, float& outU, float& outV)
{
    outU = u;
    outV = (storage == UvStorage::ViewportDirect) ? v : (1.0f - v);
}

void StaticMeshViewportSpace::uvEngineToMaya(float u, float v, UvStorage storage, float& outU, float& outV)
{
    outU = u;
    outV = (storage == UvStorage::ViewportDirect) ? v : (1.0f - v);
}

bool StaticMeshViewportSpace::swapTriangleCornersForViewport(
    const MPoint& p0m, const MPoint& p1m, const MPoint& p2m, const MVector& faceNormalMaya)
{
    const bool mayaSwap = mayaSpaceNeedsSwap(p0m, p1m, p2m, faceNormalMaya);

    const Vector p0 = positionMayaToEngine(MVector(p0m.x, p0m.y, p0m.z));
    const Vector p1 = positionMayaToEngine(MVector(p1m.x, p1m.y, p1m.z));
    const Vector p2 = positionMayaToEngine(MVector(p2m.x, p2m.y, p2m.z));
    const Vector nEng = normalMayaToEngine(faceNormalMaya);
    const bool engineSwap = engineSpaceNeedsSwap(p0, p1, p2, nEng);

    return mayaSwap || engineSwap;
}

StaticMeshViewportSpace::UvStorage StaticMeshViewportSpace::uvStorageFromExportOptions(bool objExportDirectUv)
{
    return objExportDirectUv ? UvStorage::ViewportDirect : UvStorage::LegacyOneMinusV;
}
