#ifndef SWGMAYAEDITOR_POBAUTHORINGSHARED_H
#define SWGMAYAEDITOR_POBAUTHORINGSHARED_H

#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>
#include <maya/MDagPath.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include <string>

class Transform;

namespace PobAuthoring
{
    bool isCellName(const MString& n);
    int countCellChildren(MObject node);
    MObject findPobRootFromPath(MDagPath path);

    /// Selection is a cell or its `portals` group.
    MStatus resolvePortalsGroupFromSelection(MDagPath& outPortalsPath);
    MStatus resolvePortalsGroupFromPathStr(const std::string& pathStr, MDagPath& outPortalsPath);

    int maxBuildingPortalIndexUnderRoot(MObject rootObj);
    int nextPortalChildSuffix(MObject portalsTransformObj);

    void addPortalAuthoringAttrs(MFnDependencyNode& transformDepFn, int portalIndex, bool clockwise,
                                 int targetCell, bool disabled, bool passable, const std::string& doorStyle);

    /// Door hardpoint for export: **data only** (bool + matrix), no DAG geometry/locator child.
    void applyDoorHardpointAttributes(MObject portalTransformObj, bool enabled, const Transform& doorTransform);

    /// Parses `r0`, `r12` -> 0, 12; otherwise -1.
    int cellIndexFromRName(const MString& n);

    MStatus getChildTransformByName(MObject parent, const char* shortName, MDagPath& outPath);

    /// Creates quad portal (2 tris), `portal` shape attr, authoring attrs, parents under `portalsObj`.
    MStatus createPortalDoorMeshAndParent(MObject portalsObj, const std::string& portalTransformName,
                                          float width, float height, int portalIndex, bool clockwise, int targetCell,
                                          bool disabled, bool passable, const std::string& doorStyle,
                                          bool addDoorHardpoint, MDagPath& outPortalTransformPath);

    MStatus resolvePobRootFromSelectionOrArg(const std::string& rootPathStr, MDagPath& outRootPath);
}

#endif
