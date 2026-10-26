#include "ImportShader.h"
#include "ImportPathResolver.h"
#include "MayaSceneBuilder.h"
#include "DdsToTgaConverter.h"
#include "SetDirectoryCommand.h"
#include "MayaUtility.h"
#include "Iff.h"
#include "Tag.h"

#include <maya/MArgList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MGlobal.h>

#include <string>
#include <vector>
#include <iostream>
#include <cstdarg>
#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
    static void shaderImportLog(const char* fmt, ...)
    {
        char buf[1024];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        std::string msg = std::string("[ImportShader] ") + buf + "\n";
        std::cerr << msg;
#ifdef _WIN32
        OutputDebugStringA(msg.c_str());
#endif
    }

    static std::string tagToFourcc(Tag t)
    {
        char s[5];
        s[0] = static_cast<char>((t >> 24) & 0xff);
        s[1] = static_cast<char>((t >> 16) & 0xff);
        s[2] = static_cast<char>((t >> 8) & 0xff);
        s[3] = static_cast<char>(t & 0xff);
        s[4] = 0;
        return std::string(s);
    }

    static bool directoryIsWritable(std::string dir)
    {
        if (dir.empty())
            return false;
        for (char& c : dir)
            if (c == '/')
                c = '\\';
        while (!dir.empty() && (dir.back() == '\\' || dir.back() == '/'))
            dir.pop_back();
        if (dir.empty())
            return false;
        dir += '\\';
        MayaUtility::createDirectory(dir.c_str());
        const std::string probe = dir + "_swg_tga_write_probe.tmp";
        FILE* f = fopen(probe.c_str(), "wb");
        if (!f)
            return false;
        fclose(f);
        remove(probe.c_str());
        return true;
    }

    static std::string normalizeDir(std::string dir)
    {
        for (char& c : dir)
            if (c == '/')
                c = '\\';
        if (!dir.empty() && dir.back() != '\\')
            dir += '\\';
        return dir;
    }

    /// Default textureWriteDir is "\exported\texture\" which is not writable. Prefer data-root
    /// exported/texture, then %TEMP%, then the DDS folder, so File>Import can bind a real map.
    static std::string writableTgaOutputDir(const std::string& ddsPath)
    {
        std::vector<std::string> candidates;
        const char* textureWriteDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::TEXTURE_WRITE_DIR_INDEX);
        if (textureWriteDir && textureWriteDir[0] && textureWriteDir[1] == ':')
            candidates.push_back(textureWriteDir);

        const std::string staging = getExportedStagingDirectory();
        if (!staging.empty())
            candidates.push_back(staging + "texture\\");

        const std::string dataRoot = getImportDataRoot();
        if (!dataRoot.empty())
        {
            std::string root = dataRoot;
            for (char& c : root)
                if (c == '/')
                    c = '\\';
            if (!root.empty() && root.back() != '\\')
                root += '\\';
            candidates.push_back(root + "exported\\texture\\");
        }

#ifdef _WIN32
        char tmp[MAX_PATH];
        if (GetTempPathA(MAX_PATH, tmp) > 0)
            candidates.push_back(std::string(tmp) + "SwgMayaEditor\\tga\\");
#endif

        const size_t slash = ddsPath.find_last_of("/\\");
        if (slash != std::string::npos)
            candidates.push_back(ddsPath.substr(0, slash + 1));

        for (const std::string& raw : candidates)
        {
            if (!directoryIsWritable(raw))
                continue;
            return normalizeDir(raw);
        }
        return std::string();
    }

    static std::string resolveTexturePath(const std::string& treeFilePath, const std::string& inputFilename)
    {
        if (treeFilePath.empty())
            return std::string();

        std::string path = treeFilePath;
        for (size_t i = 0; i < path.size(); ++i)
            if (path[i] == '\\') path[i] = '/';

        if (path.find_last_of('.') == std::string::npos || path.find_last_of('.') < path.find_last_of("/\\"))
            path += ".dds";

        const std::string resolved = resolveTreeAssetPath(path, inputFilename, "texture/");
        return resolved.empty() ? path : resolved;
    }

    const Tag TAG_CSHD  = TAG(C,S,H,D);
    const Tag TAG_SSHT  = TAG(S,S,H,T);
    const Tag TAG_MATS  = TAG(M,A,T,S);
    const Tag TAG_MATL  = TAG(M,A,T,L);
    const Tag TAG_TXMS  = TAG(T,X,M,S);
    const Tag TAG_TXM   = TAG3(T,X,M);
    const Tag TAG_TCSS  = TAG(T,C,S,S);
    const Tag TAG_TFNS  = TAG(T,F,N,S);
    const Tag TAG_ARVS  = TAG(A,R,V,S);
    const Tag TAG_SRVS  = TAG(S,R,V,S);
    const Tag TAG_TXTR  = TAG(T,X,T,R);
    const Tag TAG_CUST  = TAG(C,U,S,T);
    const Tag TAG_TX1D  = TAG(T,X,1,D);
    const Tag TAG_TFAC  = TAG(T,F,A,C);
    const Tag TAG_PAL   = TAG3(P,A,L);
    const Tag TAG_TAG   = TAG3(T,A,G);
    const Tag TAG_MAIN  = TAG(M,A,I,N);
    const Tag TAG_NIAM  = TAG(N,I,A,M); // on-disk fourcc of MAIN when read as BE
    const Tag TAG_ENVM  = TAG(E,N,V,M);
    const Tag TAG_MASK  = TAG(M,A,S,K);
    const Tag TAG_CNRM  = TAG(C,N,R,M);
    const Tag TAG_NRML  = TAG(N,R,M,L);
    const Tag TAG_SPEC  = TAG(S,P,E,C);
    const Tag TAG_DOT3  = TAG(D,O,T,3);

    struct TextureData
    {
        Tag tag;
        std::string path;
        bool placeholder;
    };

    static uint32 swapTagBytes(uint32 t)
    {
        return ((t & 0xff000000u) >> 24) | ((t & 0x00ff0000u) >> 8) |
               ((t & 0x0000ff00u) << 8) | ((t & 0x000000ffu) << 24);
    }

    static bool tagIs(Tag got, Tag want)
    {
        // Iff::read_uint32 is native LE; some tools compare the raw fourcc (NIAM vs MAIN).
        return got == want || got == static_cast<Tag>(swapTagBytes(static_cast<uint32>(want)));
    }

    static bool isAlbedoSlot(Tag t)
    {
        return tagIs(t, TAG_MAIN) || tagIs(t, TAG_NIAM);
    }

    static bool isNonColorSlot(Tag t)
    {
        // Player CSHD order is CNRM, MAIN, SPEC — never bind the first TXM as color.
        return tagIs(t, TAG_ENVM) || tagIs(t, TAG_MASK) || tagIs(t, TAG_CNRM)
            || tagIs(t, TAG_NRML) || tagIs(t, TAG_SPEC) || tagIs(t, TAG_DOT3);
    }

    static const int kMaxIffBlocks = 4096;

    static bool skipCurrentIffBlock(Iff& iff)
    {
        if (iff.atEndOfForm())
            return false;
        return iff.goForward(1, true);
    }

    static void consumeRemainingBlocks(Iff& iff)
    {
        int guard = 0;
        while (iff.getNumberOfBlocksLeft() > 0 && guard++ < kMaxIffBlocks)
        {
            if (!skipCurrentIffBlock(iff))
                break;
        }
    }

    static bool readTxm(Iff& iff, TextureData& tex)
    {
        // Mirror clientGraphics StaticShaderTemplate::load_texture_0000/0001/0002.
        tex.tag = 0;
        tex.path.clear();
        tex.placeholder = false;
        if (!iff.enterForm(TAG_TXM, true))
            return false;
        const Tag txmVer = iff.getCurrentName();
        if (txmVer != TAG_0000 && txmVer != TAG_0001 && txmVer != TAG_0002)
        {
            shaderImportLog("skipping TXM version %08x", static_cast<unsigned>(txmVer));
            iff.exitForm(TAG_TXM, true);
            return false;
        }
        iff.enterForm(txmVer);

        iff.enterChunk(TAG_DATA);
        if (txmVer == TAG_0000)
        {
            tex.placeholder = iff.read_uint8() != 0;
            tex.tag = iff.read_uint32();
        }
        else
        {
            tex.tag = iff.read_uint32();
            tex.placeholder = iff.read_uint8() != 0;
            while (iff.getChunkLengthLeft(1) > 0)
                iff.read_uint8();
        }
        if (tagIs(tex.tag, TAG_ENVM))
            tex.placeholder = true;
        iff.exitChunk(TAG_DATA, true);

        if (!iff.atEndOfForm() && !iff.isCurrentForm() && iff.getCurrentName() == TAG_NAME)
        {
            iff.enterChunk(TAG_NAME);
            tex.path = iff.read_stdstring();
            iff.exitChunk(TAG_NAME, true);
        }

        iff.exitForm(txmVer, true);
        iff.exitForm(TAG_TXM);
        return !tex.path.empty();
    }

    static void readStaticShader(
        Iff& iff,
        std::vector<TextureData>& textures,
        std::string& effectName)
    {
        iff.enterForm(TAG_SSHT);
        const Tag sshtVer = iff.getCurrentName();
        if (sshtVer != TAG_0000 && sshtVer != TAG_0001)
        {
            shaderImportLog("unsupported SSHT version %08x", static_cast<unsigned>(sshtVer));
            iff.exitForm(TAG_SSHT, true);
            return;
        }
        iff.enterForm(sshtVer);

        int sshtGuard = 0;
        while (iff.getNumberOfBlocksLeft() > 0 && sshtGuard++ < kMaxIffBlocks)
        {
            if (iff.isCurrentForm())
            {
                const Tag formTag = iff.getCurrentName();

                if (formTag == TAG_MATS)
                {
                    iff.enterForm(TAG_MATS);
                    iff.enterForm(TAG_0000);
                    int matGuard = 0;
                    while (iff.getNumberOfBlocksLeft() > 0 && matGuard++ < 256)
                    {
                        const int blocksBefore = iff.getNumberOfBlocksLeft();
                        const Tag chunkTag = iff.getCurrentName();
                        if (!iff.enterChunk(true))
                        {
                            skipCurrentIffBlock(iff);
                            continue;
                        }
                        if (chunkTag == TAG_TAG)
                            iff.read_uint32();
                        else if (chunkTag == TAG_MATL)
                        {
                            for (int i = 0; i < 17 && iff.getChunkLengthLeft(4) > 0; ++i)
                                iff.read_float();
                        }
                        iff.exitChunk(true);
                        if (iff.getNumberOfBlocksLeft() >= blocksBefore)
                            skipCurrentIffBlock(iff);
                    }
                    iff.exitForm(TAG_0000, true);
                    iff.exitForm(TAG_MATS, true);
                }
                else if (formTag == TAG_TXMS)
                {
                    iff.enterForm(TAG_TXMS);
                    int txmGuard = 0;
                    while (!iff.atEndOfForm() && txmGuard++ < 256)
                    {
                        if (iff.isCurrentForm() && iff.getCurrentName() == TAG_TXM)
                        {
                            TextureData tex;
                            if (readTxm(iff, tex))
                                textures.push_back(tex);
                            else if (!skipCurrentIffBlock(iff))
                                break;
                        }
                        else if (!skipCurrentIffBlock(iff))
                            break;
                    }
                    iff.exitForm(TAG_TXMS, true);
                }
                else if (formTag == TAG_TCSS)
                {
                    iff.enterForm(TAG_TCSS);
                    iff.enterChunk(TAG_0000);
                    while (iff.getChunkLengthLeft(1) >= 5)
                    {
                        iff.read_uint32();
                        iff.read_uint8();
                    }
                    iff.exitChunk(TAG_0000);
                    iff.exitForm(TAG_TCSS);
                }
                else if (formTag == TAG_TFNS)
                {
                    iff.enterForm(TAG_TFNS);
                    iff.enterChunk(TAG_0000);
                    while (iff.getChunkLengthLeft(1) >= 8)
                    {
                        iff.read_uint32();
                        iff.read_uint32();
                    }
                    iff.exitChunk(TAG_0000);
                    iff.exitForm(TAG_TFNS);
                }
                else if (formTag == TAG_ARVS)
                {
                    iff.enterForm(TAG_ARVS);
                    iff.enterChunk(TAG_0000);
                    while (iff.getChunkLengthLeft(1) >= 5)
                    {
                        iff.read_uint32();
                        iff.read_uint8();
                    }
                    iff.exitChunk(TAG_0000);
                    iff.exitForm(TAG_ARVS);
                }
                else if (formTag == TAG_SRVS)
                {
                    iff.enterForm(TAG_SRVS);
                    iff.enterChunk(TAG_0000);
                    while (iff.getChunkLengthLeft(1) >= 8)
                    {
                        iff.read_uint32();
                        iff.read_uint32();
                    }
                    iff.exitChunk(TAG_0000);
                    iff.exitForm(TAG_SRVS);
                }
                else
                {
                    skipCurrentIffBlock(iff);
                }
            }
            else
            {
                const Tag chunkTag = iff.getCurrentName();

                if (chunkTag == TAG_NAME)
                {
                    iff.enterChunk(TAG_NAME);
                    effectName = iff.read_stdstring();
                    iff.exitChunk(TAG_NAME, true);
                }
                else
                {
                    skipCurrentIffBlock(iff);
                }
            }
        }

        iff.exitForm(sshtVer, true);
        iff.exitForm(TAG_SSHT);
    }
}

void* ImportShader::creator()
{
    return new ImportShader();
}

MStatus ImportShader::doIt(const MArgList& args)
{
    MStatus status;
    std::string filename;

    const unsigned argCount = args.length(&status);
    if (!status) return MS::kFailure;
    if (argCount < 2)
    {
        std::cerr << "ImportShader: usage importShader -i <filename> [-materialBase <name>]" << std::endl;
        return MS::kFailure;
    }

    std::string materialBaseOverride;
    for (unsigned i = 0; i < argCount; ++i)
    {
        MString arg = args.asString(i, &status);
        if (!status) return MS::kFailure;
        if (arg == "-i" && (i + 1) < argCount)
        {
            filename = args.asString(i + 1, &status).asChar();
            ++i;
        }
        else if (arg == MString("-materialBase") && (i + 1) < argCount)
        {
            materialBaseOverride = args.asString(i + 1, &status).asChar();
            ++i;
        }
    }

    if (filename.empty())
    {
        std::cerr << "ImportShader: no filename specified, use -i <filename>" << std::endl;
        return MS::kFailure;
    }

    MString sgName;
    const MStatus importStatus = importFromPath(filename, materialBaseOverride, sgName);
    if (importStatus)
        setResult(sgName);
    return importStatus;
}

MStatus ImportShader::importFromPath(
    const std::string& inFilename,
    const std::string& materialBaseOverride,
    MString& outShadingGroupName,
    MObject* outShadingGroup)
{
    outShadingGroupName.clear();
    std::string filename = inFilename;
    filename = resolveImportPath(filename);
    shaderImportLog("Opening: %s", filename.c_str());

    Iff iff;
    if (!iff.open(filename.c_str(), false))
    {
        std::cerr << "ImportShader: failed to open " << filename << std::endl;
        shaderImportLog("FAILED to open shader file");
        return MS::kFailure;
    }

    std::vector<TextureData> textures;
    std::string effectName;

    const Tag topTag = iff.getCurrentName();

    if (topTag == TAG_CSHD)
    {
        iff.enterForm(TAG_CSHD);
        const Tag cshdVer = iff.getCurrentName();
        if (cshdVer != TAG_0000 && cshdVer != TAG_0001)
        {
            std::cerr << "ImportShader: unsupported CSHD version" << std::endl;
            return MS::kFailure;
        }
        iff.enterForm(cshdVer);

        readStaticShader(iff, textures, effectName);

        if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_TXTR)
        {
            iff.enterForm(TAG_TXTR);
            if (iff.getNumberOfBlocksLeft() > 0 && !iff.isCurrentForm() && iff.getCurrentName() == TAG_DATA)
            {
                iff.enterChunk(TAG_DATA);
                const int16 texCount = iff.read_int16();
                for (int16 ti = 0; ti < texCount; ++ti)
                    iff.read_stdstring();
                iff.exitChunk(TAG_DATA);
            }
            if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_CUST)
            {
                iff.enterForm(TAG_CUST);
                consumeRemainingBlocks(iff);
                iff.exitForm(TAG_CUST, true);
            }
            consumeRemainingBlocks(iff);
            iff.exitForm(TAG_TXTR, true);
        }

        if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_TFAC)
        {
            iff.enterForm(TAG_TFAC);
            consumeRemainingBlocks(iff);
            iff.exitForm(TAG_TFAC, true);
        }

        consumeRemainingBlocks(iff);

        iff.exitForm(cshdVer, true);
        iff.exitForm(TAG_CSHD);
    }
    else if (topTag == TAG_SSHT)
    {
        readStaticShader(iff, textures, effectName);
    }
    else
    {
        std::cerr << "ImportShader: unrecognized top-level form, expected SSHT or CSHD" << std::endl;
        return MS::kFailure;
    }

    std::string mainTexturePath;
    for (size_t ti = 0; ti < textures.size(); ++ti)
    {
        shaderImportLog("TXM[%zu] %s placeholder=%d path=%s", ti,
            tagToFourcc(textures[ti].tag).c_str(),
            textures[ti].placeholder ? 1 : 0,
            textures[ti].path.empty() ? "(none)" : textures[ti].path.c_str());
        if (textures[ti].placeholder || textures[ti].path.empty())
            continue;
        if (isAlbedoSlot(textures[ti].tag))
        {
            mainTexturePath = textures[ti].path;
            break;
        }
    }
    if (mainTexturePath.empty())
    {
        for (size_t ti = 0; ti < textures.size(); ++ti)
        {
            if (textures[ti].placeholder || textures[ti].path.empty())
                continue;
            if (isNonColorSlot(textures[ti].tag))
                continue;
            mainTexturePath = textures[ti].path;
            break;
        }
    }

    if (textures.empty())
        shaderImportLog("No textures in shader");
    else
        shaderImportLog("Shader has %zu texture(s), MAIN/albedo: %s", textures.size(),
            mainTexturePath.empty() ? "(empty — refused CNRM/SPEC/ENVM fallback)" : mainTexturePath.c_str());

    std::string textureSlots;
    for (size_t ti = 0; ti < textures.size(); ++ti)
    {
        if (!textureSlots.empty())
            textureSlots += ';';
        textureSlots += tagToFourcc(textures[ti].tag);
        textureSlots += '=';
        std::string slotTree = toGameTreeRelativePath(textures[ti].path);
        if (slotTree.empty())
        {
            slotTree = textures[ti].path;
            for (char& c : slotTree)
                if (c == '\\') c = '/';
            if (slotTree.find("texture/") != 0)
                slotTree = std::string("texture/") + slotTree;
        }
        textureSlots += slotTree;
    }

    if (!mainTexturePath.empty())
        mainTexturePath = resolveTexturePath(mainTexturePath, filename);

    if (mainTexturePath.empty() && !textures.empty())
        shaderImportLog("WARNING: could not resolve main texture; searched compiled/game and TITAN_DATA_ROOT");

    std::string texturePathForMaterial = mainTexturePath;
    if (!mainTexturePath.empty())
    {
        const std::string outputDir = writableTgaOutputDir(mainTexturePath);
        bool flattenAlpha = true;
        {
            std::string hint = filename;
            hint += " ";
            hint += mainTexturePath;
            for (char& c : hint)
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c + ('a' - 'A'));
            if (hint.find("hair") != std::string::npos || hint.find("alpha") != std::string::npos)
                flattenAlpha = false;
        }
        const std::string safe = DdsToTgaConverter::mayaSafeImagePath(mainTexturePath, outputDir, flattenAlpha);
        if (!safe.empty())
            texturePathForMaterial = safe;
        else
            shaderImportLog("mayaSafeImagePath failed for %s", mainTexturePath.c_str());
        if (texturePathForMaterial.size() >= 4)
        {
            std::string ext = texturePathForMaterial.substr(texturePathForMaterial.size() - 4);
            for (char& c : ext)
                if (c >= 'A' && c <= 'Z')
                    c = static_cast<char>(c + ('a' - 'A'));
            if (ext == ".dds")
                shaderImportLog("WARNING: still a DDS (%s) — Maya VP2 cannot sample DXT, mesh will look white",
                    texturePathForMaterial.c_str());
        }
    }

    shaderImportLog("mainTexturePath: %s", mainTexturePath.empty() ? "(empty)" : mainTexturePath.c_str());

    std::string shaderName;
    if (!materialBaseOverride.empty())
        shaderName = materialBaseOverride;
    else
    {
        shaderName = filename;
        const size_t lastSlash = shaderName.find_last_of("\\/");
        if (lastSlash != std::string::npos)
            shaderName = shaderName.substr(lastSlash + 1);
        const size_t dotPos = shaderName.find_last_of('.');
        if (dotPos != std::string::npos)
            shaderName = shaderName.substr(0, dotPos);
    }

    std::string sanitizedName;
    for (size_t i = 0; i < shaderName.size(); ++i)
    {
        char c = shaderName[i];
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
            sanitizedName += c;
        else
            sanitizedName += '_';
    }
    if (!sanitizedName.empty() && sanitizedName[0] >= '0' && sanitizedName[0] <= '9')
        sanitizedName = "_" + sanitizedName;
    if (sanitizedName.empty())
        sanitizedName = "material";

    std::string shaderTreeRel = toGameTreeRelativePath(filename);
    if (shaderTreeRel.empty())
        shaderTreeRel = filename;
    std::string textureTreeRel = toGameTreeRelativePath(mainTexturePath);
    if (textureTreeRel.empty())
        textureTreeRel = mainTexturePath;

    std::string effectTreeRel = effectName;
    if (!effectTreeRel.empty())
    {
        std::string normalizedEffect = toGameTreeRelativePath(effectTreeRel);
        if (normalizedEffect.empty())
        {
            for (char& c : effectTreeRel)
                if (c == '\\') c = '/';
            if (effectTreeRel.find("effect/") != 0)
                effectTreeRel = std::string("effect/") + effectTreeRel;
        }
        else
            effectTreeRel = normalizedEffect;
    }

    MObject shadingGroup;
    MStatus status = MayaSceneBuilder::createMaterial(sanitizedName, texturePathForMaterial, shadingGroup,
        shaderTreeRel, textureTreeRel, effectTreeRel, textureSlots);
    if (!status)
    {
        std::cerr << "ImportShader: failed to create material " << sanitizedName << std::endl;
        shaderImportLog("createMaterial FAILED for %s", sanitizedName.c_str());
        return status;
    }
    MFnDependencyNode fn(shadingGroup);
    outShadingGroupName = fn.absoluteName();
    if (outShadingGroupName.length() == 0)
        outShadingGroupName = fn.name();
    if (outShadingGroup)
        *outShadingGroup = shadingGroup;
    shaderImportLog("createMaterial OK: %s -> %s tex=%s", sanitizedName.c_str(), outShadingGroupName.asChar(),
        texturePathForMaterial.empty() ? "(none)" : texturePathForMaterial.c_str());

    return MS::kSuccess;
}
