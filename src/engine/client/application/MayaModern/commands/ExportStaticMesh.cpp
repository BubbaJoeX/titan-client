#include "ExportStaticMesh.h"
#include "StaticMeshWriter.h"
#include "ImportPathResolver.h"
#include "SetDirectoryCommand.h"
#include "ShaderExporter.h"
#include "MayaUtility.h"
#include "MayaConversions.h"
#include "MayaCompoundString.h"
#include "Iff.h"
#include "Quaternion.h"
#include "Tag.h"
#include "Vector.h"
#include "ConfigFile.h"

#include <maya/MArgList.h>
#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MItDependencyGraph.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MObject.h>
#include <maya/MFloatPointArray.h>
#include <maya/MIntArray.h>
#include <maya/MPointArray.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MFileObject.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MTypes.h>
#include <maya/MVector.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace
{
    /// True when the SwgMsh export options string contains a given key (e.g. legacyTriangleFlip=).
    /// Used so explicit dialog values override SwgMayaEditor.cfg / mesh attrs when Maya passes the full options line.
    bool mshExportOptionsKeySpecified(const MString& opt, const char* keyEq)
    {
        const char* cs = opt.asChar();
        return cs != nullptr && std::strstr(cs, keyEq) != nullptr;
    }

    constexpr int DEFAULT_IFF_SIZE = 65536;

    const Tag TAG_APT = TAG3(A,P,T);
    const Tag TAG_DRTS = TAG(D,R,T,S);
    const Tag TAG_DRFT = TAG(D,R,F,T);
    const Tag TAG_DPPT = TAG(D,P,P,T);

    static std::string getStringAttr(MFnDependencyNode& fn, const char* attrName)
    {
        MPlug p = fn.findPlug(attrName, true);
        if (p.isNull()) return std::string();
        MString val;
        if (p.getValue(val) != MS::kSuccess) return std::string();
        return val.asChar();
    }

    /// Optional on shadingEngine: overrides File > Export SwgMsh UV/triangle options for faces using that SG.
    /// Use when one combined mesh mixes SWG .msh re-import shells (typically swgExportUvDirect=0) with OBJ shells (defaults / 1).
    static bool shadingGroupExportUvDirect(MObject shadingGroupObj, bool globalDefault)
    {
        MStatus st;
        MFnDependencyNode dep(shadingGroupObj, &st);
        if (!st) return globalDefault;
        if (!dep.hasAttribute("swgExportUvDirect")) return globalDefault;
        MPlug p = dep.findPlug("swgExportUvDirect", true, &st);
        if (!st || p.isNull()) return globalDefault;
        bool v = globalDefault;
        if (p.getValue(v) != MS::kSuccess) return globalDefault;
        return v;
    }

    /// Same override family: when absent, use global reverseTriangleIndices from dialog/cfg.
    static bool shadingGroupExportTriangleSwap(MObject shadingGroupObj, bool globalDefault)
    {
        MStatus st;
        MFnDependencyNode dep(shadingGroupObj, &st);
        if (!st) return globalDefault;
        if (!dep.hasAttribute("swgExportTriangleSwap")) return globalDefault;
        MPlug p = dep.findPlug("swgExportTriangleSwap", true, &st);
        if (!st || p.isNull()) return globalDefault;
        bool v = globalDefault;
        if (p.getValue(v) != MS::kSuccess) return globalDefault;
        return v;
    }

    static bool stringTruthy(const std::string& s)
    {
        if (s.empty()) return false;
        if (s == "1") return true;
        std::string l;
        l.reserve(s.size());
        for (unsigned char uc : s)
            l += static_cast<char>(std::tolower(uc));
        return l == "true" || l == "yes" || l == "on";
    }

    static void splitTabsHpLine(const std::string& line, std::vector<std::string>& cols)
    {
        cols.clear();
        size_t start = 0;
        for (;;)
        {
            const size_t tab = line.find('\t', start);
            if (tab == std::string::npos)
            {
                cols.push_back(line.substr(start));
                break;
            }
            cols.push_back(line.substr(start, tab - start));
            start = tab + 1;
        }
    }

    /** Same blob as `MayaSceneBuilder::storeSkmgHardpointsOnMeshTransform` / .mgn exporter. */
    static bool parseSwgSkmgHardpointsForStaticExport(const std::string& blob, std::vector<StaticMeshWriterHardpoint>& out)
    {
        out.clear();
        static const char hdr[] = "swgSkmgHp v1";
        constexpr size_t hdrLen = sizeof(hdr) - 1;
        if (blob.size() < hdrLen || blob.compare(0, hdrLen, hdr) != 0)
            return false;
        const size_t nl = blob.find('\n');
        if (nl == std::string::npos)
            return true;
        std::istringstream iss(blob.substr(nl + 1));
        std::string line;
        std::vector<std::string> cols;
        while (std::getline(iss, line))
        {
            if (line.empty())
                continue;
            splitTabsHpLine(line, cols);
            if (cols.size() < 5)
                continue;
            const char kind = cols[0].empty() ? 0 : cols[0][0];
            if (kind != 's' && kind != 'd')
                continue;
            float px = 0.f, py = 0.f, pz = 0.f;
            float rx = 0.f, ry = 0.f, rz = 0.f, rw = 1.f;
            if (std::sscanf(cols[3].c_str(), "%f %f %f", &px, &py, &pz) != 3)
                continue;
            if (std::sscanf(cols[4].c_str(), "%f %f %f %f", &rx, &ry, &rz, &rw) != 4)
                continue;
            StaticMeshWriterHardpoint hp;
            hp.name = cols[1];
            hp.position[0] = px;
            hp.position[1] = py;
            hp.position[2] = pz;
            hp.rotation[0] = rx;
            hp.rotation[1] = ry;
            hp.rotation[2] = rz;
            hp.rotation[3] = rw;
            out.push_back(hp);
        }
        return true;
    }

    /// SAT / msh import can tag hueable materials; clones shaderPrototypeHueableSht when true.
    static bool getHueableFromSg(MFnDependencyNode& sgFn)
    {
        if (stringTruthy(getStringAttr(sgFn, "swgHueable"))) return true;
        MPlug p = sgFn.findPlug("swgHueable", true);
        if (p.isNull()) return false;
        bool b = false;
        if (p.getValue(b) == MS::kSuccess) return b;
        int n = 0;
        if (p.getValue(n) == MS::kSuccess) return n != 0;
        return false;
    }

    static bool getTransparentOverrideFromSg(MFnDependencyNode& sgFn)
    {
        if (stringTruthy(getStringAttr(sgFn, "swgTransparent"))) return true;
        MPlug p = sgFn.findPlug("swgTransparent", true);
        if (p.isNull()) return false;
        bool b = false;
        if (p.getValue(b) == MS::kSuccess) return b;
        int n = 0;
        if (p.getValue(n) == MS::kSuccess) return n != 0;
        return false;
    }

    /// StandardSurface / aiStandardSurface: opacity 1 = opaque; lower or connected => blend.
    static bool opacityPlugIndicatesTransparency(MPlug op)
    {
        if (op.isNull()) return false;
        MPlugArray con;
        op.connectedTo(con, true, false);
        if (con.length() > 0) return true;
        if (op.isCompound())
        {
            double mn = 1.0;
            const unsigned nc = op.numChildren();
            const unsigned lim = nc < 3u ? nc : 3u;
            for (unsigned i = 0; i < lim; ++i)
            {
                double v = 1.0;
                op.child(i).getValue(v);
                if (v < mn) mn = v;
            }
            return mn < 0.999;
        }
        double s = 1.0;
        op.getValue(s);
        return s < 0.999;
    }

    /// Transmission-style weight: >0 or textured => treat like transparency for static shader clone (physical glass often leaves opacity at 1).
    static bool transmissionWeightPlugIndicatesTransparency(MPlug p)
    {
        if (p.isNull()) return false;
        MPlugArray con;
        p.connectedTo(con, true, false);
        if (con.length() > 0) return true;
        if (p.isCompound())
        {
            double mx = 0.0;
            const unsigned nc = p.numChildren();
            for (unsigned i = 0; i < nc && i < 4u; ++i)
            {
                double v = 0.0;
                p.child(i).getValue(v);
                if (v > mx) mx = v;
            }
            return mx > 1e-4;
        }
        double s = 0.0;
        p.getValue(s);
        return s > 1e-4;
    }

    static bool standardSurfaceTransmissionIndicatesGlass(MFnDependencyNode& surfaceFn)
    {
        static const char* const weightPlugs[] = {"transmission", "specularTransmission", "specular_transmission"};
        for (const char* name : weightPlugs)
        {
            MPlug w = surfaceFn.findPlug(name, true);
            if (!w.isNull() && transmissionWeightPlugIndicatesTransparency(w)) return true;
        }
        return false;
    }

    /// Lambert / Phong / Blinn: transparency (0,0,0)=opaque; white=transparent; connection => blend.
    static bool surfaceShaderIndicatesTransparency(MFnDependencyNode& surfaceFn)
    {
        const MString typ = surfaceFn.typeName();

        if (typ == MString("standardSurface") || typ == MString("aiStandardSurface"))
        {
            MPlug op = surfaceFn.findPlug("opacity", true);
            if (!op.isNull() && opacityPlugIndicatesTransparency(op)) return true;
            MPlug mat = surfaceFn.findPlug("matteOpacity", true);
            if (!mat.isNull() && opacityPlugIndicatesTransparency(mat)) return true;
            if (standardSurfaceTransmissionIndicatesGlass(surfaceFn)) return true;
            return false;
        }

        MPlug tr = surfaceFn.findPlug("transparency", true);
        if (tr.isNull()) return false;
        MPlugArray con;
        tr.connectedTo(con, true, false);
        if (con.length() > 0) return true;
        if (tr.isCompound())
        {
            double mx = 0.0;
            const unsigned nc = tr.numChildren();
            const unsigned lim = nc < 3u ? nc : 3u;
            for (unsigned i = 0; i < lim; ++i)
            {
                double v = 0.0;
                tr.child(i).getValue(v);
                if (v > mx) mx = v;
            }
            return mx > 1e-4;
        }
        return false;
    }

    static bool shadingGroupIndicatesTransparency(MObject shadingGroupObj)
    {
        MStatus st;
        MFnDependencyNode sgFn(shadingGroupObj, &st);
        if (!st) return false;
        if (getTransparentOverrideFromSg(sgFn)) return true;

        MPlug surfPlug = sgFn.findPlug("surfaceShader", true);
        if (surfPlug.isNull()) return false;
        MPlugArray scon;
        surfPlug.connectedTo(scon, true, false);
        if (scon.length() == 0) return false;
        MFnDependencyNode surfaceFn(scon[0].node(), &st);
        if (!st) return false;
        return surfaceShaderIndicatesTransparency(surfaceFn);
    }

    /// soe_effectName (static template writer) or legacy soe_effect — applied when cloning .sht if prototype has a NAME chunk ending in .eft.
    static std::string normalizedEffectPathFromSurfaceShader(MObject shadingGroupObj)
    {
        MStatus st;
        MFnDependencyNode sgFn(shadingGroupObj, &st);
        if (!st)
            return std::string();
        MPlug surfPlug = sgFn.findPlug("surfaceShader", true);
        if (surfPlug.isNull())
            return std::string();
        MPlugArray scon;
        surfPlug.connectedTo(scon, true, false);
        if (scon.length() == 0)
            return std::string();
        MFnDependencyNode surfaceFn(scon[0].node(), &st);
        if (!st)
            return std::string();
        std::string e = getStringAttr(surfaceFn, "soe_effectName");
        if (e.empty())
            e = getStringAttr(surfaceFn, "soe_effect");
        while (!e.empty() && (e[0] == '/' || e[0] == '\\'))
            e.erase(0, 1);
        for (char& c : e)
            if (c == '\\')
                c = '/';
        return e;
    }

    static bool diffuseImagePathOftenHasAlphaFile(const std::string& absPath)
    {
        size_t dot = absPath.find_last_of('.');
        if (dot == std::string::npos || dot + 1 >= absPath.size()) return false;
        std::string ext = absPath.substr(dot + 1);
        for (char& c : ext)
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        return ext == "png" || ext == "tga" || ext == "tif" || ext == "tiff";
    }

    static bool looksLikeFilesystemImagePathLocal(const std::string& p)
    {
        if (p.empty()) return false;
        if (p.find(':') != std::string::npos) return true;
        if (p[0] == '/' || p[0] == '\\') return true;
        return false;
    }

    /// File basename (no path, no extension) from swgTexturePath or "texture/foo" / "foo.dds".
    static std::string textureStemFromSwgOrTreePath(const std::string& raw)
    {
        if (raw.empty()) return std::string();
        std::string s = raw;
        for (char& c : s)
            if (c == '\\') c = '/';
        while (!s.empty() && (s[0] == '/' || s[0] == '\\'))
            s.erase(0, 1);
        const size_t slash = s.find_last_of('/');
        if (slash != std::string::npos)
            s = s.substr(slash + 1);
        if (s.size() > 4)
        {
            std::string tail = s.substr(s.size() - 4);
            for (char& c : tail)
                c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            if (tail == ".dds")
                s.resize(s.size() - 4);
        }
        return s;
    }

    /// First existing image in textureWriteDir named <baseName>.<ext> (artist drop-in for export).
    static std::string tryFindImageInTextureWriteDir(const std::string& baseNameNoExt)
    {
        if (baseNameNoExt.empty()) return std::string();
        const char* tw = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::TEXTURE_WRITE_DIR_INDEX);
        if (!tw || !tw[0]) return std::string();
        std::string dir(tw);
        if (!dir.empty() && dir.back() != '\\' && dir.back() != '/')
            dir += '\\';
        static const char* exts[] = { ".tga", ".png", ".jpg", ".jpeg", ".tif", ".tiff", ".bmp" };
        for (const char* ext : exts)
        {
            const std::string p = dir + baseNameNoExt + ext;
            FILE* f = fopen(p.c_str(), "rb");
            if (f)
            {
                fclose(f);
                return p;
            }
        }
        return std::string();
    }

    /// Normalizes swgTexturePath to texture/<base>.dds for TXM replacement (client loads DDS by this path).
    static std::string normalizeTextureTreeForShader(const std::string& s)
    {
        return ShaderExporter::ensureTextureTreePathForTxmName(s);
    }

    static std::string toTreePath(const std::string& path)
    {
        std::string p = path;
        for (size_t i = 0; i < p.size(); ++i)
            if (p[i] == '\\') p[i] = '/';
        // Strip leading backslashes and drive
        while (!p.empty() && (p[0] == '/' || p[0] == '\\')) p.erase(0, 1);
        size_t colon = p.find(':');
        if (colon != std::string::npos && colon > 0)
            p = p.substr(colon + 1);
        return p;
    }

    static std::string ensureTrailingSlash(const std::string& s)
    {
        if (s.empty()) return s;
        char c = s.back();
        if (c != '/' && c != '\\') return s + '\\';
        return s;
    }

    static std::string trimTrailingSeparatorsStr(std::string p)
    {
        while (!p.empty())
        {
            char c = p.back();
            if (c == '/' || c == '\\') p.pop_back();
            else break;
        }
        return p;
    }

    static std::string lastPathSegment(const std::string& path)
    {
        const std::string p = trimTrailingSeparatorsStr(path);
        const size_t pos = p.find_last_of("/\\");
        if (pos == std::string::npos) return p;
        return p.substr(pos + 1);
    }

    static std::string parentPathStr(const std::string& path)
    {
        const std::string p = trimTrailingSeparatorsStr(path);
        const size_t pos = p.find_last_of("/\\");
        if (pos == std::string::npos) return std::string();
        return p.substr(0, pos);
    }

    static bool iequalsSeg(const std::string& a, const std::string& b)
    {
#ifdef _WIN32
        return _stricmp(a.c_str(), b.c_str()) == 0;
#else
        if (a.size() != b.size()) return false;
        for (size_t i = 0; i < a.size(); ++i)
        {
            if (std::tolower(static_cast<unsigned char>(a[i])) !=
                std::tolower(static_cast<unsigned char>(b[i])))
                return false;
        }
        return true;
#endif
    }

    static std::string winPathForMkdir(const std::string& s)
    {
#ifdef _WIN32
        std::string p = s;
        for (char& c : p)
            if (c == '/') c = '\\';
        return p;
#else
        return s;
#endif
    }

    /// Maya namespaces use group:shape; ':' is invalid in Windows filenames for .sht / .msh / .apt.
    static std::string replaceColonsForAssetFileName(std::string name)
    {
        for (char& c : name)
            if (c == ':')
                c = '_';
        return name;
    }

    static std::string normalizePathForCompare(std::string p)
    {
        for (char& c : p)
        {
            if (c == '\\') c = '/';
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        }
        return p;
    }

    /// Os::writeFile / CreateFile on Windows is more reliable with backslashes for dialog paths.
    static std::string osPathForFileWrite(const std::string& s)
    {
#ifdef _WIN32
        std::string p = s;
        for (char& c : p)
            if (c == '/') c = '\\';
        return p;
#else
        return s;
#endif
    }

#ifdef _WIN32
    static bool copyFileContentsWin32(const std::string& src, const std::string& dst)
    {
        return CopyFileA(osPathForFileWrite(src).c_str(), osPathForFileWrite(dst).c_str(), FALSE) != 0;
    }
#endif

    static bool fileExistsForBundle(const std::string& p)
    {
        FILE* f = fopen(osPathForFileWrite(p).c_str(), "rb");
        if (!f) return false;
        fclose(f);
        return true;
    }

    static long getFileSizeBytes(const std::string& absPath)
    {
        FILE* f = fopen(osPathForFileWrite(absPath).c_str(), "rb");
        if (!f) return -1;
        if (fseek(f, 0, SEEK_END) != 0)
        {
            fclose(f);
            return -1;
        }
        const long sz = ftell(f);
        fclose(f);
        return sz;
    }

    static bool fileStartsWithDdsMagic(const std::string& absPath)
    {
        FILE* f = fopen(osPathForFileWrite(absPath).c_str(), "rb");
        if (!f) return false;
        unsigned char b[4];
        const size_t n = fread(b, 1, 4, f);
        fclose(f);
        return n == 4 && b[0] == 'D' && b[1] == 'D' && b[2] == 'S' && b[3] == ' ';
    }

    /// Maps TXM path (texture/foo.dds) to the DDS file written under textureWriteDir (foo.dds).
    static std::string absolutePathForTextureWriteDirDds(const std::string& texTree)
    {
        if (texTree.empty()) return std::string();
        const char* tw = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::TEXTURE_WRITE_DIR_INDEX);
        if (!tw || !tw[0]) return std::string();
        std::string dir(tw);
        if (!dir.empty() && dir.back() != '\\' && dir.back() != '/')
            dir += '\\';
        for (char& c : dir)
            if (c == '/') c = '\\';
        const std::string stem = textureStemFromSwgOrTreePath(texTree);
        if (stem.empty()) return std::string();
        return dir + stem + ".dds";
    }

    static bool seqAllDigits(const std::string& s)
    {
        if (s.empty()) return false;
        for (unsigned char u : s)
        {
            const char c = static_cast<char>(u);
            if (c < '0' || c > '9') return false;
        }
        return true;
    }

    static bool seqEndsWithIgnoreCase(const std::string& name, const std::string& extWithDot)
    {
        if (name.size() < extWithDot.size()) return false;
        for (size_t i = 0; i < extWithDot.size(); ++i)
        {
            const char a = name[name.size() - extWithDot.size() + i];
            const char b = extWithDot[i];
            if (std::tolower(static_cast<unsigned char>(a)) != std::tolower(static_cast<unsigned char>(b)))
                return false;
        }
        return true;
    }

    /// Builds a sorted frame list from any one resolved frame path (same directory + naming convention).
    /// Supports Maya-style names with **no hardcoded base paths**:
    ///   - `stem.1234.ext` (padding / dot before frame index)
    ///   - `stem1234.ext` (digits glued to stem before extension)
    /// Requires 2+ matching files in the sample's directory. Uses directory scan (UTF-8 paths via u8path).
    static bool collectNumericSequenceFramesFromSamplePath(const std::string& absSamplePath, std::vector<std::string>& outSortedAbs)
    {
        outSortedAbs.clear();
        if (absSamplePath.empty()) return false;

        struct Entry
        {
            std::string path;
            int n;
        };
        std::vector<Entry> acc;

        const fs::path sampleU8 = fs::u8path(absSamplePath);
        fs::path dir = sampleU8.parent_path();
        if (dir.empty())
            dir = fs::current_path();
        std::string fileName = sampleU8.filename().u8string();
        if (fileName.empty()) return false;

        const size_t lastDot = fileName.find_last_of('.');
        if (lastDot == std::string::npos || lastDot == 0) return false;
        const std::string extWithDot = fileName.substr(lastDot);
        const std::string withoutExt = fileName.substr(0, lastDot);

        std::error_code ec;

        auto collectDottedStem = [&](const std::string& stem) {
            acc.clear();
            ec.clear();
            const std::string prefix = stem + '.';
            for (const auto& ent : fs::directory_iterator(dir, ec))
            {
                if (ec) break;
                std::error_code fe;
                if (!ent.is_regular_file(fe)) continue;
                const std::string name = ent.path().filename().u8string();
                if (!seqEndsWithIgnoreCase(name, extWithDot)) continue;
                if (name.size() <= prefix.size() + extWithDot.size()) continue;
                if (name.compare(0, prefix.size(), prefix) != 0) continue;
                const std::string mid = name.substr(prefix.size(), name.size() - prefix.size() - extWithDot.size());
                if (!seqAllDigits(mid)) continue;
                acc.push_back({ent.path().u8string(), std::atoi(mid.c_str())});
            }
        };

        auto collectSuffixDigits = [&](const std::string& prefix) {
            acc.clear();
            ec.clear();
            if (prefix.empty()) return;
            for (const auto& ent : fs::directory_iterator(dir, ec))
            {
                if (ec) break;
                std::error_code fe;
                if (!ent.is_regular_file(fe)) continue;
                const std::string name = ent.path().filename().u8string();
                if (!seqEndsWithIgnoreCase(name, extWithDot)) continue;
                if (name.size() <= prefix.size() + extWithDot.size()) continue;
                if (name.compare(0, prefix.size(), prefix) != 0) continue;
                const std::string mid = name.substr(prefix.size(), name.size() - prefix.size() - extWithDot.size());
                if (!seqAllDigits(mid)) continue;
                acc.push_back({ent.path().u8string(), std::atoi(mid.c_str())});
            }
        };

        const size_t innerDot = withoutExt.find_last_of('.');
        if (innerDot != std::string::npos && innerDot + 1 < withoutExt.size())
        {
            const std::string mid = withoutExt.substr(innerDot + 1);
            if (seqAllDigits(mid))
            {
                const std::string stem = withoutExt.substr(0, innerDot);
                collectDottedStem(stem);
            }
        }

        if (acc.size() < 2)
        {
            size_t i = withoutExt.size();
            while (i > 0 && withoutExt[i - 1] >= '0' && withoutExt[i - 1] <= '9') --i;
            if (i > 0 && i < withoutExt.size())
                collectSuffixDigits(withoutExt.substr(0, i));
        }

        if (acc.size() < 2)
        {
            outSortedAbs.clear();
            return false;
        }
        std::sort(acc.begin(), acc.end(), [](const Entry& a, const Entry& b) {
            if (a.n != b.n) return a.n < b.n;
            return a.path < b.path;
        });
        outSortedAbs.reserve(acc.size());
        for (const Entry& e : acc) outSortedAbs.push_back(e.path);
        return true;
    }

    static bool readAnimatingTextureParamsFromFileNode(MFnDependencyNode& fileFn, float& fpsMin, float& fpsMax, int& orderEnum)
    {
        auto tryFloatAttr = [&](const char* a, const char* b, float& v) -> bool {
            for (const char* nm : {a, b})
            {
                MPlug p = fileFn.findPlug(nm, true);
                if (p.isNull()) continue;
                if (p.getValue(v) == MS::kSuccess) return true;
            }
            return false;
        };

        const bool ha = tryFloatAttr("swgAnimFpsMin", "soe_animatingFpsMin", fpsMin);
        const bool hb = tryFloatAttr("swgAnimFpsMax", "soe_animatingFpsMax", fpsMax);

        orderEnum = 0;
        MPlug o = fileFn.findPlug("swgAnimOrder", true);
        if (o.isNull()) o = fileFn.findPlug("soe_animatingOrder", true);
        if (!o.isNull()) o.getValue(orderEnum);

        return ha && hb && fpsMin > 0.f;
    }

    static Tag switcherTagForAnimOrder(int orderEnum012)
    {
        switch (orderEnum012)
        {
            case 1:
                return TAG_DPPT;
            case 2:
                return TAG_DRFT;
            case 0:
            default:
                return TAG_DRTS;
        }
    }

    static std::string frameStemForAnim(const std::string& texBase, size_t frameIdx)
    {
        char buf[384];
        std::snprintf(buf, sizeof(buf), "%s_f%04zu", texBase.c_str(), frameIdx);
        return std::string(buf);
    }

    /// Verifies SWTS main file, _base SSHT, and every published frame DDS.
    static bool verifyAnimatedShaderExportArtifacts(const std::string& absSwtsWritten, const std::string& absBaseWritten,
        const std::vector<std::string>& frameTextureTrees, const std::string& outShaderTreeForLog)
    {
        if (getFileSizeBytes(absSwtsWritten) < 32)
        {
            std::cerr << "[ExportStaticMesh] Verify failed: animated shader missing or too small: " << absSwtsWritten << "\n";
            MGlobal::displayError(
                MString("SwgMsh export verify: animated .sht invalid: ") + absSwtsWritten.c_str());
            return false;
        }
        if (getFileSizeBytes(absBaseWritten) < 32)
        {
            std::cerr << "[ExportStaticMesh] Verify failed: base shader for SWTS invalid: " << absBaseWritten << "\n";
            MGlobal::displayError(
                MString("SwgMsh export verify: base shader for animation invalid: ") + absBaseWritten.c_str());
            return false;
        }
        for (const std::string& tree : frameTextureTrees)
        {
            const std::string ddsAbs = absolutePathForTextureWriteDirDds(tree);
            if (ddsAbs.empty() || !fileStartsWithDdsMagic(ddsAbs))
            {
                std::cerr << "[ExportStaticMesh] Verify failed: animated frame DDS missing or invalid for " << tree << "\n";
                MGlobal::displayError(MString("SwgMsh export verify: invalid DDS for animated frame ") + tree.c_str());
                return false;
            }
        }
        std::cerr << "[ExportStaticMesh] Verified animated shader \"" << outShaderTreeForLog << "\" + base + "
                  << frameTextureTrees.size() << " frame DDS\n";
        return true;
    }

    /// Confirms the cloned .sht exists and, when a diffuse tree path was set, the matching DDS is present and valid.
    static bool verifyShaderExportArtifacts(const std::string& absShaderWritten, const std::string& texTree,
        const std::string& outShaderTreeForLog)
    {
        if (getFileSizeBytes(absShaderWritten) < 32)
        {
            std::cerr << "[ExportStaticMesh] Verify failed: shader missing or too small: " << absShaderWritten << "\n";
            MGlobal::displayError(
                MString("SwgMsh export verify: shader output invalid: ") + absShaderWritten.c_str());
            return false;
        }

        if (texTree.empty())
        {
            MGlobal::displayWarning(
                MString("SwgMayaEditor: verify: \"") + outShaderTreeForLog.c_str()
                + "\" uses prototype TXM paths only (no diffuse published). Connect file/aiImage to base color, "
                  "or set swgTexturePath / drop images in textureWriteDir, then re-export. "
                  "Otherwise confirm prototype textures exist under the viewer data root.");
            return true;
        }

        const std::string ddsAbs = absolutePathForTextureWriteDirDds(texTree);
        if (ddsAbs.empty())
        {
            std::cerr << "[ExportStaticMesh] Verify failed: textureWriteDir not set; cannot check DDS for TXM \""
                      << texTree << "\"\n";
            MGlobal::displayError("SwgMsh export verify: textureWriteDir not set (setBaseDir). Cannot verify diffuse DDS.");
            return false;
        }
        if (!fileStartsWithDdsMagic(ddsAbs))
        {
            std::cerr << "[ExportStaticMesh] Verify failed: DDS missing or not a valid DDS file (expected for TXM \""
                      << texTree << "\"): " << ddsAbs << "\n";
            MGlobal::displayError(MString("SwgMsh export verify: invalid or missing DDS for ") + texTree.c_str()
                + " - expected file: " + ddsAbs.c_str());
            return false;
        }

        std::cerr << "[ExportStaticMesh] Verified: " << absShaderWritten << " + " << ddsAbs << " (TXM " << texTree
                  << ")\n";
        MGlobal::displayInfo(
            MString("SwgMayaEditor: verified shader and DDS (") + texTree.c_str() + ")");
        return true;
    }

    static std::string fileNameFromPath(const std::string& p)
    {
        const size_t slash = p.find_last_of("/\\");
        return (slash == std::string::npos) ? p : p.substr(slash + 1);
    }

    static void pushUniquePath(std::vector<std::string>& v, const std::string& p)
    {
        if (p.empty()) return;
        for (const auto& x : v)
            if (x == p) return;
        v.push_back(p);
    }

    /// Ensures shader/, texture/, and appearance/ under setBaseDir (DATA_ROOT) contain the exported files.
    /// Mesh is only written to appearance/mesh/ (no duplicate .msh at export root).
    static void mirrorViewerBundleToDataRoot(
        const std::vector<std::string>& exportedShaderAbsPaths,
        const std::vector<std::string>& exportedDdsAbsPaths,
        const std::string& mshCanonicalPath,
        const std::string& aptCanonicalPath,
        const std::string& meshName)
    {
        const char* dr = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::DATA_ROOT_DIR_INDEX);
        if (!dr || !dr[0]) return;
        std::string bundleRoot = dr;
        for (char& c : bundleRoot)
            if (c == '/') c = '\\';
        if (!bundleRoot.empty() && bundleRoot.back() != '\\' && bundleRoot.back() != '/')
            bundleRoot += '\\';

        const std::string bundleShaderDir = bundleRoot + "shader\\";
        const std::string bundleTexDir = bundleRoot + "texture\\";
        const std::string bundleMeshDir = bundleRoot + "appearance\\mesh\\";
        const std::string bundleAppearanceDir = bundleRoot + "appearance\\";
        MayaUtility::createDirectory(winPathForMkdir(bundleShaderDir).c_str());
        MayaUtility::createDirectory(winPathForMkdir(bundleTexDir).c_str());
        MayaUtility::createDirectory(winPathForMkdir(bundleMeshDir).c_str());
        MayaUtility::createDirectory(winPathForMkdir(bundleAppearanceDir).c_str());

#ifdef _WIN32
        for (const auto& src : exportedShaderAbsPaths)
        {
            if (src.empty()) continue;
            const std::string base = fileNameFromPath(src);
            const std::string dst = bundleShaderDir + base;
            if (normalizePathForCompare(src) == normalizePathForCompare(dst)) continue;
            if (!fileExistsForBundle(src))
            {
                std::cerr << "[ExportStaticMesh] Bundle skip (missing shader): " << src << "\n";
                continue;
            }
            if (copyFileContentsWin32(src, dst))
                std::cerr << "[ExportStaticMesh] Bundled .sht: " << dst << "\n";
            else
                std::cerr << "[ExportStaticMesh] Bundle copy failed: " << src << " -> " << dst << "\n";
        }
        for (const auto& src : exportedDdsAbsPaths)
        {
            if (src.empty()) continue;
            const std::string base = fileNameFromPath(src);
            const std::string dst = bundleTexDir + base;
            if (normalizePathForCompare(src) == normalizePathForCompare(dst)) continue;
            if (!fileExistsForBundle(src))
            {
                std::cerr << "[ExportStaticMesh] Bundle skip (missing texture): " << src << "\n";
                continue;
            }
            if (copyFileContentsWin32(src, dst))
                std::cerr << "[ExportStaticMesh] Bundled .dds: " << dst << "\n";
            else
                std::cerr << "[ExportStaticMesh] Bundle copy failed: " << src << " -> " << dst << "\n";
        }
        if (!mshCanonicalPath.empty() && fileExistsForBundle(mshCanonicalPath))
        {
            const std::string dstMsh = bundleMeshDir + meshName + ".msh";
            if (normalizePathForCompare(mshCanonicalPath) != normalizePathForCompare(dstMsh))
            {
                if (copyFileContentsWin32(mshCanonicalPath, dstMsh))
                    std::cerr << "[ExportStaticMesh] Bundled .msh: " << dstMsh << "\n";
            }
        }
        if (!aptCanonicalPath.empty() && fileExistsForBundle(aptCanonicalPath))
        {
            const std::string dstApt = bundleAppearanceDir + meshName + ".apt";
            if (normalizePathForCompare(aptCanonicalPath) != normalizePathForCompare(dstApt))
            {
                if (copyFileContentsWin32(aptCanonicalPath, dstApt))
                    std::cerr << "[ExportStaticMesh] Bundled .apt: " << dstApt << "\n";
            }
        }
#else
        (void)exportedShaderAbsPaths;
        (void)exportedDdsAbsPaths;
        (void)mshCanonicalPath;
        (void)aptCanonicalPath;
        (void)meshName;
#endif
        MGlobal::displayInfo(
            MString("SwgMayaEditor: viewer tree at setBaseDir - ") + (bundleRoot + "shader/, texture/, appearance/").c_str());
    }

    /// Chooses appearance root (parent of mesh/) and mesh output directory; shader/texture stay separate (SetDirectoryCommand).
    static void resolveAppearanceAndMeshDirs(const std::string& directoryPathNoFile,
        std::string& outAppearanceRoot, std::string& outMeshDir)
    {
        const std::string path = trimTrailingSeparatorsStr(directoryPathNoFile);
        const std::string last = lastPathSegment(path);
        if (iequalsSeg(last, "mesh"))
        {
            outMeshDir = ensureTrailingSlash(path);
            outAppearanceRoot = ensureTrailingSlash(parentPathStr(path));
        }
        else if (iequalsSeg(last, "appearance"))
        {
            outAppearanceRoot = ensureTrailingSlash(path);
            outMeshDir = ensureTrailingSlash(outAppearanceRoot + "mesh");
        }
        else
        {
            const std::string exportRoot = ensureTrailingSlash(path);
            outAppearanceRoot = ensureTrailingSlash(exportRoot + "appearance");
            outMeshDir = ensureTrailingSlash(outAppearanceRoot + "mesh");
        }
    }

    static bool resolveMayaImagePathString(const MString& rawPath, std::string& outPath)
    {
        if (rawPath.length() == 0) return false;
        MFileObject fileObj;
        fileObj.setRawFullName(rawPath);
        MString full = fileObj.resolvedFullName();
        if (full.length() == 0) full = rawPath;
        outPath = resolveWindowsMayaAbsolutePath(std::string(full.asChar()));
        return !outPath.empty();
    }

    /// True when fileTextureName is a pattern (sequence / UDIM) and needs Maya's evaluated string.
    static bool texturePathNeedsComputedEvaluation(const MString& path)
    {
        if (path.length() == 0) return false;
        std::string s(path.asChar());
        for (unsigned char uc : s)
        {
            const char c = static_cast<char>(uc);
            if (c == '<' || c == '#' || c == '%') return true;
        }
        std::string lower;
        lower.reserve(s.size());
        for (unsigned char uc : s)
            lower += static_cast<char>(std::tolower(uc));
        if (lower.find("udim") != std::string::npos) return true;
        return false;
    }

    static bool tryReadFileOrMovieTexturePath(MFnDependencyNode& texFn, std::string& outPath)
    {
        MPlug ftnPlug = texFn.findPlug("fileTextureName", true);
        MString ftn;
        const bool haveFtn =
            !ftnPlug.isNull() && ftnPlug.getValue(ftn) == MS::kSuccess && ftn.length() > 0;

        MPlug cfnpPlug = texFn.findPlug("computedFileTextureNamePattern", true);
        MString cfnp;
        const bool haveCfnp =
            !cfnpPlug.isNull() && cfnpPlug.getValue(cfnp) == MS::kSuccess && cfnp.length() > 0;

        auto tryResolve = [&](const MString& raw) -> bool { return resolveMayaImagePathString(raw, outPath); };

        if (haveFtn && texturePathNeedsComputedEvaluation(ftn))
        {
            if (haveCfnp && tryResolve(cfnp)) return true;
            if (tryResolve(ftn)) return true;
            return false;
        }

        // Normal image path: use what the artist set on the file node first (e.g. kettle_copper_b.tga).
        // computedFileTextureNamePattern can still point at an older resolved name in some DG states.
        if (haveFtn && tryResolve(ftn)) return true;
        if (haveCfnp && tryResolve(cfnp)) return true;
        return false;
    }

    static bool tryReadAiImageTexturePath(MFnDependencyNode& texFn, std::string& outPath)
    {
        static const char* candidates[] = { "filename", "image" };
        for (const char* an : candidates)
        {
            MPlug p = texFn.findPlug(an, true);
            if (p.isNull()) continue;
            MString rawPath;
            if (p.getValue(rawPath) != MS::kSuccess || rawPath.length() == 0) continue;
            if (resolveMayaImagePathString(rawPath, outPath)) return true;
        }
        return false;
    }

    /// First upstream plug feeding a color-like attribute on the surface (or layeredShader layer).
    static MPlug firstColorInputPlug(MFnDependencyNode& fn, const char* attrName)
    {
        MPlug p = fn.findPlug(attrName, true);
        if (p.isNull()) return MPlug();
        MPlugArray con;
        p.connectedTo(con, true, false);
        if (con.length() > 0) return con[0];
        if (p.isCompound())
        {
            const unsigned nc = p.numChildren();
            for (unsigned i = 0; i < nc; ++i)
            {
                MPlug ch = p.child(i);
                ch.connectedTo(con, true, false);
                if (con.length() > 0) return con[0];
            }
        }
        return MPlug();
    }

    static MPlug diffuseSourcePlugFromSurface(MFnDependencyNode& surfaceFn)
    {
        const MString type = surfaceFn.typeName();

        if (type == MString("layeredShader"))
        {
            MPlug inputs = surfaceFn.findPlug("inputs", true);
            if (!inputs.isNull() && inputs.isArray())
            {
                const unsigned n = inputs.numElements();
                for (unsigned i = 0; i < n; ++i)
                {
                    MPlug elem = inputs.elementByPhysicalIndex(i);
                    const unsigned nch = elem.numChildren();
                    for (unsigned ch = 0; ch < nch; ++ch)
                    {
                        MPlug o = elem.child(ch);
                        MPlugArray con;
                        o.connectedTo(con, true, false);
                        if (con.length() > 0) return con[0];
                    }
                }
            }
        }

        static const char* surfaceAttrs[] = {
            "baseColor",
            "base_color",
            "color",
            "diffuseColor",
            "diffuse",
            "emissionColor",
            "emission_color",
            "incandescence",
            "TEX_color",
        };
        for (const char* an : surfaceAttrs)
        {
            MPlug c = firstColorInputPlug(surfaceFn, an);
            if (!c.isNull()) return c;
        }
        return MPlug();
    }

    static bool traceTextureSourceGraph(const MObject& nodeObj, std::string& outPath, int depth);

    /// Follow one Maya plug (compound or scalar) into traceTextureSourceGraph. Named function avoids MSVC/recursive-lambda edge cases.
    static bool tracePlugIntoTextureGraph(MPlug& p, std::string& outPath, int depth)
    {
        if (p.isNull()) return false;
        MPlugArray con;
        if (p.isCompound())
        {
            for (unsigned c = 0; c < p.numChildren(); ++c)
            {
                MPlug ch = p.child(c);
                ch.connectedTo(con, true, false);
                if (con.length() > 0 && traceTextureSourceGraph(con[0].node(), outPath, depth + 1))
                    return true;
            }
            return false;
        }
        p.connectedTo(con, true, false);
        return con.length() > 0 && traceTextureSourceGraph(con[0].node(), outPath, depth + 1);
    }

    static bool traceTextureSourceGraph(const MObject& nodeObj, std::string& outPath, int depth)
    {
        if (depth > 16) return false;
        MStatus st;
        MFnDependencyNode fn(nodeObj, &st);
        if (!st) return false;
        const MString t = fn.typeName();

        if (t == MString("file") || t == MString("movie") || t == MString("psdFileTex"))
            return tryReadFileOrMovieTexturePath(fn, outPath);
        if (t == MString("aiImage"))
            return tryReadAiImageTexturePath(fn, outPath);

        if (t == MString("bump2d"))
        {
            MPlug b = fn.findPlug("bumpValue", true);
            if (!b.isNull() && tracePlugIntoTextureGraph(b, outPath, depth))
                return true;
            return false;
        }

        if (t == MString("multiplyDivide"))
        {
            static const char* mdAttrs[] = { "input1", "input2" };
            for (const char* an : mdAttrs)
            {
                MPlug p = fn.findPlug(an, true);
                if (tracePlugIntoTextureGraph(p, outPath, depth)) return true;
            }
            return false;
        }

        if (t == MString("blendColors") || t == MString("aiMixRgb"))
        {
            static const char* bc[] = { "color1", "color2", "input1", "input2" };
            for (const char* an : bc)
            {
                MPlug p = fn.findPlug(an, true);
                if (tracePlugIntoTextureGraph(p, outPath, depth)) return true;
            }
            return false;
        }

        if (t == MString("plusMinusAverage"))
        {
            static const char* pmaArrays[] = { "input3D", "input2D", "input1D" };
            for (const char* arrName : pmaArrays)
            {
                MPlug arr = fn.findPlug(arrName, true);
                if (arr.isNull() || !arr.isArray()) continue;
                const unsigned ne = arr.numElements();
                for (unsigned ei = 0; ei < ne; ++ei)
                {
                    MPlug elem = arr.elementByPhysicalIndex(ei);
                    const unsigned nch = elem.numChildren();
                    for (unsigned ch = 0; ch < nch; ++ch)
                    {
                        MPlug o = elem.child(ch);
                        MPlugArray con;
                        o.connectedTo(con, true, false);
                        if (con.length() > 0 && traceTextureSourceGraph(con[0].node(), outPath, depth + 1))
                            return true;
                    }
                }
            }
            return false;
        }

        if (t == MString("gammaCorrect") || t == MString("remapHsv") || t == MString("remapColor"))
        {
            static const char* gc[] = { "value", "color", "input" };
            for (const char* an : gc)
            {
                MPlug p = fn.findPlug(an, true);
                if (tracePlugIntoTextureGraph(p, outPath, depth)) return true;
            }
            return false;
        }

        if (t == MString("colorCorrect") || t == MString("aiColorCorrect"))
        {
            MPlug p = fn.findPlug("input", true);
            if (tracePlugIntoTextureGraph(p, outPath, depth)) return true;
            return false;
        }

        if (t == MString("layeredTexture"))
        {
            MPlug inputs = fn.findPlug("inputs", true);
            if (inputs.isNull() || !inputs.isArray()) return false;
            const unsigned n = inputs.numElements();
            for (unsigned ei = 0; ei < n; ++ei)
            {
                MPlug e = inputs.elementByPhysicalIndex(ei);
                const unsigned nc = e.numChildren();
                for (unsigned ci = 0; ci < nc; ++ci)
                {
                    MPlug ch = e.child(ci);
                    MPlugArray con;
                    ch.connectedTo(con, true, false);
                    if (con.length() > 0 && traceTextureSourceGraph(con[0].node(), outPath, depth + 1))
                        return true;
                }
            }
            return false;
        }

        static const char* passThrough[] = {
            "input",
            "inColor",
            "image",
            "default",
            "input1",
            "input2",
            "color1",
            "color2",
        };
        for (const char* an : passThrough)
        {
            MPlug p = fn.findPlug(an, true);
            if (p.isNull()) continue;
            if (p.isArray() && p.numElements() > 0)
            {
                MPlug e = p.elementByPhysicalIndex(0);
                const unsigned nc = e.numChildren();
                for (unsigned ci = 0; ci < nc; ++ci)
                {
                    MPlug ch = e.child(ci);
                    MPlugArray con;
                    ch.connectedTo(con, true, false);
                    if (con.length() > 0 && traceTextureSourceGraph(con[0].node(), outPath, depth + 1))
                        return true;
                }
            }
            else
            {
                MPlugArray con;
                p.connectedTo(con, true, false);
                if (con.length() > 0 && traceTextureSourceGraph(con[0].node(), outPath, depth + 1))
                    return true;
            }
        }
        return false;
    }

    static void collectUpstreamFileTextureNodes(const MObject& rootNode, std::vector<MObject>& outFiles)
    {
        outFiles.clear();
        if (rootNode.isNull()) return;
        MStatus st;
        MObject root = rootNode;
        MItDependencyGraph dgIt(root, MFn::kFileTexture, MItDependencyGraph::kUpstream,
            MItDependencyGraph::kBreadthFirst, MItDependencyGraph::kNodeLevel, &st);
        if (!st) return;
        for (; !dgIt.isDone(); dgIt.next())
        {
            MObject o = dgIt.currentItem(&st);
            if (!st || o.isNull()) continue;
            bool dup = false;
            for (const MObject& e : outFiles)
            {
                if (e == o)
                {
                    dup = true;
                    break;
                }
            }
            if (!dup) outFiles.push_back(o);
        }
    }

    static bool primaryFileTextureFromShadingGroup(const MObject& sgObj, MObject& outFile)
    {
        outFile = MObject();
        MStatus st;
        MFnDependencyNode sgFn(sgObj, &st);
        if (!st) return false;
        MPlug surf = sgFn.findPlug("surfaceShader", true);
        MPlugArray scon;
        surf.connectedTo(scon, true, false);
        if (scon.length() == 0) return false;
        std::vector<MObject> files;
        collectUpstreamFileTextureNodes(scon[0].node(), files);
        if (files.empty()) return false;
        outFile = files[0];
        return true;
    }

    /// OBJ / kitbash graphs sometimes insert nodes we do not traverse; filenames like *_normal* are usually not diffuse.
    static bool basenameLooksLikeAuxTexture(const std::string& absPath)
    {
        std::string base = absPath;
        size_t slash = base.find_last_of("/\\");
        if (slash != std::string::npos) base = base.substr(slash + 1);
        std::string lower;
        lower.reserve(base.size());
        for (unsigned char uc : base)
            lower += static_cast<char>(tolower(uc));
        if (lower.find("normal") != std::string::npos) return true;
        if (lower.find("rough") != std::string::npos) return true;
        if (lower.find("metal") != std::string::npos) return true;
        if (lower.find("orm") != std::string::npos) return true;
        if (lower.find("bump") != std::string::npos) return true;
        if (lower.find("height") != std::string::npos) return true;
        if (lower.find("disp") != std::string::npos) return true;
        const size_t dot = lower.rfind('.');
        if (dot >= 2 && dot != std::string::npos && lower[dot - 2] == '_' &&
            (lower[dot - 1] == 'n' || lower[dot - 1] == 'N'))
            return true;
        return false;
    }

    static bool tryPickDiffusePathFromUpstreamFileNodes(const std::vector<MObject>& files, std::string& outPath)
    {
        std::vector<std::string> paths;
        paths.reserve(files.size());
        for (const MObject& o : files)
        {
            if (o.isNull()) continue;
            MFnDependencyNode fn(o);
            std::string path;
            if (!tryReadFileOrMovieTexturePath(fn, path)) continue;
            paths.push_back(std::move(path));
        }
        if (paths.empty()) return false;

        auto tryPick = [&](bool allowAux) -> bool {
            int best = -1;
            std::string bestPath;
            for (const std::string& path : paths)
            {
                if (!allowAux && basenameLooksLikeAuxTexture(path)) continue;
                int sc = fileExistsForBundle(path) ? 100 : 0;
                if (!basenameLooksLikeAuxTexture(path)) sc += 50;
                if (sc > best)
                {
                    best = sc;
                    bestPath = path;
                }
            }
            if (bestPath.empty()) return false;
            outPath = std::move(bestPath);
            return true;
        };

        if (tryPick(false)) return true;
        return tryPick(true);
    }

    /// Follow shadingEngine -> surfaceShader -> diffuse/baseColor -> file / aiImage / layeredTexture chain.
    static bool tryGetDiffuseImageAbsolutePath(MObject shadingGroupObj, std::string& outPath)
    {
        MStatus st;
        MFnDependencyNode sgFn(shadingGroupObj, &st);
        if (!st) return false;
        MPlug surfPlug = sgFn.findPlug("surfaceShader", true, &st);
        if (surfPlug.isNull()) return false;
        MPlugArray surfConnections;
        surfPlug.connectedTo(surfConnections, true, false);
        if (surfConnections.length() == 0) return false;

        MFnDependencyNode surfaceFn(surfConnections[0].node(), &st);
        if (!st) return false;

        MPlug src = diffuseSourcePlugFromSurface(surfaceFn);
        if (src.isNull()) return false;
        if (traceTextureSourceGraph(src.node(), outPath, 0))
            return true;

        std::vector<MObject> upstreamFiles;
        collectUpstreamFileTextureNodes(src.node(), upstreamFiles);
        return tryPickDiffusePathFromUpstreamFileNodes(upstreamFiles, outPath);
    }

    static bool traceTextureUvSetGraph(const MObject& nodeObj, MString& outUvSet, int depth);

    static bool tracePlugIntoUvSetGraph(MPlug& p, MString& outUvSet, int depth)
    {
        if (p.isNull()) return false;
        MPlugArray con;
        if (p.isCompound())
        {
            for (unsigned c = 0; c < p.numChildren(); ++c)
            {
                MPlug ch = p.child(c);
                ch.connectedTo(con, true, false);
                if (con.length() > 0 && traceTextureUvSetGraph(con[0].node(), outUvSet, depth + 1))
                    return true;
            }
            return false;
        }
        p.connectedTo(con, true, false);
        return con.length() > 0 && traceTextureUvSetGraph(con[0].node(), outUvSet, depth + 1);
    }

    /// Same graph as traceTextureSourceGraph, but stops at the first file/aiImage and reads optional uvSetName.
    static bool traceTextureUvSetGraph(const MObject& nodeObj, MString& outUvSet, int depth)
    {
        if (depth > 16) return false;
        MStatus st;
        MFnDependencyNode fn(nodeObj, &st);
        if (!st) return false;
        const MString t = fn.typeName();

        if (t == MString("file") || t == MString("movie") || t == MString("psdFileTex"))
        {
            outUvSet.clear();
            MPlug uvp = fn.findPlug("uvSetName", true);
            if (!uvp.isNull())
                uvp.getValue(outUvSet);
            return true;
        }
        if (t == MString("aiImage"))
        {
            outUvSet.clear();
            MPlug uvp = fn.findPlug("uvSetName", true);
            if (!uvp.isNull())
                uvp.getValue(outUvSet);
            return true;
        }

        if (t == MString("multiplyDivide"))
        {
            static const char* mdAttrs[] = {"input1", "input2"};
            for (const char* an : mdAttrs)
            {
                MPlug p = fn.findPlug(an, true);
                if (tracePlugIntoUvSetGraph(p, outUvSet, depth)) return true;
            }
            return false;
        }

        if (t == MString("blendColors") || t == MString("aiMixRgb"))
        {
            static const char* bc[] = {"color1", "color2", "input1", "input2"};
            for (const char* an : bc)
            {
                MPlug p = fn.findPlug(an, true);
                if (tracePlugIntoUvSetGraph(p, outUvSet, depth)) return true;
            }
            return false;
        }

        if (t == MString("plusMinusAverage"))
        {
            static const char* pmaArrays[] = {"input3D", "input2D", "input1D"};
            for (const char* arrName : pmaArrays)
            {
                MPlug arr = fn.findPlug(arrName, true);
                if (arr.isNull() || !arr.isArray()) continue;
                const unsigned ne = arr.numElements();
                for (unsigned ei = 0; ei < ne; ++ei)
                {
                    MPlug elem = arr.elementByPhysicalIndex(ei);
                    const unsigned nch = elem.numChildren();
                    for (unsigned ch = 0; ch < nch; ++ch)
                    {
                        MPlug o = elem.child(ch);
                        MPlugArray con;
                        o.connectedTo(con, true, false);
                        if (con.length() > 0 && traceTextureUvSetGraph(con[0].node(), outUvSet, depth + 1))
                            return true;
                    }
                }
            }
            return false;
        }

        if (t == MString("gammaCorrect") || t == MString("remapHsv") || t == MString("remapColor"))
        {
            static const char* gc[] = {"value", "color", "input"};
            for (const char* an : gc)
            {
                MPlug p = fn.findPlug(an, true);
                if (tracePlugIntoUvSetGraph(p, outUvSet, depth)) return true;
            }
            return false;
        }

        if (t == MString("colorCorrect") || t == MString("aiColorCorrect"))
        {
            MPlug p = fn.findPlug("input", true);
            if (tracePlugIntoUvSetGraph(p, outUvSet, depth)) return true;
            return false;
        }

        if (t == MString("layeredTexture"))
        {
            MPlug inputs = fn.findPlug("inputs", true);
            if (inputs.isNull() || !inputs.isArray()) return false;
            const unsigned n = inputs.numElements();
            for (unsigned ei = 0; ei < n; ++ei)
            {
                MPlug e = inputs.elementByPhysicalIndex(ei);
                const unsigned nc = e.numChildren();
                for (unsigned ci = 0; ci < nc; ++ci)
                {
                    MPlug ch = e.child(ci);
                    MPlugArray con;
                    ch.connectedTo(con, true, false);
                    if (con.length() > 0 && traceTextureUvSetGraph(con[0].node(), outUvSet, depth + 1))
                        return true;
                }
            }
            return false;
        }

        static const char* passThrough[] = {
            "input",
            "inColor",
            "image",
            "default",
            "input1",
            "input2",
            "color1",
            "color2",
        };
        for (const char* an : passThrough)
        {
            MPlug p = fn.findPlug(an, true);
            if (p.isNull()) continue;
            if (p.isArray() && p.numElements() > 0)
            {
                MPlug e = p.elementByPhysicalIndex(0);
                const unsigned nc = e.numChildren();
                for (unsigned ci = 0; ci < nc; ++ci)
                {
                    MPlug ch = e.child(ci);
                    MPlugArray con;
                    ch.connectedTo(con, true, false);
                    if (con.length() > 0 && traceTextureUvSetGraph(con[0].node(), outUvSet, depth + 1))
                        return true;
                }
            }
            else
            {
                MPlugArray con;
                p.connectedTo(con, true, false);
                if (con.length() > 0 && traceTextureUvSetGraph(con[0].node(), outUvSet, depth + 1))
                    return true;
            }
        }
        return false;
    }

    static bool tryGetDiffuseFileUvSetName(MObject shadingGroupObj, MString& outUvSet)
    {
        outUvSet.clear();
        MStatus st;
        MFnDependencyNode sgFn(shadingGroupObj, &st);
        if (!st) return false;
        MPlug surfPlug = sgFn.findPlug("surfaceShader", true, &st);
        if (surfPlug.isNull()) return false;
        MPlugArray surfConnections;
        surfPlug.connectedTo(surfConnections, true, false);
        if (surfConnections.length() == 0) return false;

        MFnDependencyNode surfaceFn(surfConnections[0].node(), &st);
        if (!st) return false;

        MPlug src = diffuseSourcePlugFromSurface(surfaceFn);
        if (src.isNull()) return false;
        return traceTextureUvSetGraph(src.node(), outUvSet, 0);
    }

    static bool meshHasUvSet(MFnMesh& meshFn, const MString& name)
    {
        if (name.length() == 0) return false;
        MStringArray names;
        meshFn.getUVSetNames(names);
        for (unsigned i = 0; i < names.length(); ++i)
            if (names[i] == name) return true;
        return false;
    }

    /// Reverse triangle corners vs Maya getTriangles(); mesh attr swgLegacyTriangleFlip / cfg apply only when exporting
    /// via exportStaticMesh command (no SwgMsh options string).
    static bool legacyTriangleFlipFromMeshShape(MFnMesh& meshFn)
    {
        MStatus st;
        MFnDependencyNode meshDep(meshFn.object(), &st);
        if (!st) return false;
        MPlug p = meshDep.findPlug("swgLegacyTriangleFlip", true);
        if (p.isNull()) return false;
        bool b = false;
        if (p.getValue(b) != MS::kSuccess) return false;
        return b;
    }

    /// Per face / shading group: file texture may reference a uvSetName — use it when the mesh has that set (combined .msh + OBJ often differ).
    static MString effectiveUvSetForFace(MFnMesh& meshFn, MObject shadingGroupObj, const MString& meshWideDefault)
    {
        MString fromShader;
        if (tryGetDiffuseFileUvSetName(shadingGroupObj, fromShader) && meshHasUvSet(meshFn, fromShader))
            return fromShader;
        return meshWideDefault;
    }

    /// Prefer Maya current UV set (viewport / UV editor); else unanimous diffuse file uvSetName; else map1; else first listed.
    static MString chooseExportUvSetName(MFnMesh& meshFn, const MObjectArray& shaderObjs, const MIntArray& faceToShader)
    {
        std::vector<int> usedSg;
        usedSg.reserve(8);
        for (unsigned fi = 0; fi < faceToShader.length(); ++fi)
        {
            const int idx = faceToShader[fi];
            if (idx < 0 || static_cast<unsigned>(idx) >= shaderObjs.length()) continue;
            if (std::find(usedSg.begin(), usedSg.end(), idx) == usedSg.end())
                usedSg.push_back(idx);
        }

        MString current;
        if (meshFn.getCurrentUVSetName(current) == MS::kSuccess && current.length() > 0 && meshHasUvSet(meshFn, current))
            return current;

        MString fromFile;
        bool haveExplicit = false;
        bool conflict = false;
        for (int idx : usedSg)
        {
            MString u;
            if (!tryGetDiffuseFileUvSetName(shaderObjs[static_cast<unsigned>(idx)], u))
                continue;
            if (u.length() == 0)
                continue;
            if (!meshHasUvSet(meshFn, u))
                continue;
            if (!haveExplicit)
            {
                fromFile = u;
                haveExplicit = true;
            }
            else if (fromFile != u)
            {
                conflict = true;
                break;
            }
        }

        if (haveExplicit && !conflict)
            return fromFile;

        MStringArray uvSetNames;
        meshFn.getUVSetNames(uvSetNames);
        for (unsigned i = 0; i < uvSetNames.length(); ++i)
        {
            if (uvSetNames[i] == MString("map1"))
                return uvSetNames[i];
        }
        if (uvSetNames.length() > 0)
            return uvSetNames[0];
        return MString();
    }
}

void* ExportStaticMesh::creator()
{
    return new ExportStaticMesh();
}

MStatus ExportStaticMesh::doIt(const MArgList& args)
{
    MStatus status;
    std::string outputPath;
    std::string meshName;
    bool legacyTriangleFlipCmd = false;
    bool objExportDirectUvCmd = false;

    for (unsigned i = 0; i < args.length(&status); ++i)
    {
        if (!status) return MS::kFailure;
        MString argName = args.asString(i, &status);
        if (!status) return MS::kFailure;

        if (argName == "-path" && i + 1 < args.length(&status))
        {
            outputPath = args.asString(i + 1, &status).asChar();
            if (status) ++i;
        }
        else if (argName == "-name" && i + 1 < args.length(&status))
        {
            meshName = args.asString(i + 1, &status).asChar();
            if (status) ++i;
        }
        else if (argName == "-legacyTriangleFlip" || argName == "-legacyTriFlip")
        {
            legacyTriangleFlipCmd = true;
        }
        else if (argName == "-objExportDirectUv")
        {
            objExportDirectUvCmd = true;
        }
    }

    MSelectionList sel;
    status = MGlobal::getActiveSelectionList(sel);
    if (!status || sel.length() == 0)
    {
        std::cerr << "ExportStaticMesh: select a mesh" << std::endl;
        return MS::kFailure;
    }

    MDagPath dagPath;
    bool foundMesh = false;
    bool meshWithoutShader = false;
    for (unsigned si = 0; si < sel.length(); ++si)
    {
        status = sel.getDagPath(si, dagPath);
        if (!status) continue;
        MDagPath meshPath;
        if (MayaUtility::findFirstMeshShapeWithShadersInHierarchy(dagPath, meshPath))
        {
            dagPath = meshPath;
            foundMesh = true;
            break;
        }
        if (MayaUtility::findFirstMeshShapeInHierarchy(dagPath, meshPath))
            meshWithoutShader = true;
    }
    if (!foundMesh)
    {
        if (meshWithoutShader)
        {
            std::cerr << "ExportStaticMesh: mesh under selection has no shading group (or only meshes "
                         "without materials were found first)" << std::endl;
            MGlobal::displayError(
                "SwgMsh export: mesh has no shader assignment. Found geometry but no material on the mesh "
                "that would be exported. After combining, assign a material to the combined mesh, delete "
                "hidden leftover shapes, or select the combined mesh shape directly.");
            return MS::kFailure;
        }
        std::cerr << "ExportStaticMesh: no mesh under selection" << std::endl;
        return MS::kFailure;
    }

    std::string outMeshPath, outAptPath;
    if (!performExport(dagPath, outputPath, outMeshPath, outAptPath, legacyTriangleFlipCmd, false, objExportDirectUvCmd,
            false, MString()))
        return MS::kFailure;

    MGlobal::displayInfo(MString("Exported mesh: ") + outMeshPath.c_str());
    if (!outAptPath.empty())
        MGlobal::displayInfo(MString("Exported APT: ") + outAptPath.c_str());
    return MS::kSuccess;
}

bool ExportStaticMesh::performExport(const MDagPath& meshDagPath, const std::string& outputPathOverride,
    std::string& outMeshPath, std::string& outAptPath, bool legacyTriangleFlipFromCmd,
    bool legacyTriangleFlipFromFileDialog, bool objExportDirectUvFromCmd, bool objExportDirectUvFromExportDialog,
    const MString& rawMshExportOptions)
{
    MStatus status;
    outMeshPath.clear();
    outAptPath.clear();
    std::vector<std::string> exportedShaderAbsPaths;
    std::vector<std::string> exportedDdsAbsPaths;

    MDagPath shapePath = meshDagPath;
    if (shapePath.hasFn(MFn::kTransform))
        shapePath.extendToShape();

    MFnMesh meshFn(shapePath, &status);
    if (!status)
    {
        std::cerr << "ExportStaticMesh: MFnMesh failed" << std::endl;
        return false;
    }

    // When the file translator passes a full options line, honor dialog 0/1 — do not let cfg or mesh attrs override.
    const bool legacyKeyInOptions = mshExportOptionsKeySpecified(rawMshExportOptions, "legacyTriangleFlip=");
    const bool objUvKeyInOptions = mshExportOptionsKeySpecified(rawMshExportOptions, "objExportDirectUv=");

    const bool legacyTriangleOrder =
        legacyTriangleFlipFromCmd
        || (legacyKeyInOptions
                ? legacyTriangleFlipFromFileDialog
                : (legacyTriangleFlipFromFileDialog || legacyTriangleFlipFromMeshShape(meshFn)
                      || ConfigFile::getKeyBool("SwgMayaEditor", "staticMeshLegacyTriangleFlip", false)));

    const bool objExportDirectUv =
        objExportDirectUvFromCmd
        || (objUvKeyInOptions ? objExportDirectUvFromExportDialog
                              : (objExportDirectUvFromExportDialog
                                    || ConfigFile::getKeyBool("SwgMayaEditor", "staticMeshObjExportDirectUv", false)));

    // Legacy / cfg-only fallback when a triangle is degenerate in engine space (zero-area cross).
    const bool legacyTriangleFallback = legacyTriangleOrder || objExportDirectUv;

    MDagPath transformPath = shapePath;
    if (transformPath.hasFn(MFn::kMesh))
        transformPath.pop();

    MFnDagNode transformFn(transformPath, &status);
    if (!status) return false;

    MString nodeName = transformFn.name(&status);
    if (!status) return false;

    MayaCompoundString compoundName(nodeName);
    std::string meshName;
    if (compoundName.getComponentCount() > 1)
        meshName = compoundName.getComponentStdString(1);
    else
        meshName = nodeName.asChar();

    size_t dot = meshName.find_last_of('.');
    if (dot != std::string::npos)
        meshName = meshName.substr(0, dot);

    std::string appearanceRootDir;
    std::string meshWriteDir;
    if (!outputPathOverride.empty())
    {
        std::string pathRemainder = outputPathOverride;
        if (pathRemainder.find('.') != std::string::npos)
        {
            const size_t slash = pathRemainder.find_last_of("/\\");
            if (slash != std::string::npos)
            {
                const std::string fileCandidate = pathRemainder.substr(slash + 1);
                if (fileCandidate.find('.') != std::string::npos)
                {
                    meshName = fileCandidate.substr(0, fileCandidate.find('.'));
                    pathRemainder = pathRemainder.substr(0, slash);
                }
            }
        }
        resolveAppearanceAndMeshDirs(pathRemainder, appearanceRootDir, meshWriteDir);
    }
    else
    {
        const char* baseDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::APPEARANCE_WRITE_DIR_INDEX);
        if (!baseDir || !baseDir[0])
        {
            std::cerr << "ExportStaticMesh: appearance output directory not configured" << std::endl;
            return false;
        }
        appearanceRootDir = ensureTrailingSlash(baseDir);
        meshWriteDir = ensureTrailingSlash(appearanceRootDir + "mesh");
    }

    meshName = replaceColonsForAssetFileName(std::move(meshName));

    meshWriteDir = ensureTrailingSlash(meshWriteDir);
    appearanceRootDir = ensureTrailingSlash(appearanceRootDir);
    MayaUtility::createDirectory(winPathForMkdir(appearanceRootDir).c_str());
    MayaUtility::createDirectory(winPathForMkdir(meshWriteDir).c_str());
    {
        const char* sw = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::SHADER_TEMPLATE_WRITE_DIR_INDEX);
        const char* tw = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::TEXTURE_WRITE_DIR_INDEX);
        if (sw && sw[0])
            MayaUtility::createDirectory(winPathForMkdir(ensureTrailingSlash(std::string(sw))).c_str());
        if (tw && tw[0])
            MayaUtility::createDirectory(winPathForMkdir(ensureTrailingSlash(std::string(tw))).c_str());
    }

    MObjectArray shaderObjs;
    MIntArray faceToShader;
    status = meshFn.getConnectedShaders(shapePath.instanceNumber(), shaderObjs, faceToShader);
    if (!status)
    {
        std::cerr << "ExportStaticMesh: getConnectedShaders failed" << std::endl;
        return false;
    }
    if (shaderObjs.length() == 0)
    {
        std::cerr << "ExportStaticMesh: No shading group assigned to this mesh. Assign a material (Lambert/aiStandardSurface/etc.).\n";
        MGlobal::displayError(
            "SwgMsh export: mesh has no shader assignment. Assign a material to the mesh, then export again.");
        return false;
    }

    MPointArray mayaPoints;
    status = meshFn.getPoints(mayaPoints, MSpace::kObject);
    if (!status)
    {
        std::cerr << "ExportStaticMesh: getPoints failed" << std::endl;
        return false;
    }

    const MString uvSetName = chooseExportUvSetName(meshFn, shaderObjs, faceToShader);

    std::map<int, StaticMeshWriterShaderGroup> shaderGroups;

    MItMeshPolygon polyIt(shapePath, MObject::kNullObj, &status);
    if (!status)
    {
        std::cerr << "ExportStaticMesh: MItMeshPolygon failed" << std::endl;
        return false;
    }

    for (; !polyIt.isDone(); polyIt.next())
    {
        int faceIdx = polyIt.index();
        int shaderIdx = (faceIdx < faceToShader.length()) ? faceToShader[faceIdx] : 0;
        if (shaderIdx < 0 || static_cast<unsigned>(shaderIdx) >= shaderObjs.length())
            shaderIdx = 0;

        MObject sgObj = shaderObjs[static_cast<unsigned>(shaderIdx)];
        MFnDependencyNode sgFn(sgObj, &status);
        std::string shaderTemplateName = "shader/placeholder";
        if (status)
        {
            std::string swgPath = getStringAttr(sgFn, "swgShaderPath");
            if (!swgPath.empty())
                shaderTemplateName = swgPath;
        }

        const bool uvDirectThisSg = shadingGroupExportUvDirect(sgObj, objExportDirectUv);

        const MString uvSetForFace = effectiveUvSetForFace(meshFn, sgObj, uvSetName);

        StaticMeshWriterShaderGroup& sg = shaderGroups[shaderIdx];
        if (sg.shaderTemplateName.empty())
            sg.shaderTemplateName = shaderTemplateName;

        MPointArray triPoints;
        MIntArray triVerts;
        status = polyIt.getTriangles(triPoints, triVerts, MSpace::kObject);
        if (!status) continue;

        auto getUV = [&](int meshVert) -> std::pair<float, float> {
            const MString trySets[2] = {uvSetForFace, uvSetName};
            for (int si = 0; si < 2; ++si)
            {
                if (trySets[si].length() == 0) continue;
                if (si > 0 && trySets[si] == trySets[0]) continue;
                for (unsigned i = 0; i < polyIt.polygonVertexCount(&status); ++i)
                {
                    if (polyIt.vertexIndex(static_cast<int>(i), &status) == meshVert)
                    {
                        float2 uv;
                        if (polyIt.getUV(i, uv, &trySets[si]) == MS::kSuccess)
                            return {uv[0], uv[1]};
                    }
                }
            }
            return {0.0f, 0.0f};
        };

        for (unsigned t = 0; t + 2 < triVerts.length(); t += 3)
        {
            int v0 = triVerts[t];
            int v1 = triVerts[t + 1];
            int v2 = triVerts[t + 2];

            MVector nFace;
            meshFn.getPolygonNormal(faceIdx, nFace, MSpace::kObject);
            const MVector n0 = nFace;
            const MVector n1 = nFace;
            const MVector n2 = nFace;

            float u0 = getUV(v0).first, v0_ = getUV(v0).second;
            float u1 = getUV(v1).first, v1_ = getUV(v1).second;
            float u2 = getUV(v2).first, v2_ = getUV(v2).second;

            auto addVert = [&](int vi, float u, float v, const MVector& n) {
                const MPoint& pt = mayaPoints[static_cast<unsigned>(vi)];
                Vector enginePos = MayaConversions::convertVector(MVector(pt.x, pt.y, pt.z));
                Vector engineNorm = MayaConversions::convertVector(n);

                size_t base = sg.positions.size() / 3;
                sg.positions.push_back(enginePos.x);
                sg.positions.push_back(enginePos.y);
                sg.positions.push_back(enginePos.z);
                sg.normals.push_back(engineNorm.x);
                sg.normals.push_back(engineNorm.y);
                sg.normals.push_back(engineNorm.z);
                sg.uvs.push_back(u);
                {
                    const float mv = static_cast<float>(v);
                    // Global or per-shading-group: direct Maya V vs 1-V for game (see shadingGroupExportUvDirect).
                    sg.uvs.push_back(uvDirectThisSg ? mv : (1.0f - mv));
                }

                return static_cast<uint16_t>(base);
            };

            uint16_t i0 = addVert(v0, static_cast<float>(u0), static_cast<float>(v0_), n0);
            uint16_t i1 = addVert(v1, static_cast<float>(u1), static_cast<float>(v1_), n1);
            uint16_t i2 = addVert(v2, static_cast<float>(u2), static_cast<float>(v2_), n2);

            // Winding: Maya viewport uses object-space normals; engine applies -X on positions/normals. Pick a swap
            // so the triangle cross product in engine space agrees with the converted face normal (viewport parity).
            // Degenerate triangles fall back to legacyTriangleFallback / swgExportTriangleSwap on the SG.
            const MPoint& p0m = mayaPoints[static_cast<unsigned>(v0)];
            const MPoint& p1m = mayaPoints[static_cast<unsigned>(v1)];
            const MPoint& p2m = mayaPoints[static_cast<unsigned>(v2)];
            Vector P0 = MayaConversions::convertVector(MVector(p0m.x, p0m.y, p0m.z));
            Vector P1 = MayaConversions::convertVector(MVector(p1m.x, p1m.y, p1m.z));
            Vector P2 = MayaConversions::convertVector(MVector(p2m.x, p2m.y, p2m.z));
            Vector e1 = P1 - P0;
            Vector e2 = P2 - P0;
            Vector G = e1.cross(e2);
            Vector Nref = MayaConversions::convertVector(nFace);
            constexpr real kDegenerateAreaSq = static_cast<real>(1e-18);
            const bool degenerate = G.magnitudeSquared() < kDegenerateAreaSq;
            const bool autoNeedSwap = !degenerate && (G.dot(Nref) < static_cast<real>(0));
            const bool baseSwap = degenerate ? legacyTriangleFallback : autoNeedSwap;
            const bool reverseTrianglesThisSg = shadingGroupExportTriangleSwap(sgObj, baseSwap);

            if (reverseTrianglesThisSg)
            {
                sg.indices.push_back(i0);
                sg.indices.push_back(i2);
                sg.indices.push_back(i1);
            }
            else
            {
                sg.indices.push_back(i0);
                sg.indices.push_back(i1);
                sg.indices.push_back(i2);
            }

            uint16_t ord[3] = {i0, i1, i2};
            if (reverseTrianglesThisSg)
            {
                ord[1] = i2;
                ord[2] = i1;
            }
            Vector Q0(sg.positions[static_cast<size_t>(ord[0]) * 3], sg.positions[static_cast<size_t>(ord[0]) * 3 + 1],
                sg.positions[static_cast<size_t>(ord[0]) * 3 + 2]);
            Vector Q1(sg.positions[static_cast<size_t>(ord[1]) * 3], sg.positions[static_cast<size_t>(ord[1]) * 3 + 1],
                sg.positions[static_cast<size_t>(ord[1]) * 3 + 2]);
            Vector Q2(sg.positions[static_cast<size_t>(ord[2]) * 3], sg.positions[static_cast<size_t>(ord[2]) * 3 + 1],
                sg.positions[static_cast<size_t>(ord[2]) * 3 + 2]);
            Vector geo = (Q1 - Q0).cross(Q2 - Q0);
            if (geo.normalize())
            {
                for (uint16_t idx : ord)
                {
                    const size_t o = static_cast<size_t>(idx) * 3;
                    sg.normals[o] = static_cast<float>(geo.x);
                    sg.normals[o + 1] = static_cast<float>(geo.y);
                    sg.normals[o + 2] = static_cast<float>(geo.z);
                }
            }
        }
    }

    {
        // Every material slot that receives geometry gets a fresh .sht + published DDS from Maya (1:1 with viewport),
        // not a second-hand clone from DATA_ROOT. swgShaderPath (when set) is only the clone *prototype* (effect layout).
        bool anyShaderRebuildFailed = false;
        unsigned geomMaterialCount = 0;
        for (unsigned si = 0; si < shaderObjs.length(); ++si)
        {
            auto it = shaderGroups.find(static_cast<int>(si));
            if (it == shaderGroups.end()) continue;
            if (!it->second.positions.empty() && !it->second.indices.empty())
                ++geomMaterialCount;
        }

        for (unsigned si = 0; si < shaderObjs.length(); ++si)
        {
            auto it = shaderGroups.find(static_cast<int>(si));
            if (it == shaderGroups.end()) continue;
            StaticMeshWriterShaderGroup& g = it->second;
            if (g.positions.empty() || g.indices.empty()) continue;

            MStatus stSG;
            MFnDependencyNode sgFn(shaderObjs[si], &stSG);
            if (!stSG) continue;

            const std::string swgShaderPrototype = getStringAttr(sgFn, "swgShaderPath");
            const bool hue = getHueableFromSg(sgFn);

            const std::string texBase =
                (geomMaterialCount > 1) ? (meshName + "_m" + std::to_string(si)) : (meshName + std::string("_d"));

            std::string outShaderTree = std::string("shader/") + meshName;
            if (geomMaterialCount > 1)
                outShaderTree += "_sg" + std::to_string(si);

            bool doAnimSwts = false;
            std::vector<std::string> animFrameAbsPaths;
            std::vector<std::string> animFrameTreePaths;
            float animFpsMin = 8.f;
            float animFpsMax = 8.f;
            Tag animSwitcherTag = TAG_DRTS;
            MObject animFileObj;
            if (primaryFileTextureFromShadingGroup(shaderObjs[si], animFileObj))
            {
                MFnDependencyNode animFileFn(animFileObj);
                bool useFrameExt = false;
                MPlug ufePlug = animFileFn.findPlug("useFrameExtension", true);
                if (!ufePlug.isNull()) ufePlug.getValue(useFrameExt);
                if (useFrameExt)
                {
                    std::string samplePath;
                    if (tryReadFileOrMovieTexturePath(animFileFn, samplePath)
                        && collectNumericSequenceFramesFromSamplePath(samplePath, animFrameAbsPaths))
                    {
                        doAnimSwts = true;
                        int orderEnum = 0;
                        if (!readAnimatingTextureParamsFromFileNode(animFileFn, animFpsMin, animFpsMax, orderEnum))
                        {
                            animFpsMin
                                = 8.f; // defaults when useFrameExtension is on but legacy attrs are missing
                            animFpsMax = 8.f;
                            orderEnum = 0;
                        }
                        animSwitcherTag = switcherTagForAnimOrder(orderEnum);
                    }
                }
            }

            std::string texTree;
            std::string absImage;

            if (doAnimSwts)
            {
                animFrameTreePaths.clear();
                bool framesPublished = true;
                for (size_t fi = 0; fi < animFrameAbsPaths.size(); ++fi)
                {
                    const std::string stem = frameStemForAnim(texBase, fi);
                    const std::string ft =
                        ShaderExporter::publishDiffuseTextureForGame(animFrameAbsPaths[fi], stem);
                    if (ft.empty())
                    {
                        framesPublished = false;
                        std::cerr << "[ExportStaticMesh] Animated texture: failed to publish frame " << fi << " from "
                                  << animFrameAbsPaths[fi] << "\n";
                        break;
                    }
                    animFrameTreePaths.push_back(ft);
                }
                if (!framesPublished)
                {
                    doAnimSwts = false;
                    MGlobal::displayWarning(
                        MString("SwgMayaEditor: animated texture export failed for \"") + outShaderTree.c_str()
                        + "\" — check nvtt / paths. Falling back to static shader if possible.");
                }
                else
                {
                    texTree = animFrameTreePaths[0];
                    absImage = animFrameAbsPaths[0];
                    for (const std::string& ft : animFrameTreePaths)
                    {
                        const std::string ddsAbs = absolutePathForTextureWriteDirDds(ft);
                        if (!ddsAbs.empty()) pushUniquePath(exportedDdsAbsPaths, ddsAbs);
                    }
                    std::cerr << "[ExportStaticMesh] Animated texture: " << animFrameAbsPaths.size()
                              << " frames -> SWTS \"" << outShaderTree << "\" (fps " << animFpsMin << ".." << animFpsMax
                              << ")\n";
                }
            }

            if (!doAnimSwts)
            {
                // 1) Drop-in under textureWriteDir (<mesh>_d / <mesh>_mN.*) wins over the file node path so swapping a
                //    .tga/.png beside the published DDS actually exports (viewport may still point at an older path).
                absImage = tryFindImageInTextureWriteDir(texBase);
                if (!absImage.empty())
                {
                    texTree = ShaderExporter::publishDiffuseTextureForGame(absImage, texBase);
                    if (texTree.empty())
                        std::cerr << "[ExportStaticMesh] Could not publish texture from textureWriteDir drop-in: " << absImage
                                  << "\n";
                    else
                        std::cerr << "[ExportStaticMesh] Published diffuse from textureWriteDir (" << texBase << "): " << absImage
                                  << " -> " << texTree << "\n";
                }

                // 2) Maya shading network (file/aiImage) — viewport / source images when no drop-in.
                if (texTree.empty() && tryGetDiffuseImageAbsolutePath(shaderObjs[si], absImage))
                {
                    texTree = ShaderExporter::publishDiffuseTextureForGame(absImage, texBase);
                    if (texTree.empty())
                        std::cerr << "[ExportStaticMesh] Could not publish texture (textureWriteDir / nvtt). Image: " << absImage
                                  << "\n";
                }

                // 3) swgTexturePath (often set on import): bake from disk when possible, not only a tree string.
                if (texTree.empty())
                {
                    std::string swgTex = getStringAttr(sgFn, "swgTexturePath");
                    if (!swgTex.empty())
                    {
                        if (looksLikeFilesystemImagePathLocal(swgTex))
                        {
                            texTree = ShaderExporter::publishDiffuseTextureForGame(swgTex, texBase);
                            if (texTree.empty())
                                std::cerr << "[ExportStaticMesh] Could not publish swgTexturePath image: " << swgTex << "\n";
                        }
                        else
                        {
                            const std::string stem = textureStemFromSwgOrTreePath(swgTex);
                            absImage = tryFindImageInTextureWriteDir(stem);
                            if (!absImage.empty())
                            {
                                texTree = ShaderExporter::publishDiffuseTextureForGame(absImage, texBase);
                                if (texTree.empty())
                                    std::cerr << "[ExportStaticMesh] Could not publish swgTexturePath disk file: " << absImage
                                              << "\n";
                                else
                                    std::cerr << "[ExportStaticMesh] Published diffuse from swgTexturePath basename in textureWriteDir: "
                                              << absImage << " -> " << texTree << "\n";
                            }
                            else
                                texTree = normalizeTextureTreeForShader(swgTex);
                        }
                    }
                }
            }

            if (texTree.empty())
            {
                if (!doAnimSwts)
                {
                    MString surfaceType("unknown");
                    MPlug surfPlug = sgFn.findPlug("surfaceShader", true);
                    if (!surfPlug.isNull())
                    {
                        MPlugArray scon;
                        surfPlug.connectedTo(scon, true, false);
                        if (scon.length() > 0)
                        {
                            MStatus stSurf;
                            MFnDependencyNode sfn(scon[0].node(), &stSurf);
                            if (stSurf)
                                surfaceType = sfn.typeName();
                        }
                    }
                    std::cerr << "[ExportStaticMesh] No diffuse/DDS source for shading group \""
                              << sgFn.name().asChar() << "\" (slot " << si << ", surface=" << surfaceType.asChar()
                              << "). Connect file/aiImage to baseColor/color, or set swgTexturePath.\n";
                }
            }

            if (!texTree.empty())
            {
                const std::string ddsAbsForBundle = absolutePathForTextureWriteDirDds(texTree);
                if (!ddsAbsForBundle.empty())
                    pushUniquePath(exportedDdsAbsPaths, ddsAbsForBundle);
            }

            std::string diffuseAbsForAlpha = absImage;
            if (diffuseAbsForAlpha.empty())
            {
                const std::string swgTexPath = getStringAttr(sgFn, "swgTexturePath");
                if (looksLikeFilesystemImagePathLocal(swgTexPath))
                    diffuseAbsForAlpha = swgTexPath;
            }
            const bool fileAlphaHint =
                !diffuseAbsForAlpha.empty() && diffuseImagePathOftenHasAlphaFile(diffuseAbsForAlpha);
            const bool transparent =
                shadingGroupIndicatesTransparency(shaderObjs[si]) || fileAlphaHint;

            std::string effectOverrideStr = normalizedEffectPathFromSurfaceShader(shaderObjs[si]);
            const std::string* effectOverridePtr = effectOverrideStr.empty() ? nullptr : &effectOverrideStr;

            std::string cloned;
            std::string animBaseWrittenAbsForVerify;
            if (doAnimSwts && !animFrameTreePaths.empty())
            {
                const std::string baseTree = outShaderTree + "_base";
                const std::string baseCloned = ShaderExporter::exportShaderClonedFromPrototype(
                    baseTree, swgShaderPrototype, animFrameTreePaths[0], hue, true, transparent, effectOverridePtr);
                if (baseCloned.empty())
                {
                    anyShaderRebuildFailed = true;
                    std::cerr << "[ExportStaticMesh] Animated export: failed to write base shader " << baseTree << "\n";
                    MGlobal::displayWarning(
                        MString("SwgMayaEditor: failed base shader for animated export: ") + baseTree.c_str());
                }
                else
                {
                    animBaseWrittenAbsForVerify = baseCloned;
                    pushUniquePath(exportedShaderAbsPaths, baseCloned);
                    cloned = ShaderExporter::exportSwitchTextureAnimatedShader(
                        outShaderTree, baseTree, animSwitcherTag, animFpsMin, animFpsMax, animFrameTreePaths);
                    if (cloned.empty())
                    {
                        anyShaderRebuildFailed = true;
                        std::cerr << "[ExportStaticMesh] Animated export: failed SWTS " << outShaderTree << "\n";
                    }
                }
            }
            else
            {
                cloned = ShaderExporter::exportShaderClonedFromPrototype(
                    outShaderTree, swgShaderPrototype, texTree, hue, !texTree.empty(), transparent, effectOverridePtr);
            }
            if (!cloned.empty())
            {
                if (doAnimSwts && !animFrameTreePaths.empty() && !animBaseWrittenAbsForVerify.empty())
                {
                    if (!verifyAnimatedShaderExportArtifacts(
                            cloned, animBaseWrittenAbsForVerify, animFrameTreePaths, outShaderTree))
                        return false;

                    g.shaderTemplateName = outShaderTree;
                    std::cerr << "[ExportStaticMesh] Rebuilt animated shader (SWTS): " << outShaderTree
                              << " base=" << (outShaderTree + "_base") << " frames=" << animFrameTreePaths.size()
                              << " fps=" << animFpsMin << ".." << animFpsMax << "\n";
                    pushUniquePath(exportedShaderAbsPaths, cloned);
                    MGlobal::displayInfo(
                        MString("SwgMayaEditor: wrote animated shader (SWTS) — copy shader/ + texture/ + appearance/ to viewer: ")
                        + cloned.c_str());
                }
                else
                {
                    if (!verifyShaderExportArtifacts(cloned, texTree, outShaderTree))
                        return false;

                    g.shaderTemplateName = outShaderTree;
                    std::cerr << "[ExportStaticMesh] Rebuilt shader: " << outShaderTree
                              << " proto=" << (swgShaderPrototype.empty() ? "<default>" : swgShaderPrototype.c_str())
                              << " hueable=" << (hue ? "yes" : "no")
                              << " transparent=" << (transparent ? "yes" : "no")
                              << " effectOverride=" << (effectOverrideStr.empty() ? "<none>" : effectOverrideStr.c_str())
                              << " tex=" << (texTree.empty() ? "<prototype>" : texTree.c_str()) << "\n";
                    pushUniquePath(exportedShaderAbsPaths, cloned);
                    MGlobal::displayInfo(
                        MString("SwgMayaEditor: wrote shader - copy shader/ + texture/ + appearance/ as one tree into the viewer (same as setBaseDir): ")
                        + cloned.c_str());
                }
            }
            else
            {
                anyShaderRebuildFailed = true;
                const char* sw =
                    SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::SHADER_TEMPLATE_WRITE_DIR_INDEX);
                if (!sw || !sw[0])
                    std::cerr << "[ExportStaticMesh] Shader rebuild failed for " << outShaderTree
                              << ": shaderTemplateWriteDir not set (setBaseDir).\n";
                else
                    std::cerr << "[ExportStaticMesh] Shader rebuild failed for " << outShaderTree
                              << " (see [ShaderExporter] errors). Leaving mesh shader path unset or stale.\n";
                MGlobal::displayWarning(
                    MString("SwgMayaEditor: shader rebuild failed for \"") + outShaderTree.c_str()
                    + "\". The mesh may reference a missing .sht in the viewer. Check setBaseDir, shader prototype, "
                      "and Script Editor for [ShaderExporter] errors.");
            }
        }

        if (anyShaderRebuildFailed)
        {
            MGlobal::displayError(
                "SwgMsh export aborted: at least one material did not produce a .sht on disk. The viewer loads "
                "shader/<name>.sht from the same tree as appearance/ and texture/. Run setBaseDir to your export root, "
                "put shader/defaultshader.sht under DATA_ROOT or shaderTemplateWriteDir, set shaderPrototypeSht in "
                "SwgMayaEditor.cfg if needed, and read Script Editor lines [ShaderExporter] / [TgaToDds].");
            return false;
        }
    }

    StaticMeshWriter writer;

    for (unsigned i = 0; i < shaderObjs.length(); ++i)
    {
        auto it = shaderGroups.find(static_cast<int>(i));
        if (it != shaderGroups.end() && !it->second.positions.empty() && !it->second.indices.empty())
            writer.addShaderGroup(it->second);
    }

    bool hardpointsFromAttr = false;
    {
        std::vector<StaticMeshWriterHardpoint> blobHps;
        MFnDependencyNode meshTfFn(transformPath.node(), &status);
        if (status)
        {
            MPlug hpPlug = meshTfFn.findPlug("swgSkmgHardpoints", true);
            if (!hpPlug.isNull())
            {
                MString blobStr;
                if (hpPlug.getValue(blobStr) == MS::kSuccess && blobStr.length() > 0
                    && parseSwgSkmgHardpointsForStaticExport(std::string(blobStr.asChar()), blobHps))
                {
                    hardpointsFromAttr = true;
                    for (const auto& h : blobHps)
                        writer.addHardpoint(h);
                }
            }
        }
        if (!hardpointsFromAttr && transformPath.length() >= 2)
        {
            MDagPath parentDag = transformPath;
            parentDag.pop(1);
            MFnDependencyNode parentFn(parentDag.node(), &status);
            if (status)
            {
                MPlug hpPlug2 = parentFn.findPlug("swgSkmgHardpoints", true);
                if (!hpPlug2.isNull())
                {
                    MString blobStr;
                    if (hpPlug2.getValue(blobStr) == MS::kSuccess && blobStr.length() > 0
                        && parseSwgSkmgHardpointsForStaticExport(std::string(blobStr.asChar()), blobHps))
                    {
                        hardpointsFromAttr = true;
                        for (const auto& h : blobHps)
                            writer.addHardpoint(h);
                    }
                }
            }
        }
    }

    if (!hardpointsFromAttr && transformPath.childCount(&status) > 0 && status)
    {
        for (unsigned c = 0; c < transformPath.childCount(&status); ++c)
        {
            MObject childObj = transformPath.child(c, &status);
            if (!status) continue;
            MFnDagNode childFn(childObj, &status);
            if (!status) continue;
            if (childFn.typeId(&status) != MFn::kTransform) continue;

            MDagPath childPath;
            if (!childFn.getPath(childPath)) continue;

            MFnTransform childTransform(childPath, &status);
            if (!status) continue;

            MString hpName = childTransform.name(&status);
            if (!status) continue;

            MString hpNameLower = hpName;
            hpNameLower.toLowerCase();
            if (hpNameLower == "hardpoints")
                continue;

            if (hpName == "floor_component")
            {
                MFnDependencyNode depFn(childPath.node(), &status);
                if (status)
                {
                    MPlug fp = depFn.findPlug("floorPath", false);
                    if (!fp.isNull())
                    {
                        MString pathVal;
                        if (fp.getValue(pathVal) == MS::kSuccess && pathVal.length() > 0)
                            writer.setFloorReference(pathVal.asChar());
                    }
                }
                continue;
            }

            MVector mayaTrans = childTransform.translation(MSpace::kTransform, &status);
            if (!status) continue;

            MEulerRotation mayaRot;
            status = childTransform.getRotation(mayaRot);
            if (!status) continue;

            Vector enginePos = MayaConversions::convertVector(mayaTrans);
            Quaternion engineRot = MayaConversions::convertRotation(mayaRot);

            StaticMeshWriterHardpoint hp;
            hp.name = hpName.asChar();
            hp.position[0] = enginePos.x;
            hp.position[1] = enginePos.y;
            hp.position[2] = enginePos.z;
            hp.rotation[0] = engineRot.x;
            hp.rotation[1] = engineRot.y;
            hp.rotation[2] = engineRot.z;
            hp.rotation[3] = engineRot.w;
            writer.addHardpoint(hp);
        }
    }

    std::string mshFullPath = meshWriteDir + meshName + ".msh";
    Iff iff(DEFAULT_IFF_SIZE, true);
    if (!writer.write(iff))
    {
        std::cerr << "ExportStaticMesh: StaticMeshWriter::write failed" << std::endl;
        return false;
    }
    if (!iff.write(mshFullPath.c_str(), true))
    {
        std::cerr << "ExportStaticMesh: failed to write " << mshFullPath << std::endl;
        return false;
    }

    // Mesh lives only under appearance/mesh/ (setBaseDir). Do not write a second .msh at the File dialog path
    // (e.g. D:/exported/foo.msh). If Maya reports "Could not save file", export to
    // <setBaseDir>/appearance/mesh/<name>.msh or use exportStaticMesh -path.

    outMeshPath = toTreePath(mshFullPath);
    if (outMeshPath.find("appearance/") != 0 && outMeshPath.find("appearance\\") != 0)
        outMeshPath = "appearance/mesh/" + meshName + ".msh";

    // APT lives under appearance/; .msh under appearance/mesh/ (shader/ and texture/ are export-root siblings via SetDirectoryCommand).
    std::string aptDiskPathWritten;
    {
        std::string aptPath = appearanceRootDir + meshName + ".apt";
        Iff aptIff(4096, true);
        aptIff.insertForm(TAG_APT);
        aptIff.insertForm(::TAG_0000);
        aptIff.insertChunk(::TAG_NAME);
        aptIff.insertChunkString(outMeshPath.c_str());
        aptIff.exitChunk(::TAG_NAME);
        aptIff.exitForm(::TAG_0000);
        aptIff.exitForm(TAG_APT);
        if (!aptIff.write(aptPath.c_str(), true))
            std::cerr << "[ExportStaticMesh] Failed to write APT: " << aptPath << std::endl;
        else
        {
            outAptPath = toTreePath(aptPath);
            aptDiskPathWritten = aptPath;
        }
    }

    mirrorViewerBundleToDataRoot(exportedShaderAbsPaths, exportedDdsAbsPaths, mshFullPath, aptDiskPathWritten, meshName);

    return true;
}
