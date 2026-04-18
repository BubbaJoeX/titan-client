#include "SwgApplyWavefrontMtl.h"

#include "ConfigFile.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MGlobal.h>
#include <maya/MItDependencyNodes.h>
#include <maya/MIntArray.h>
#include <maya/MObjectArray.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace
{
    struct MtlMaterial
    {
        std::string name;
        std::string mapKd;
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

    static bool startsWithIgnoreCase(const std::string& s, const char* prefix)
    {
        const size_t n = strlen(prefix);
        if (s.size() < n) return false;
        for (size_t i = 0; i < n; ++i)
            if (tolower(static_cast<unsigned char>(s[i])) != tolower(static_cast<unsigned char>(prefix[i])))
                return false;
        return true;
    }

    static std::string toLowerCopy(std::string s)
    {
        for (char& c : s)
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        return s;
    }

    static std::string stripNamespaces(const std::string& mayaName)
    {
        size_t c = mayaName.rfind(':');
        if (c != std::string::npos && c + 1 < mayaName.size())
            return mayaName.substr(c + 1);
        return mayaName;
    }

    static bool pathLooksAbsolute(const std::string& p)
    {
        if (p.empty()) return false;
        if (p.size() >= 2 && p[1] == ':') return true;
        if (p[0] == '/' || p[0] == '\\') return true;
        return false;
    }

    static std::string joinDirFile(const std::string& dir, std::string file)
    {
        for (char& c : file)
            if (c == '\\') c = '/';
        if (dir.empty()) return file;
        char dlast = dir.back();
        if (dlast != '/' && dlast != '\\')
            return dir + '/' + file;
        return dir + file;
    }

    static std::string dirnameOfFile(const std::string& path)
    {
        size_t slash = path.find_last_of("/\\");
        if (slash == std::string::npos) return std::string();
        return path.substr(0, slash);
    }

    /// Wavefront map_* lines may include options before the filename; take the last non-option token.
    static std::string textureTokenFromMapLine(const std::string& line)
    {
        std::istringstream iss(line);
        std::string word;
        std::vector<std::string> tokens;
        while (iss >> word)
            tokens.push_back(word);
        for (int i = static_cast<int>(tokens.size()) - 1; i >= 0; --i)
        {
            if (tokens[static_cast<size_t>(i)].empty()) continue;
            if (tokens[static_cast<size_t>(i)][0] == '-') continue;
            return tokens[static_cast<size_t>(i)];
        }
        return std::string();
    }

    static std::string unquotePath(std::string p)
    {
        p = trim(std::move(p));
        if (p.size() >= 2 && p.front() == '"' && p.back() == '"')
            return p.substr(1, p.size() - 2);
        return p;
    }

    static std::string resolveTexturePath(const std::string& mtlDir, std::string raw)
    {
        raw = unquotePath(std::move(raw));
        if (raw.empty()) return std::string();
        for (char& c : raw)
            if (c == '\\') c = '/';
        if (pathLooksAbsolute(raw)) return raw;
        return joinDirFile(mtlDir, raw);
    }

    static bool parseMtlFile(const std::string& absPath, std::vector<MtlMaterial>& out)
    {
        out.clear();
        std::ifstream f(absPath.c_str(), std::ios::binary);
        if (!f)
            return false;

        const std::string mtlDir = dirnameOfFile(absPath);
        MtlMaterial cur;
        bool haveCur = false;

        std::string line;
        while (std::getline(f, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            line = trim(std::move(line));
            if (line.empty() || line[0] == '#')
                continue;

            if (startsWithIgnoreCase(line, "newmtl"))
            {
                if (haveCur && !cur.name.empty())
                    out.push_back(cur);
                cur = MtlMaterial();
                haveCur = true;
                cur.name = trim(line.substr(6));
                continue;
            }

            if (!haveCur || cur.name.empty())
                continue;

            if (startsWithIgnoreCase(line, "map_Kd"))
            {
                std::string tok = textureTokenFromMapLine(line);
                cur.mapKd = resolveTexturePath(mtlDir, tok);
            }
        }

        if (haveCur && !cur.name.empty())
            out.push_back(cur);

        return true;
    }

    static std::string defaultPrototypeShaderPath()
    {
        const char* cfg = ConfigFile::getKeyString("SwgMayaEditor", "shaderPrototypeSht", "");
        if (cfg && cfg[0])
        {
            std::string s(cfg);
            for (char& c : s)
                if (c == '\\') c = '/';
            return s;
        }
        return std::string("shader/defaultshader.sht");
    }

    static void setStringAttrOnNode(MFnDependencyNode& nodeFn, const char* longName, const std::string& val)
    {
        if (val.empty()) return;
        MPlug p = nodeFn.findPlug(longName, true);
        if (p.isNull())
        {
            MFnTypedAttribute tAttr;
            MObject a = tAttr.create(longName, longName, MFnData::kString);
            if (nodeFn.addAttribute(a))
            {
                p = nodeFn.findPlug(longName, true);
                if (!p.isNull()) p.setValue(MString(val.c_str()));
            }
        }
        else
            p.setValue(MString(val.c_str()));
    }

    static bool shadingGroupMatchesMtlName(const std::string& sgNameRaw, const std::string& mtlNameRaw)
    {
        const std::string sg = stripNamespaces(sgNameRaw);
        const std::string sgLower = toLowerCopy(sg);
        const std::string mtlLower = toLowerCopy(mtlNameRaw);

        if (sgLower == mtlLower)
            return true;
        if (sgLower.size() >= 2 && sgLower.substr(sgLower.size() - 2) == "sg")
        {
            const std::string stem = sgLower.substr(0, sgLower.size() - 2);
            if (stem == mtlLower)
                return true;
        }
        return false;
    }

    static std::string surfaceShaderShortName(const MObject& sgObj)
    {
        MFnDependencyNode sgFn(sgObj);
        MPlug surf = sgFn.findPlug("surfaceShader", true);
        if (surf.isNull()) return std::string();
        MPlugArray scon;
        surf.connectedTo(scon, true, false);
        if (scon.length() == 0) return std::string();
        MFnDependencyNode sfn(scon[0].node());
        return stripNamespaces(sfn.name().asChar());
    }

    static bool shadingEngineMatchesMaterial(MObject sgObj, const std::string& sgNameRaw, const std::string& mtlNameRaw)
    {
        if (shadingGroupMatchesMtlName(sgNameRaw, mtlNameRaw))
            return true;
        const std::string surf = surfaceShaderShortName(sgObj);
        if (!surf.empty() && toLowerCopy(surf) == toLowerCopy(mtlNameRaw))
            return true;
        return false;
    }

    static bool collectSelectedShadingGroupObjects(std::unordered_set<std::string>& outSgNames)
    {
        outSgNames.clear();
        MSelectionList sel;
        if (MGlobal::getActiveSelectionList(sel) != MS::kSuccess || sel.length() == 0)
            return false;

        for (unsigned i = 0; i < sel.length(); ++i)
        {
            MDagPath dag;
            if (sel.getDagPath(i, dag) != MS::kSuccess)
                continue;
            if (!dag.hasFn(MFn::kMesh))
            {
                if (dag.hasFn(MFn::kTransform))
                {
                    MFnDagNode dn(dag);
                    for (unsigned c = 0; c < dn.childCount(); ++c)
                    {
                        MObject ch = dn.child(c);
                        if (!ch.isNull() && ch.hasFn(MFn::kMesh))
                        {
                            dag.push(ch);
                            break;
                        }
                    }
                }
            }
            if (!dag.hasFn(MFn::kMesh))
                continue;

            MFnMesh meshFn(dag);
            MObjectArray shaders;
            MIntArray indices;
            if (meshFn.getConnectedShaders(dag.instanceNumber(), shaders, indices) != MS::kSuccess)
                continue;
            for (unsigned s = 0; s < shaders.length(); ++s)
            {
                MFnDependencyNode sgFn(shaders[s]);
                outSgNames.insert(sgFn.name().asChar());
            }
        }
        return !outSgNames.empty();
    }
}

void* SwgApplyWavefrontMtl::creator()
{
    return new SwgApplyWavefrontMtl();
}

MStatus SwgApplyWavefrontMtl::doIt(const MArgList& args)
{
    MStatus st;
    std::string mtlPath;
    std::string prototypeOverride;
    bool selectionOnly = false;

    const unsigned n = args.length(&st);
    if (!st) return MS::kFailure;
    for (unsigned i = 0; i < n; ++i)
    {
        MString a = args.asString(i, &st);
        if (!st) return MS::kFailure;
        if ((a == "-i" || a == "-path") && i + 1 < n)
        {
            mtlPath = args.asString(++i, &st).asChar();
            continue;
        }
        if ((a == "-shader" || a == "-prototype") && i + 1 < n)
        {
            prototypeOverride = args.asString(++i, &st).asChar();
            continue;
        }
        if (a == "-selectionOnly" || a == "-sel")
            selectionOnly = true;
    }

    if (mtlPath.empty())
    {
        MGlobal::displayError(
            "swgApplyWavefrontMtl: specify .mtl path, e.g. swgApplyWavefrontMtl -i \"D:/models/mat.mtl\"");
        return MS::kFailure;
    }

    for (char& c : mtlPath)
        if (c == '\\') c = '/';

    std::vector<MtlMaterial> materials;
    if (!parseMtlFile(mtlPath, materials))
    {
        MGlobal::displayError(MString("swgApplyWavefrontMtl: failed to read ") + mtlPath.c_str());
        return MS::kFailure;
    }

    if (materials.empty())
    {
        MGlobal::displayWarning(MString("swgApplyWavefrontMtl: no newmtl entries in ") + mtlPath.c_str());
        return MS::kSuccess;
    }

    std::unordered_set<std::string> allowedSg;
    if (selectionOnly && !collectSelectedShadingGroupObjects(allowedSg))
    {
        MGlobal::displayError(
            "swgApplyWavefrontMtl: -selectionOnly requires a mesh or mesh transform in the selection.");
        return MS::kFailure;
    }

    const std::string prototype = prototypeOverride.empty() ? defaultPrototypeShaderPath() : prototypeOverride;

    unsigned applied = 0;

    for (const MtlMaterial& mm : materials)
    {
        if (mm.name.empty())
            continue;

        bool foundThis = false;
        for (MItDependencyNodes it(MFn::kShadingEngine); !it.isDone(); it.next())
        {
            MObject sgObj = it.thisNode();
            MFnDependencyNode sgFn(sgObj);
            MString sgNameStr = sgFn.name();
            const std::string sgName = sgNameStr.asChar();

            if (sgName == "initialShadingGroup" || sgName == "initialParticleSE")
                continue;

            if (!shadingEngineMatchesMaterial(sgObj, sgName, mm.name))
                continue;

            if (selectionOnly && allowedSg.find(sgName) == allowedSg.end())
                continue;

            foundThis = true;

            setStringAttrOnNode(sgFn, "swgShaderPath", prototype);
            if (!mm.mapKd.empty())
                setStringAttrOnNode(sgFn, "swgTexturePath", mm.mapKd);

            ++applied;
            MGlobal::displayInfo(MString("swgApplyWavefrontMtl: ") + sgNameStr + " <- newmtl \"" +
                                   mm.name.c_str() + "\"" +
                                   (mm.mapKd.empty() ? "" : MString(" map_Kd ") + mm.mapKd.c_str()));
            break;
        }

        if (!foundThis)
        {
            std::cerr << "[swgApplyWavefrontMtl] No shading group matched newmtl \"" << mm.name << "\"\n";
        }
    }

    MString summary;
    summary += "swgApplyWavefrontMtl: ";
    summary += static_cast<int>(applied);
    summary += " shading group(s) updated. Prototype ";
    summary += prototype.c_str();
    summary += ". Run exportStaticMesh to emit .sht / texture/*.dds.";
    MGlobal::displayInfo(summary);

    if (applied == 0)
        MGlobal::displayWarning(
            "swgApplyWavefrontMtl: no shading groups matched .mtl names—compare Maya SG names (e.g. fooSG) "
            "to newmtl entries.");

    return MS::kSuccess;
}
