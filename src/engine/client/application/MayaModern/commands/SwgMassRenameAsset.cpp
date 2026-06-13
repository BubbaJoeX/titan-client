#include "SwgMassRenameAsset.h"

#include "MayaUtility.h"
#include "SetDirectoryCommand.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFnAttribute.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MItDag.h>
#include <maya/MObjectHandle.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include <algorithm>
#include <cctype>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
    struct MObjectHandleHash
    {
        size_t operator()(const MObjectHandle& h) const { return static_cast<size_t>(h.hashCode()); }
    };

    static std::string trim(std::string s)
    {
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r'))
            s.pop_back();
        size_t i = 0;
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t'))
            ++i;
        return s.substr(i);
    }

    static std::string replaceAll(std::string s, const std::string& from, const std::string& to)
    {
        if (from.empty())
            return s;
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos)
        {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
        return s;
    }

    static bool containsToken(const std::string& s, const std::string& token)
    {
        return !token.empty() && s.find(token) != std::string::npos;
    }

    static std::vector<std::string> splitFromTokens(const std::string& csv)
    {
        std::vector<std::string> out;
        std::string cur;
        for (char c : csv)
        {
            if (c == ',')
            {
                cur = trim(cur);
                if (!cur.empty())
                    out.push_back(cur);
                cur.clear();
            }
            else
                cur += c;
        }
        cur = trim(cur);
        if (!cur.empty())
            out.push_back(cur);
        return out;
    }

    static std::string applyFromTokensReplace(std::string s, const std::vector<std::string>& fromTokens, const std::string& to)
    {
        for (const std::string& f : fromTokens)
        {
            if (!f.empty())
                s = replaceAll(s, f, to);
        }
        return s;
    }

    static bool containsAnyToken(const std::string& s, const std::vector<std::string>& fromTokens)
    {
        for (const std::string& f : fromTokens)
        {
            if (containsToken(s, f))
                return true;
        }
        return false;
    }

    static void collectFromPlugInputs(MPlug plug, std::vector<MObject>& out, std::unordered_set<unsigned>& seen)
    {
        MPlugArray conns;
        if (plug.connectedTo(conns, true, false) != MS::kSuccess)
            return;
        for (unsigned i = 0; i < conns.length(); ++i)
        {
            MObject node = conns[i].node();
            if (node.isNull())
                continue;
            MObjectHandle h(node);
            const unsigned hc = h.hashCode();
            if (seen.count(hc))
                continue;
            seen.insert(hc);
            out.push_back(node);

            MStatus st;
            MFnDependencyNode fn(node, &st);
            if (!st)
                continue;
            const unsigned nAttrs = fn.attributeCount();
            for (unsigned ai = 0; ai < nAttrs; ++ai)
            {
                MObject attr = fn.attribute(ai);
                MFnAttribute attrFn(attr, &st);
                if (!st || !attrFn.isReadable())
                    continue;
                MPlug ap(node, attr);
                if (ap.isCompound())
                {
                    for (unsigned ci = 0; ci < ap.numChildren(); ++ci)
                        collectFromPlugInputs(ap.child(ci), out, seen);
                }
                else
                    collectFromPlugInputs(ap, out, seen);
            }
        }
    }

    static void collectShadingNetworkFromSg(MObject sgObj, std::vector<MObject>& out, std::unordered_set<unsigned>& seen)
    {
        if (sgObj.isNull())
            return;
        MObjectHandle h(sgObj);
        const unsigned hc = h.hashCode();
        if (seen.count(hc))
            return;
        seen.insert(hc);
        out.push_back(sgObj);

        MStatus st;
        MFnDependencyNode sgFn(sgObj, &st);
        if (!st)
            return;

        const char* upstreamPlugs[] = {"surfaceShader", "volumeShader", "displacementShader", "miMaterialShader"};
        for (const char* plugName : upstreamPlugs)
        {
            MPlug p = sgFn.findPlug(plugName, true);
            if (!p.isNull())
                collectFromPlugInputs(p, out, seen);
        }

        const unsigned nAttrs = sgFn.attributeCount();
        for (unsigned ai = 0; ai < nAttrs; ++ai)
        {
            MObject attr = sgFn.attribute(ai);
            MFnAttribute attrFn(attr, &st);
            if (!st || !attrFn.isReadable())
                continue;
            MPlug ap(sgObj, attr);
            if (ap.isCompound())
            {
                for (unsigned ci = 0; ci < ap.numChildren(); ++ci)
                    collectFromPlugInputs(ap.child(ci), out, seen);
            }
            else
                collectFromPlugInputs(ap, out, seen);
        }
    }

    static void collectDagSubtree(const MDagPath& root, std::vector<MObject>& out, std::unordered_set<unsigned>& seen)
    {
        MItDag it(MItDag::kDepthFirst, MFn::kInvalid);
        for (it.reset(root); !it.isDone(); it.next())
        {
            MObject obj = it.currentItem();
            if (obj.isNull())
                continue;
            MObjectHandle h(obj);
            const unsigned hc = h.hashCode();
            if (seen.count(hc))
                continue;
            seen.insert(hc);
            out.push_back(obj);

            MDagPath path;
            if (it.getPath(path) == MS::kSuccess && path.hasFn(MFn::kMesh))
            {
                MFnMesh meshFn(path);
                MObjectArray shaders;
                MIntArray indices;
                if (meshFn.getConnectedShaders(path.instanceNumber(), shaders, indices) == MS::kSuccess)
                {
                    for (unsigned si = 0; si < shaders.length(); ++si)
                        collectShadingNetworkFromSg(shaders[si], out, seen);
                }
            }
        }
    }

    static void collectSelectionRoots(MSelectionList& sel, std::vector<MDagPath>& outRoots)
    {
        outRoots.clear();
        for (unsigned i = 0; i < sel.length(); ++i)
        {
            MDagPath path;
            if (sel.getDagPath(i, path) != MS::kSuccess)
                continue;

            if (path.hasFn(MFn::kMesh))
            {
                MDagPath parent = path;
                parent.pop();
                outRoots.push_back(parent);
                continue;
            }

            if (path.hasFn(MFn::kTransform))
            {
                outRoots.push_back(path);
                continue;
            }
        }
    }

    static bool isStringPlug(MPlug plug)
    {
        MObject attr = plug.attribute();
        MFnTypedAttribute ta(attr);
        return ta.type() == MFnData::kString;
    }

    static bool updateStringPlugsOnNode(
        MObject obj,
        const std::vector<std::string>& fromTokens,
        const std::string& to,
        bool dryRun,
        bool clearStaleSwgTexturePath,
        int& attrCount)
    {
        MStatus st;
        MFnDependencyNode fn(obj, &st);
        if (!st)
            return false;

        bool changed = false;
        const unsigned nAttrs = fn.attributeCount();
        for (unsigned ai = 0; ai < nAttrs; ++ai)
        {
            MObject attr = fn.attribute(ai);
            MFnAttribute attrFn(attr, &st);
            if (!st || !attrFn.isReadable() || !attrFn.isWritable())
                continue;

            MPlug plug(obj, attr);
            if (plug.isCompound())
            {
                for (unsigned ci = 0; ci < plug.numChildren(); ++ci)
                {
                    MPlug child = plug.child(ci);
                    if (!isStringPlug(child))
                        continue;
                    MString val;
                    if (child.getValue(val) != MS::kSuccess)
                        continue;
                    const std::string oldStr = val.asChar();
                    if (!containsAnyToken(oldStr, fromTokens))
                        continue;
                    std::string newStr = applyFromTokensReplace(oldStr, fromTokens, to);
                    if (clearStaleSwgTexturePath && child.partialName() == MString("swgTexturePath")
                        && containsAnyToken(newStr, fromTokens))
                    {
                        newStr.clear();
                        MGlobal::displayWarning(MString("[swgMassRenameAsset] cleared stale swgTexturePath on ")
                            + fn.name() + " (still referenced old token after rename)");
                    }
                    ++attrCount;
                    changed = true;
                    if (!dryRun)
                        child.setValue(MString(newStr.c_str()));
                    MGlobal::displayInfo(MString("[swgMassRenameAsset] attr ") + fn.name() + "." + child.partialName()
                        + ": \"" + oldStr.c_str() + "\" -> \"" + newStr.c_str() + "\"");
                }
            }
            else
            {
                if (!isStringPlug(plug))
                    continue;
                MString val;
                if (plug.getValue(val) != MS::kSuccess)
                    continue;
                const std::string oldStr = val.asChar();
                if (!containsAnyToken(oldStr, fromTokens))
                    continue;
                std::string newStr = applyFromTokensReplace(oldStr, fromTokens, to);
                if (clearStaleSwgTexturePath && plug.partialName() == MString("swgTexturePath")
                    && containsAnyToken(newStr, fromTokens))
                {
                    newStr.clear();
                    MGlobal::displayWarning(MString("[swgMassRenameAsset] cleared stale swgTexturePath on ")
                        + fn.name() + " (still referenced old token after rename)");
                }
                ++attrCount;
                changed = true;
                if (!dryRun)
                    plug.setValue(MString(newStr.c_str()));
                MGlobal::displayInfo(MString("[swgMassRenameAsset] attr ") + fn.name() + "." + plug.partialName()
                    + ": \"" + oldStr.c_str() + "\" -> \"" + newStr.c_str() + "\"");
            }
        }
        return changed;
    }

    static bool renameNode(
        MObject obj,
        const std::vector<std::string>& fromTokens,
        const std::string& to,
        bool dryRun,
        int& renameCount)
    {
        MStatus st;
        MFnDependencyNode fn(obj, &st);
        if (!st)
            return false;

        const std::string oldName = fn.name().asChar();
        if (!containsAnyToken(oldName, fromTokens))
            return false;

        const std::string newName = applyFromTokensReplace(oldName, fromTokens, to);
        if (newName == oldName || newName.empty())
            return false;

        ++renameCount;
        if (!dryRun)
        {
            MStatus rs;
            fn.setName(MString(newName.c_str()), &rs);
            if (rs != MS::kSuccess)
            {
                MGlobal::displayWarning(MString("[swgMassRenameAsset] rename failed: ") + oldName.c_str() + " -> "
                    + newName.c_str());
                return false;
            }
        }
        MGlobal::displayInfo(MString("[swgMassRenameAsset] node: ") + oldName.c_str() + " -> " + newName.c_str());
        return true;
    }

    static void renameDiskFilesInDir(
        const std::string& dir,
        const std::vector<std::string>& fromTokens,
        const std::string& to,
        bool dryRun,
        int& fileCount)
    {
        if (dir.empty() || fromTokens.empty())
            return;

#ifdef _WIN32
        std::string pattern = dir;
        if (!pattern.empty() && pattern.back() != '/' && pattern.back() != '\\')
            pattern += '/';
        pattern += '*';

        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE)
            return;

        do
        {
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                continue;
            const std::string fileName = fd.cFileName;
            if (!containsAnyToken(fileName, fromTokens))
                continue;

            std::string src = dir;
            if (!src.empty() && src.back() != '/' && src.back() != '\\')
                src += '/';
            src += fileName;

            const std::string dstName = applyFromTokensReplace(fileName, fromTokens, to);
            std::string dst = dir;
            if (!dst.empty() && dst.back() != '/' && dst.back() != '\\')
                dst += '/';
            dst += dstName;

            ++fileCount;
            if (dryRun)
            {
                MGlobal::displayInfo(MString("[swgMassRenameAsset] disk (dry): ") + src.c_str() + " -> " + dst.c_str());
            }
            else if (MoveFileA(src.c_str(), dst.c_str()))
            {
                MGlobal::displayInfo(MString("[swgMassRenameAsset] disk: ") + src.c_str() + " -> " + dst.c_str());
            }
            else
            {
                MGlobal::displayWarning(MString("[swgMassRenameAsset] disk rename failed: ") + src.c_str());
            }
        } while (FindNextFileA(h, &fd));

        FindClose(h);
#endif
    }
}

void* SwgMassRenameAsset::creator()
{
    return new SwgMassRenameAsset();
}

MStatus SwgMassRenameAsset::doIt(const MArgList& args)
{
    MStatus st;
    std::string fromToken;
    std::string toToken;
    bool dryRun = false;
    bool renameDiskTextures = false;
    bool clearStaleSwgTexturePath = true;

    const unsigned n = args.length(&st);
    if (!st)
        return MS::kFailure;

    for (unsigned i = 0; i < n; ++i)
    {
        MString a = args.asString(i, &st);
        if (!st)
            return MS::kFailure;
        if ((a == "-from" || a == "-f") && i + 1 < n)
            fromToken = trim(args.asString(++i, &st).asChar());
        else if ((a == "-to" || a == "-t") && i + 1 < n)
            toToken = trim(args.asString(++i, &st).asChar());
        else if (a == "-dryRun" || a == "-dry")
            dryRun = true;
        else if (a == "-renameDiskTextures" || a == "-renameTextures")
            renameDiskTextures = true;
        else if (a == "-keepSwgTexturePath")
            clearStaleSwgTexturePath = false;
        else if (fromToken.empty() && !a.length())
            continue;
        else if (fromToken.empty() && toToken.empty() && i + 1 < n)
        {
            // Positional: swgMassRenameAsset edb_foo bubbajuice;
            fromToken = trim(a.asChar());
            toToken = trim(args.asString(++i, &st).asChar());
        }
    }

    if (fromToken.empty() || toToken.empty())
    {
        MGlobal::displayError(
            "swgMassRenameAsset: usage — select mesh transform(s), then "
            "swgMassRenameAsset -from \"npe_sign_medcenter,sign_medcenter\" -to thm_sign_evolve "
            "[-renameDiskTextures] [-keepSwgTexturePath] [-dryRun]");
        return MS::kFailure;
    }

    const std::vector<std::string> fromTokens = splitFromTokens(fromToken);
    if (fromTokens.empty())
    {
        MGlobal::displayError("swgMassRenameAsset: -from must contain at least one token.");
        return MS::kFailure;
    }

    if (fromTokens.size() == 1 && fromTokens[0] == toToken)
    {
        MGlobal::displayError("swgMassRenameAsset: -from and -to must differ.");
        return MS::kFailure;
    }

    MSelectionList sel;
    if (MGlobal::getActiveSelectionList(sel) != MS::kSuccess || sel.length() == 0)
    {
        MGlobal::displayError("swgMassRenameAsset: select the mesh transform (or its hierarchy) to rename.");
        return MS::kFailure;
    }

    std::vector<MDagPath> roots;
    collectSelectionRoots(sel, roots);
    if (roots.empty())
    {
        MGlobal::displayError("swgMassRenameAsset: selection must be transform(s) or mesh shape(s).");
        return MS::kFailure;
    }

    std::vector<MObject> nodes;
    std::unordered_set<unsigned> seen;
    for (const MDagPath& root : roots)
        collectDagSubtree(root, nodes, seen);

    // Longer names first reduces Maya rename collisions when one node name is a prefix of another.
    std::sort(nodes.begin(), nodes.end(), [](const MObject& a, const MObject& b) {
        MFnDependencyNode fa(a);
        MFnDependencyNode fb(b);
        return fa.name().length() > fb.name().length();
    });

    int attrUpdates = 0;
    int nodeRenames = 0;
    int diskRenames = 0;

    if (dryRun)
        MGlobal::displayInfo("[swgMassRenameAsset] dry run — no changes will be written.");

    // String attrs before node renames so logs still show the pre-rename node names.
    for (const MObject& obj : nodes)
        updateStringPlugsOnNode(obj, fromTokens, toToken, dryRun, clearStaleSwgTexturePath, attrUpdates);

    for (const MObject& obj : nodes)
        renameNode(obj, fromTokens, toToken, dryRun, nodeRenames);

    if (renameDiskTextures)
    {
        const char* texDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::TEXTURE_WRITE_DIR_INDEX);
        if (texDir && texDir[0])
            renameDiskFilesInDir(texDir, fromTokens, toToken, dryRun, diskRenames);
        else
            MGlobal::displayWarning("swgMassRenameAsset: textureWriteDir not set — run setBaseDir first for -renameDiskTextures.");
    }

    MString summary = MString("[swgMassRenameAsset] ") + (dryRun ? "would update " : "updated ")
        + fromToken.c_str() + " -> " + toToken.c_str() + ": " + nodeRenames + " node(s), " + attrUpdates
        + " string attr(s)";
    if (renameDiskTextures)
        summary += MString(", ") + diskRenames + " disk file(s) in textureWriteDir";
    MGlobal::displayInfo(summary);

    if (nodeRenames == 0 && attrUpdates == 0 && diskRenames == 0)
    {
        MGlobal::displayWarning(MString("[swgMassRenameAsset] no matches for token \"") + fromToken.c_str()
            + "\" under selection. Check -from spelling or select the imported mesh root.");
    }

    return MS::kSuccess;
}
