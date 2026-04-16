#include "lsb.h"
#include "SwgTranslatorNames.h"
#include "ImportPathResolver.h"
#include "Iff.h"
#include "Tag.h"
#include "MayaUtility.h"

#include <maya/MFileObject.h>
#include <maya/MFn.h>
#include <maya/MFnTransform.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MDagPath.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>

#include <cctype>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#define LSB_STRICMP _stricmp
#else
#define LSB_STRICMP strcasecmp
#endif

struct LsbBladeRow
{
    std::string shader;
    float length = 1.8f;
    float width = 0.15f;
    float openRate = 0.5f;
    float closeRate = 0.5f;
};

namespace
{
    const Tag TAG_LSAT = TAG(L, S, A, T);
    const Tag TAG_BASE = TAG(B, A, S, E);
    const Tag TAG_ASND = TAG(A, S, N, D);
    const Tag TAG_LGHT = TAG(L, G, H, T);
    const Tag TAG_BLAD = TAG(B, L, A, D);
    const Tag TAG_SHDR = TAG(S, H, D, R);
    const Tag TAG_LGTH = TAG(L, G, T, H);
    const Tag TAG_WDTH = TAG(W, D, T, H);
    const Tag TAG_OPEN = TAG(O, P, E, N);
    const Tag TAG_CLOS = TAG(C, L, O, S);
    const Tag TAG_COLR = TAG(C, O, L, R);
    const Tag TAG_RANG = TAG(R, A, N, G);
    const Tag TAG_TIME = TAG(T, I, M, E);
    const Tag TAG_DAYN = TAG(D, A, Y, N);

    static bool bufferLooksLikeLsb(const char* buffer, short size)
    {
        if (!buffer || size < 12) return false;
        if (memcmp(buffer, "FORM", 4) != 0) return false;
        return memcmp(buffer + 8, "LSAT", 4) == 0;
    }

    static void addStringAttrMEL(const std::string& nodeFullPath, const char* attrName, const std::string& value)
    {
        std::string esc;
        for (char c : value)
        {
            if (c == '\\') esc += "\\\\";
            else if (c == '"') esc += "\\\"";
            else esc += c;
        }
        MString cmd = "if (!`attributeExists \"";
        cmd += attrName;
        cmd += "\" \"";
        cmd += nodeFullPath.c_str();
        cmd += "\"`) { addAttr -ln \"";
        cmd += attrName;
        cmd += "\" -dt \"string\" \"";
        cmd += nodeFullPath.c_str();
        cmd += "\"; }";
        MGlobal::executeCommand(cmd);
        MString setCmd = "setAttr -type \"string\" \"";
        setCmd += nodeFullPath.c_str();
        setCmd += ".";
        setCmd += attrName;
        setCmd += "\" \"";
        setCmd += esc.c_str();
        setCmd += "\"";
        MGlobal::executeCommand(setCmd);
    }

    static void addIntAttrMEL(const std::string& nodeFullPath, const char* attrName, int v)
    {
        MString cmd = "if (!`attributeExists \"";
        cmd += attrName;
        cmd += "\" \"";
        cmd += nodeFullPath.c_str();
        cmd += "\"`) { addAttr -ln \"";
        cmd += attrName;
        cmd += "\" -at long \"";
        cmd += nodeFullPath.c_str();
        cmd += "\"; }";
        MGlobal::executeCommand(cmd);
        MString setCmd = "setAttr \"";
        setCmd += nodeFullPath.c_str();
        setCmd += ".";
        setCmd += attrName;
        setCmd += "\" ";
        setCmd += v;
        MGlobal::executeCommand(setCmd);
    }

    static std::string plugString(const MFnDependencyNode& dep, const char* attrName)
    {
        if (!dep.hasAttribute(attrName)) return std::string();
        MStatus st;
        MPlug p = dep.findPlug(attrName, true, &st);
        if (!st || p.isNull()) return std::string();
        MString v;
        if (p.getValue(v) != MS::kSuccess) return std::string();
        return std::string(v.asChar());
    }

    static int plugInt(const MFnDependencyNode& dep, const char* attrName, int defVal)
    {
        if (!dep.hasAttribute(attrName)) return defVal;
        MStatus st;
        MPlug p = dep.findPlug(attrName, true, &st);
        if (!st || p.isNull()) return defVal;
        int v = defVal;
        p.getValue(v);
        return v;
    }
}

void* LsbTranslator::creator()
{
    return new LsbTranslator();
}

MString LsbTranslator::defaultExtension() const
{
    return "lsb";
}

MString LsbTranslator::filter() const
{
    return MString(swg_translator::kFilterLsb);
}

MPxFileTranslator::MFileKind LsbTranslator::identifyFile(const MFileObject& fileName, const char* buffer, short size) const
{
    const std::string pathStr = MayaUtility::fileObjectPathForIdentify(fileName);
    const int n = static_cast<int>(pathStr.size());
    if (n > 4 && LSB_STRICMP(pathStr.c_str() + n - 4, ".lsb") == 0)
        return kCouldBeMyFileType;
    if (bufferLooksLikeLsb(buffer, size))
        return kCouldBeMyFileType;
    return kNotMyFileType;
}

MStatus LsbTranslator::reader(const MFileObject& file, const MString&, MPxFileTranslator::FileAccessMode)
{
    std::string pathStr = MayaUtility::fileObjectPathForIdentify(file);
    pathStr = resolveImportPath(pathStr);
    const char* fileName = pathStr.c_str();
    if (!Iff::isValid(fileName))
        return MS::kFailure;
    Iff iff;
    if (!iff.open(fileName, false) || iff.getRawDataSize() < 1)
    {
        MGlobal::displayError("LsbTranslator: could not open file");
        return MS::kFailure;
    }
    if (!iff.enterForm(TAG_LSAT, true))
    {
        MGlobal::displayError("LsbTranslator: root form must be LSAT (FORM … LSAT)");
        return MS::kFailure;
    }
    if (iff.getCurrentName() != TAG_0000)
    {
        MGlobal::displayError("LsbTranslator: unsupported LSAT version (expected 0000)");
        iff.exitForm(TAG_LSAT);
        return MS::kFailure;
    }

    std::string hiltPath;
    std::string ambientSound;
    bool useFlicker = false;
    uint8_t flickR = 32, flickG = 32, flickB = 32, flickA = 255;
    float flickMinR = 1.f, flickMaxR = 2.f, flickMinT = 0.02f, flickMaxT = 0.1f;
    bool flickDayNight = false;

    std::vector<LsbBladeRow> blades;

    iff.enterForm(TAG_0000);
    while (!iff.atEndOfForm())
    {
        const Tag t = iff.getCurrentName();
        if (t == TAG_BASE)
        {
            iff.enterChunk(TAG_BASE);
            char buf[1024];
            iff.read_string(buf, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = 0;
            hiltPath = buf;
            iff.exitChunk(TAG_BASE);
        }
        else if (t == TAG_ASND)
        {
            iff.enterChunk(TAG_ASND);
            char buf[1024];
            iff.read_string(buf, sizeof(buf) - 1);
            buf[sizeof(buf) - 1] = 0;
            ambientSound = buf;
            iff.exitChunk(TAG_ASND);
        }
        else if (t == TAG_LGHT)
        {
            useFlicker = true;
            iff.enterForm(TAG_LGHT);
            while (!iff.atEndOfForm())
            {
                const Tag lt = iff.getCurrentName();
                if (lt == TAG_COLR)
                {
                    iff.enterChunk(TAG_COLR);
                    flickR = iff.read_uint8();
                    flickG = iff.read_uint8();
                    flickB = iff.read_uint8();
                    flickA = iff.read_uint8();
                    iff.exitChunk(TAG_COLR);
                }
                else if (lt == TAG_RANG)
                {
                    iff.enterChunk(TAG_RANG);
                    flickMinR = iff.read_float();
                    flickMaxR = iff.read_float();
                    iff.exitChunk(TAG_RANG);
                }
                else if (lt == TAG_TIME)
                {
                    iff.enterChunk(TAG_TIME);
                    flickMinT = iff.read_float();
                    flickMaxT = iff.read_float();
                    iff.exitChunk(TAG_TIME);
                }
                else if (lt == TAG_DAYN)
                {
                    iff.enterChunk(TAG_DAYN);
                    flickDayNight = iff.read_uint8() != 0;
                    iff.exitChunk(TAG_DAYN);
                }
                else
                {
                    iff.enterChunk();
                    iff.exitChunk();
                }
            }
            iff.exitForm(TAG_LGHT);
        }
        else if (t == TAG_BLAD)
        {
            LsbBladeRow br;
            iff.enterForm(TAG_BLAD);
            while (!iff.atEndOfForm())
            {
                const Tag bt = iff.getCurrentName();
                if (bt == TAG_SHDR)
                {
                    iff.enterChunk(TAG_SHDR);
                    char buf[1024];
                    iff.read_string(buf, sizeof(buf) - 1);
                    buf[sizeof(buf) - 1] = 0;
                    br.shader = buf;
                    iff.exitChunk(TAG_SHDR);
                }
                else if (bt == TAG_LGTH)
                {
                    iff.enterChunk(TAG_LGTH);
                    br.length = iff.read_float();
                    iff.exitChunk(TAG_LGTH);
                }
                else if (bt == TAG_WDTH)
                {
                    iff.enterChunk(TAG_WDTH);
                    br.width = iff.read_float();
                    iff.exitChunk(TAG_WDTH);
                }
                else if (bt == TAG_OPEN)
                {
                    iff.enterChunk(TAG_OPEN);
                    br.openRate = iff.read_float();
                    iff.exitChunk(TAG_OPEN);
                }
                else if (bt == TAG_CLOS)
                {
                    iff.enterChunk(TAG_CLOS);
                    br.closeRate = iff.read_float();
                    iff.exitChunk(TAG_CLOS);
                }
                else
                {
                    iff.enterChunk();
                    iff.exitChunk();
                }
            }
            iff.exitForm(TAG_BLAD);
            blades.push_back(std::move(br));
        }
        else
        {
            iff.enterChunk();
            iff.exitChunk();
        }
    }
    iff.exitForm(TAG_0000);
    iff.exitForm(TAG_LSAT);

    std::string nodeName = pathStr;
    const size_t slash = nodeName.find_last_of("/\\");
    if (slash != std::string::npos) nodeName = nodeName.substr(slash + 1);
    const size_t dot = nodeName.find_last_of('.');
    if (dot != std::string::npos) nodeName = nodeName.substr(0, dot);
    if (nodeName.empty()) nodeName = "lsb";

    MFnTransform tf;
    MObject tObj = tf.create();
    tf.setName(MString(nodeName.c_str()));
    const std::string fullPath(tf.fullPathName().asChar());

    addStringAttrMEL(fullPath, "swgLsbHiltPath", hiltPath);
    addStringAttrMEL(fullPath, "swgLsbAmbientSound", ambientSound);
    addIntAttrMEL(fullPath, "swgLsbUseLightFlicker", useFlicker ? 1 : 0);
    addIntAttrMEL(fullPath, "swgLsbFlickerDayNight", flickDayNight ? 1 : 0);
    {
        std::ostringstream oss;
        oss << static_cast<int>(flickR) << ' ' << static_cast<int>(flickG) << ' ' << static_cast<int>(flickB) << ' '
            << static_cast<int>(flickA) << ' ' << flickMinR << ' ' << flickMaxR << ' ' << flickMinT << ' ' << flickMaxT;
        addStringAttrMEL(fullPath, "swgLsbFlickerParams", oss.str());
    }
    addIntAttrMEL(fullPath, "swgLsbBladeCount", static_cast<int>(blades.size()));
    for (size_t i = 0; i < blades.size(); ++i)
    {
        const LsbBladeRow& b = blades[i];
        std::ostringstream line;
        line << b.shader << '\t' << b.length << '\t' << b.width << '\t' << b.openRate << '\t' << b.closeRate;
        std::string an = "swgLsbBlade";
        an += std::to_string(static_cast<int>(i));
        addStringAttrMEL(fullPath, an.c_str(), line.str());
    }

    {
        MString msg("LsbTranslator: imported ");
        msg += fullPath.c_str();
        msg += " (blades=";
        msg += static_cast<int>(blades.size());
        msg += ")";
        MGlobal::displayInfo(msg);
    }
    return MS::kSuccess;
}

MStatus LsbTranslator::writer(const MFileObject& file, const MString&, MPxFileTranslator::FileAccessMode)
{
    const char* outPath = file.expandedFullName().asChar();
    MSelectionList sel;
    MGlobal::getActiveSelectionList(sel);
    if (sel.isEmpty())
    {
        MGlobal::displayError("LsbTranslator: select the transform with swgLsb* attributes");
        return MS::kFailure;
    }
    MDagPath dag;
    if (!sel.getDagPath(0, dag))
        return MS::kFailure;
    if (dag.hasFn(MFn::kMesh))
        dag.pop();
    MFnDependencyNode dep(dag.node());
    const std::string hilt = plugString(dep, "swgLsbHiltPath");
    if (hilt.empty())
    {
        MGlobal::displayError("LsbTranslator: swgLsbHiltPath is empty on selection");
        return MS::kFailure;
    }
    const std::string ambient = plugString(dep, "swgLsbAmbientSound");
    const int useFlicker = plugInt(dep, "swgLsbUseLightFlicker", 0);
    const int flickDay = plugInt(dep, "swgLsbFlickerDayNight", 0);
    const std::string flickParams = plugString(dep, "swgLsbFlickerParams");
    int flickR = 32, flickG = 32, flickB = 32, flickA = 255;
    float flickMinR = 1.f, flickMaxR = 2.f, flickMinT = 0.02f, flickMaxT = 0.1f;
    if (!flickParams.empty())
    {
        std::istringstream iss(flickParams);
        iss >> flickR >> flickG >> flickB >> flickA >> flickMinR >> flickMaxR >> flickMinT >> flickMaxT;
    }
    const int bladeCount = plugInt(dep, "swgLsbBladeCount", 0);
    std::vector<LsbBladeRow> blades;
    if (bladeCount > 0)
    {
        blades.resize(static_cast<size_t>(bladeCount));
        for (int i = 0; i < bladeCount; ++i)
        {
            std::string an = "swgLsbBlade" + std::to_string(i);
            const std::string line = plugString(dep, an.c_str());
            if (line.empty()) continue;
            std::istringstream ls(line);
            std::getline(ls, blades[static_cast<size_t>(i)].shader, '\t');
            ls >> blades[static_cast<size_t>(i)].length;
            ls >> blades[static_cast<size_t>(i)].width;
            ls >> blades[static_cast<size_t>(i)].openRate;
            ls >> blades[static_cast<size_t>(i)].closeRate;
        }
    }
    if (blades.empty())
    {
        LsbBladeRow one;
        one.shader = "shader/default_blade.sht";
        blades.push_back(one);
    }

    Iff iff(64 * 1024);
    iff.insertForm(TAG_LSAT);
    {
        iff.insertForm(TAG_0000);
        {
            iff.insertChunk(TAG_BASE);
            iff.insertChunkString(hilt.c_str());
            iff.exitChunk(TAG_BASE);

            iff.insertChunk(TAG_ASND);
            iff.insertChunkString(ambient.empty() ? "" : ambient.c_str());
            iff.exitChunk(TAG_ASND);

            if (useFlicker)
            {
                iff.insertForm(TAG_LGHT);
                {
                    iff.insertChunk(TAG_COLR);
                    iff.insertChunkData(static_cast<uint8>(flickR));
                    iff.insertChunkData(static_cast<uint8>(flickG));
                    iff.insertChunkData(static_cast<uint8>(flickB));
                    iff.insertChunkData(static_cast<uint8>(flickA));
                    iff.exitChunk(TAG_COLR);
                    iff.insertChunk(TAG_RANG);
                    iff.insertChunkData(flickMinR);
                    iff.insertChunkData(flickMaxR);
                    iff.exitChunk(TAG_RANG);
                    iff.insertChunk(TAG_TIME);
                    iff.insertChunkData(flickMinT);
                    iff.insertChunkData(flickMaxT);
                    iff.exitChunk(TAG_TIME);
                    iff.insertChunk(TAG_DAYN);
                    iff.insertChunkData(static_cast<uint8>(flickDay ? 1 : 0));
                    iff.exitChunk(TAG_DAYN);
                }
                iff.exitForm(TAG_LGHT);
            }

            for (const LsbBladeRow& b : blades)
            {
                iff.insertForm(TAG_BLAD);
                {
                    iff.insertChunk(TAG_SHDR);
                    iff.insertChunkString(b.shader.c_str());
                    iff.exitChunk(TAG_SHDR);
                    iff.insertChunk(TAG_LGTH);
                    iff.insertChunkData(b.length);
                    iff.exitChunk(TAG_LGTH);
                    iff.insertChunk(TAG_WDTH);
                    iff.insertChunkData(b.width);
                    iff.exitChunk(TAG_WDTH);
                    iff.insertChunk(TAG_OPEN);
                    iff.insertChunkData(b.openRate);
                    iff.exitChunk(TAG_OPEN);
                    iff.insertChunk(TAG_CLOS);
                    iff.insertChunkData(b.closeRate);
                    iff.exitChunk(TAG_CLOS);
                }
                iff.exitForm(TAG_BLAD);
            }
        }
        iff.exitForm(TAG_0000);
    }
    iff.exitForm(TAG_LSAT);

    if (!iff.write(outPath))
    {
        MGlobal::displayError(MString("LsbTranslator: failed to write ") + outPath);
        return MS::kFailure;
    }
    MGlobal::displayInfo(MString("LsbTranslator: wrote ") + outPath);
    return MS::kSuccess;
}
