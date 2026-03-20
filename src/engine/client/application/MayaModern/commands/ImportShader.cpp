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

    static std::string resolveTexturePath(const std::string& treeFilePath, const std::string& inputFilename)
    {
        if (treeFilePath.empty())
            return std::string();

        std::string normalizedInput = inputFilename;
        for (size_t i = 0; i < normalizedInput.size(); ++i)
            if (normalizedInput[i] == '\\') normalizedInput[i] = '/';

        std::string baseDir;

        const char* envExportRoot = getenv("TITAN_EXPORT_ROOT");
        if (envExportRoot && envExportRoot[0])
        {
            baseDir = envExportRoot;
            if (!baseDir.empty() && baseDir.back() != '/' && baseDir.back() != '\\')
                baseDir += '/';
        }
        else
        {
            const char* envDataRoot = getenv("TITAN_DATA_ROOT");
            if (envDataRoot && envDataRoot[0])
            {
                baseDir = envDataRoot;
                if (!baseDir.empty() && baseDir.back() != '/' && baseDir.back() != '\\')
                    baseDir += '/';
            }
            else
            {
                size_t cgPos = normalizedInput.find("compiled/game/");
                if (cgPos != std::string::npos)
                    baseDir = normalizedInput.substr(0, cgPos + 14);
            }
        }

        if (baseDir.empty())
            return treeFilePath;

        std::string path = treeFilePath;
        for (size_t i = 0; i < path.size(); ++i)
            if (path[i] == '\\') path[i] = '/';

        if (path.find("texture/") != 0 && path.find("texture\\") != 0)
            path = std::string("texture/") + path;

        if (path.find_last_of('.') == std::string::npos || path.find_last_of('.') < path.find_last_of("/\\"))
            path += ".dds";

        std::string resolved = baseDir + path;
        for (size_t i = 0; i < resolved.size(); ++i)
            if (resolved[i] == '\\') resolved[i] = '/';
        return resolved;
    }

    const Tag TAG_CSHD  = TAG(C,S,H,D);
    const Tag TAG_SSHT  = TAG(S,S,H,T);
    const Tag TAG_0000  = TAG(0,0,0,0);
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

    struct TextureData
    {
        Tag tag;
        std::string path;
    };

    static void readStaticShader(
        Iff& iff,
        std::vector<TextureData>& textures,
        std::string& effectName)
    {
        iff.enterForm(TAG_SSHT);
        iff.enterForm(TAG_0000);

        while (iff.getNumberOfBlocksLeft() > 0)
        {
            if (iff.isCurrentForm())
            {
                const Tag formTag = iff.getCurrentName();

                if (formTag == TAG_MATS)
                {
                    iff.enterForm(TAG_MATS);
                    iff.enterForm(TAG_0000);
                    while (iff.getNumberOfBlocksLeft() > 0)
                    {
                        const Tag chunkTag = iff.getCurrentName();
                        iff.enterChunk(chunkTag);
                        if (chunkTag == TAG_TAG)
                            iff.read_uint32();
                        else if (chunkTag == TAG_MATL)
                        {
                            for (int i = 0; i < 17; ++i)
                                iff.read_float();
                        }
                        else
                        {
                            while (iff.getChunkLengthLeft(1) > 0)
                                iff.read_uint8();
                        }
                        iff.exitChunk(chunkTag);
                    }
                    iff.exitForm(TAG_0000);
                    iff.exitForm(TAG_MATS);
                }
                else if (formTag == TAG_TXMS)
                {
                    iff.enterForm(TAG_TXMS);

                    while (iff.getNumberOfBlocksLeft() > 0)
                    {
                        if (iff.isCurrentForm() && iff.getCurrentName() == TAG_TXM)
                        {
                            iff.enterForm(TAG_TXM);
                            iff.enterForm(TAG_0001);

                            TextureData tex;

                            iff.enterChunk(TAG_DATA);
                            tex.tag = iff.read_uint32();
                            iff.read_uint8();
                            iff.read_uint8();
                            iff.read_uint8();
                            iff.read_uint8();
                            iff.read_uint8();
                            iff.read_uint8();
                            iff.read_uint8();
                            iff.exitChunk(TAG_DATA);

                            iff.enterChunk(TAG_NAME);
                            tex.path = iff.read_stdstring();
                            iff.exitChunk(TAG_NAME);

                            textures.push_back(tex);

                            iff.exitForm(TAG_0001);
                            iff.exitForm(TAG_TXM);
                        }
                        else
                        {
                            if (iff.isCurrentForm())
                            {
                                iff.enterForm();
                                iff.exitForm();
                            }
                            else
                            {
                                iff.enterChunk();
                                iff.exitChunk();
                            }
                        }
                    }

                    iff.exitForm(TAG_TXMS);
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
                    iff.enterForm();
                    iff.exitForm();
                }
            }
            else
            {
                const Tag chunkTag = iff.getCurrentName();

                if (chunkTag == TAG_NAME)
                {
                    iff.enterChunk(TAG_NAME);
                    effectName = iff.read_stdstring();
                    iff.exitChunk(TAG_NAME);
                }
                else
                {
                    iff.enterChunk();
                    iff.exitChunk();
                }
            }
        }

        iff.exitForm(TAG_0000);
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
        std::cerr << "ImportShader: usage importShader -i <filename>" << std::endl;
        return MS::kFailure;
    }

    for (unsigned i = 0; i < argCount; ++i)
    {
        MString arg = args.asString(i, &status);
        if (!status) return MS::kFailure;
        if (arg == "-i" && (i + 1) < argCount)
        {
            filename = args.asString(i + 1, &status).asChar();
            ++i;
        }
    }

    if (filename.empty())
    {
        std::cerr << "ImportShader: no filename specified, use -i <filename>" << std::endl;
        return MS::kFailure;
    }

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
        iff.enterForm(TAG_0001);

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
                while (iff.getNumberOfBlocksLeft() > 0)
                {
                    if (!iff.isCurrentForm() && iff.getCurrentName() == TAG_TX1D)
                    {
                        iff.enterChunk(TAG_TX1D);
                        iff.read_uint32();
                        iff.read_int16();
                        iff.read_int16();
                        iff.read_stdstring();
                        iff.read_int8();
                        iff.read_int16();
                        iff.exitChunk(TAG_TX1D);
                    }
                    else if (iff.isCurrentForm())
                    {
                        iff.enterForm();
                        iff.exitForm();
                    }
                    else
                    {
                        iff.enterChunk();
                        iff.exitChunk();
                    }
                }
                iff.exitForm(TAG_CUST);
            }
            while (iff.getNumberOfBlocksLeft() > 0)
            {
                if (iff.isCurrentForm())
                {
                    iff.enterForm();
                    iff.exitForm();
                }
                else
                {
                    iff.enterChunk();
                    iff.exitChunk();
                }
            }
            iff.exitForm(TAG_TXTR);
        }

        if (iff.getNumberOfBlocksLeft() > 0 && iff.isCurrentForm() && iff.getCurrentName() == TAG_TFAC)
        {
            iff.enterForm(TAG_TFAC);
            while (iff.getNumberOfBlocksLeft() > 0)
            {
                if (!iff.isCurrentForm() && iff.getCurrentName() == TAG_PAL)
                {
                    iff.enterChunk(TAG_PAL);
                    iff.read_stdstring();
                    iff.read_int8();
                    iff.read_uint32();
                    iff.read_stdstring();
                    iff.read_int32();
                    iff.exitChunk(TAG_PAL);
                }
                else if (iff.isCurrentForm())
                {
                    iff.enterForm();
                    iff.exitForm();
                }
                else
                {
                    iff.enterChunk();
                    iff.exitChunk();
                }
            }
            iff.exitForm(TAG_TFAC);
        }

        while (iff.getNumberOfBlocksLeft() > 0)
        {
            if (iff.isCurrentForm())
            {
                iff.enterForm();
                iff.exitForm();
            }
            else
            {
                iff.enterChunk();
                iff.exitChunk();
            }
        }

        iff.exitForm(TAG_0001);
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
        if (textures[ti].tag == TAG_MAIN)
        {
            mainTexturePath = textures[ti].path;
            break;
        }
    }

    if (mainTexturePath.empty() && !textures.empty())
        mainTexturePath = textures[0].path;

    if (textures.empty())
        shaderImportLog("No textures in shader");
    else
        shaderImportLog("Shader has %zu texture(s), main raw path: %s", textures.size(), mainTexturePath.empty() ? "(empty)" : mainTexturePath.c_str());

    if (!mainTexturePath.empty())
        mainTexturePath = resolveTexturePath(mainTexturePath, filename);

    std::string texturePathForMaterial = mainTexturePath;
    if (!mainTexturePath.empty())
    {
        // D3D9/DXT5: Maya needs TGA - convert DDS to TGA and write to editable texture dir
        const char* textureWriteDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::TEXTURE_WRITE_DIR_INDEX);
        std::string outputDir;
        if (textureWriteDir && textureWriteDir[0])
        {
            outputDir = textureWriteDir;
            MayaUtility::createDirectory(outputDir.c_str());
        }
        const std::string tgaPath = DdsToTgaConverter::convertToTga(mainTexturePath, outputDir);
        if (!tgaPath.empty())
            texturePathForMaterial = tgaPath;
        else
        {
            std::cerr << "ImportShader: DDS->TGA failed for " << mainTexturePath << ", skipping texture\n";
            texturePathForMaterial.clear();
        }
    }

    shaderImportLog("mainTexturePath: %s", mainTexturePath.empty() ? "(empty)" : mainTexturePath.c_str());

    std::string shaderName = filename;
    {
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

    MObject shadingGroup;
    status = MayaSceneBuilder::createMaterial(sanitizedName, texturePathForMaterial, shadingGroup, filename, mainTexturePath);
    if (!status)
    {
        std::cerr << "ImportShader: failed to create material " << sanitizedName << std::endl;
        shaderImportLog("createMaterial FAILED for %s", sanitizedName.c_str());
        return status;
    }
    MFnDependencyNode fn(shadingGroup);
    MString actualSgName = fn.name();
    setResult(actualSgName);
    shaderImportLog("createMaterial OK: %s -> %s (DDS input, converted to TGA for Maya)", sanitizedName.c_str(), actualSgName.asChar());

    return MS::kSuccess;
}
