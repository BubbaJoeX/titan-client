#include "PobAuthoringShared.h"

#include "MayaSceneBuilder.h"
#include "Transform.h"

#include <maya/MFn.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnTransform.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MMatrix.h>

#include <cstdlib>
#include <cstdio>
#include <vector>

namespace PobAuthoring
{
    static void engineTransformToMayaMatrix(const Transform& t, MMatrix& out)
    {
        const Transform::matrix_t& mm = t.getMatrix();
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 4; ++col)
                out[row][col] = static_cast<double>(mm[row][col]);
        out[3][0] = 0.0;
        out[3][1] = 0.0;
        out[3][2] = 0.0;
        out[3][3] = 1.0;
    }

    void applyDoorHardpointAttributes(MObject portalTransformObj, bool enabled, const Transform& doorTransform)
    {
        MFnDependencyNode fn(portalTransformObj);

        MPlug enPl = fn.findPlug("doorHardpointEnabled", true);
        if (enPl.isNull())
        {
            MFnNumericAttribute na;
            MObject a = na.create("doorHardpointEnabled", "dhpEn", MFnNumericData::kBoolean);
            na.setStorable(true);
            fn.addAttribute(a);
            enPl = fn.findPlug("doorHardpointEnabled", true);
        }
        if (!enPl.isNull())
            enPl.setBool(enabled);

        MPlug matPl = fn.findPlug("doorHardpointMatrix", true);
        if (matPl.isNull())
        {
            MFnMatrixAttribute ma;
            MObject a = ma.create("doorHardpointMatrix", "dhm", MFnMatrixAttribute::kDouble);
            ma.setStorable(true);
            fn.addAttribute(a);
            matPl = fn.findPlug("doorHardpointMatrix", true);
        }
        MMatrix m;
        engineTransformToMayaMatrix(enabled ? doorTransform : Transform::identity, m);
        MFnMatrixData md;
        MObject mdObj = md.create(m);
        if (!matPl.isNull())
            matPl.setValue(mdObj);
    }

    bool isCellName(const MString& n)
    {
        if (n.length() < 2) return false;
        const char* s = n.asChar();
        return s[0] == 'r' && s[1] >= '0' && s[1] <= '9';
    }

    int countCellChildren(MObject node)
    {
        MFnDagNode fn(node);
        int c = 0;
        for (unsigned i = 0; i < fn.childCount(); ++i)
        {
            if (isCellName(MFnDagNode(fn.child(i)).name()))
                ++c;
        }
        return c;
    }

    MObject findPobRootFromPath(MDagPath path)
    {
        while (path.length() > 0)
        {
            if (countCellChildren(path.node()) >= 1)
                return path.node();
            path.pop();
        }
        return MObject::kNullObj;
    }

    MStatus resolvePortalsGroupFromSelection(MDagPath& outPortalsPath)
    {
        MSelectionList sel;
        MGlobal::getActiveSelectionList(sel);
        if (sel.length() == 0)
            return MS::kFailure;
        MDagPath selPath;
        if (sel.getDagPath(0, selPath) != MS::kSuccess)
            return MS::kFailure;
        MFnDagNode fn(selPath.node());
        if (fn.name() == "portals")
        {
            outPortalsPath = selPath;
            return MS::kSuccess;
        }
        for (unsigned i = 0; i < fn.childCount(); ++i)
        {
            MObject ch = fn.child(i);
            if (!ch.hasFn(MFn::kTransform)) continue;
            if (MFnDagNode(ch).name() == "portals")
            {
                MFnDagNode(ch).getPath(outPortalsPath);
                return MS::kSuccess;
            }
        }
        return MS::kFailure;
    }

    MStatus resolvePortalsGroupFromPathStr(const std::string& pathStr, MDagPath& outPortalsPath)
    {
        MSelectionList list;
        if (list.add(MString(pathStr.c_str())) != MS::kSuccess)
            return MS::kFailure;
        if (list.length() == 0) return MS::kFailure;
        if (list.getDagPath(0, outPortalsPath) != MS::kSuccess)
            return MS::kFailure;
        if (MFnDagNode(outPortalsPath.node()).name() != "portals")
            return MS::kFailure;
        return MS::kSuccess;
    }

    int maxBuildingPortalIndexUnderRoot(MObject rootObj)
    {
        int maxIdx = -1;
        MDagPath rootPath;
        if (MFnDagNode(rootObj).getPath(rootPath) != MS::kSuccess)
            return maxIdx;
        MItDag it(MItDag::kDepthFirst, MFn::kTransform);
        it.reset(rootPath, MItDag::kDepthFirst, MFn::kTransform);
        for (; !it.isDone(); it.next())
        {
            MDagPath p;
            if (it.getPath(p) != MS::kSuccess) continue;
            MFnDependencyNode dn(p.node());
            MPlug pl = dn.findPlug("buildingPortalIndex", true);
            if (pl.isNull()) continue;
            int v = 0;
            if (pl.getValue(v) != MS::kSuccess) continue;
            if (v > maxIdx) maxIdx = v;
        }
        return maxIdx;
    }

    int nextPortalChildSuffix(MObject portalsTransformObj)
    {
        MFnDagNode pfn(portalsTransformObj);
        int maxN = -1;
        for (unsigned i = 0; i < pfn.childCount(); ++i)
        {
            MString n = MFnDagNode(pfn.child(i)).name();
            if (n.length() < 2 || n.asChar()[0] != 'p') continue;
            const int val = std::atoi(n.asChar() + 1);
            if (val > maxN) maxN = val;
        }
        return maxN + 1;
    }

    void addPortalAuthoringAttrs(MFnDependencyNode& transformDepFn, int portalIndex, bool clockwise,
                                 int targetCell, bool disabled, bool passable, const std::string& doorStyle)
    {
        auto addIntAttr = [&](const char* name, int val) {
            MPlug p = transformDepFn.findPlug(name, true);
            if (p.isNull()) {
                MFnNumericAttribute nAttr;
                MObject a = nAttr.create(name, name, MFnNumericData::kInt);
                if (transformDepFn.addAttribute(a))
                    p = transformDepFn.findPlug(name, true);
            }
            if (!p.isNull()) p.setInt(val);
        };
        auto addBoolAttr = [&](const char* name, bool val) {
            MPlug p = transformDepFn.findPlug(name, true);
            if (p.isNull()) {
                MFnNumericAttribute nAttr;
                MObject a = nAttr.create(name, name, MFnNumericData::kBoolean);
                if (transformDepFn.addAttribute(a))
                    p = transformDepFn.findPlug(name, true);
            }
            if (!p.isNull()) p.setBool(val);
        };
        auto addStrAttr = [&](const char* name, const std::string& val) {
            MPlug p = transformDepFn.findPlug(name, true);
            if (p.isNull()) {
                MFnTypedAttribute tAttr;
                MObject a = tAttr.create(name, name, MFnData::kString);
                if (transformDepFn.addAttribute(a))
                    p = transformDepFn.findPlug(name, true);
            }
            if (!p.isNull()) p.setValue(MString(val.c_str()));
        };
        addIntAttr("buildingPortalIndex", portalIndex);
        addBoolAttr("portalClockwise", clockwise);
        addIntAttr("portalTargetCell", targetCell);
        addBoolAttr("portalDisabled", disabled);
        addBoolAttr("portalPassable", passable);
        addStrAttr("doorStyle", doorStyle);
    }

    int cellIndexFromRName(const MString& n)
    {
        if (!isCellName(n)) return -1;
        return std::atoi(n.asChar() + 1);
    }

    MStatus getChildTransformByName(MObject parent, const char* shortName, MDagPath& outPath)
    {
        MFnDagNode pfn(parent);
        for (unsigned i = 0; i < pfn.childCount(); ++i)
        {
            MObject ch = pfn.child(i);
            if (!ch.hasFn(MFn::kTransform)) continue;
            if (MFnDagNode(ch).name() == shortName)
                return MFnDagNode(ch).getPath(outPath);
        }
        return MS::kFailure;
    }

    MStatus createPortalDoorMeshAndParent(MObject portalsObj, const std::string& portalTransformName,
                                          float width, float height, int portalIndex, bool clockwise, int targetCell,
                                          bool disabled, bool passable, const std::string& doorStyle,
                                          bool addDoorHardpoint, MDagPath& outPortalTransformPath)
    {
        if (width <= 0.0f || height <= 0.0f)
            return MS::kFailure;

        const float hw = width * 0.5f;
        std::vector<float> positions = {
            hw, 0.0f, 0.0f,
            -hw, 0.0f, 0.0f,
            -hw, height, 0.0f,
            hw, height, 0.0f,
        };
        std::vector<float> normals(12u, 0.0f);
        MayaSceneBuilder::ShaderGroupData sg;
        sg.shaderTemplateName = "shader/placeholder";
        MayaSceneBuilder::TriangleData t0;
        t0.indices[0] = 0;
        t0.indices[1] = 1;
        t0.indices[2] = 2;
        MayaSceneBuilder::TriangleData t1;
        t1.indices[0] = 0;
        t1.indices[1] = 2;
        t1.indices[2] = 3;
        sg.triangles.push_back(t0);
        sg.triangles.push_back(t1);
        std::vector<MayaSceneBuilder::ShaderGroupData> groups(1, sg);

        MDagPath meshShapePath;
        MStatus status = MayaSceneBuilder::createMesh(positions, normals, groups, portalTransformName, meshShapePath);
        if (!status) return status;
        (void)MayaSceneBuilder::assignPobCollisionPreviewMaterial(meshShapePath);

        std::string shapeFullPath = meshShapePath.fullPathName().asChar();
        MString addCmd = "addAttr -ln portal -at bool \"";
        addCmd += shapeFullPath.c_str();
        addCmd += "\"";
        MGlobal::executeCommand(addCmd);
        MString setCmd = "setAttr \"";
        setCmd += shapeFullPath.c_str();
        setCmd += ".portal\" 1";
        MGlobal::executeCommand(setCmd);

        meshShapePath.pop(1);
        outPortalTransformPath = meshShapePath;
        MFnDependencyNode transformDepFn(outPortalTransformPath.node());
        addPortalAuthoringAttrs(transformDepFn, portalIndex, clockwise, targetCell, disabled, passable, doorStyle);

        MFnDagNode portalsParentFn(portalsObj);
        MString parentCmd = "parent \"";
        parentCmd += outPortalTransformPath.fullPathName();
        parentCmd += "\" \"";
        parentCmd += portalsParentFn.fullPathName();
        parentCmd += "\"";
        MGlobal::executeCommand(parentCmd);

        applyDoorHardpointAttributes(outPortalTransformPath.node(), addDoorHardpoint, Transform::identity);
        return MS::kSuccess;
    }

    MStatus resolvePobRootFromSelectionOrArg(const std::string& rootPathStr, MDagPath& outRootPath)
    {
        if (!rootPathStr.empty())
        {
            MSelectionList list;
            if (list.add(MString(rootPathStr.c_str())) != MS::kSuccess || list.length() == 0)
                return MS::kFailure;
            return list.getDagPath(0, outRootPath);
        }
        MSelectionList sel;
        MGlobal::getActiveSelectionList(sel);
        if (sel.length() == 0)
            return MS::kFailure;
        MDagPath anyPath;
        if (sel.getDagPath(0, anyPath) != MS::kSuccess)
            return MS::kFailure;
        MObject root = findPobRootFromPath(anyPath);
        if (root.isNull())
            return MS::kFailure;
        return MFnDagNode(root).getPath(outRootPath);
    }
}
