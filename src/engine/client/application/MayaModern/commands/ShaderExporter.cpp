#include "ShaderExporter.h"
#include "TgaToDdsConverter.h"
#include "SetDirectoryCommand.h"
#include "ImportPathResolver.h"
#include "MayaUtility.h"
#include "Iff.h"
#include "Tag.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    const Tag TAG_CSHD = TAG(C,S,H,D);
    const Tag TAG_SSHT = TAG(S,S,H,T);
    const Tag TAG_0000 = TAG(0,0,0,0);
    const Tag TAG_0001 = TAG(0,0,0,1);
    const Tag TAG_NAME = TAG(N,A,M,E);
    const Tag TAG_DATA = TAG(D,A,T,A);
    const Tag TAG_TXMS = TAG(T,X,M,S);
    const Tag TAG_TXM = TAG3(T,X,M);

    static std::string getBaseNameNoExt(const std::string& path)
    {
        std::string base = path;
        size_t slash = base.find_last_of("/\\");
        if (slash != std::string::npos)
            base = base.substr(slash + 1);
        size_t dot = base.find_last_of('.');
        if (dot != std::string::npos)
            base = base.substr(0, dot);
        return base;
    }

    static std::string ensureTrailingSlash(const std::string& s)
    {
        if (s.empty()) return s;
        char c = s.back();
        if (c != '/' && c != '\\') return s + '\\';
        return s;
    }

    /// Convert texture path: if TGA exists in textureWriteDir, convert to DDS.
    /// Returns tree path for shader (e.g. "texture/foo/bar" or "texture/bar" after conversion).
    static std::string processTexturePath(const std::string& treePath)
    {
        const char* textureWriteDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::TEXTURE_WRITE_DIR_INDEX);
        if (!textureWriteDir || !textureWriteDir[0])
            return treePath;

        std::string baseName = getBaseNameNoExt(treePath);
        if (baseName.empty())
            return treePath;

        std::string texDir = ensureTrailingSlash(textureWriteDir);
        std::string tgaPath = texDir + baseName + ".tga";

#ifdef _WIN32
        FILE* f = fopen(tgaPath.c_str(), "rb");
#else
        FILE* f = fopen(tgaPath.c_str(), "rb");
#endif
        if (!f)
            return treePath;
        fclose(f);

        std::string ddsOutputPath = texDir + baseName + ".dds";
        std::string converted = TgaToDdsConverter::convertToDds(tgaPath, ddsOutputPath);
        if (converted.empty())
            return treePath;

        return "texture/" + baseName;
    }

    static void copyIffRecursive(Iff& src, Iff& dest, bool insideTxm)
    {
        while (src.getNumberOfBlocksLeft() > 0)
        {
            if (src.isCurrentForm())
            {
                Tag formTag = src.getCurrentName();
                src.enterForm(formTag);
                dest.insertForm(formTag);

                bool newInsideTxm = insideTxm || (formTag == TAG_TXM);

                copyIffRecursive(src, dest, newInsideTxm);

                dest.exitForm(formTag);
                src.exitForm(formTag);
            }
            else
            {
                Tag chunkTag = src.getCurrentName();
                src.enterChunk(chunkTag);

                if (chunkTag == TAG_NAME && insideTxm)
                {
                    std::string path = src.read_stdstring();
                    path = processTexturePath(path);
                    dest.insertChunk(TAG_NAME);
                    dest.insertChunkString(path.c_str());
                    dest.exitChunk(TAG_NAME);
                }
                else
                {
                    const int len = src.getChunkLengthTotal(1);
                    if (len > 0)
                    {
                        std::vector<char> buf(static_cast<size_t>(len));
                        src.read_char(len, buf.data());
                        dest.insertChunk(chunkTag);
                        dest.insertChunkData(buf.data(), len);
                        dest.exitChunk(chunkTag);
                    }
                }

                src.exitChunk(chunkTag);
            }
        }
    }

    static void copyIffWithContext(Iff& src, Iff& dest)
    {
        copyIffRecursive(src, dest, false);
    }
}

std::string ShaderExporter::exportShader(const std::string& sourceShaderPath)
{
    if (sourceShaderPath.empty())
        return std::string();

    std::string resolvedPath = resolveImportPath(sourceShaderPath);
    for (size_t i = 0; i < resolvedPath.size(); ++i)
        if (resolvedPath[i] == '\\') resolvedPath[i] = '/';

    if (resolvedPath.size() < 4 || resolvedPath.substr(resolvedPath.size() - 4) != ".sht")
        resolvedPath += ".sht";

    Iff src;
    if (!src.open(resolvedPath.c_str(), false))
    {
        std::cerr << "[ShaderExporter] Failed to open: " << resolvedPath << "\n";
        return std::string();
    }

    const char* shaderWriteDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::SHADER_TEMPLATE_WRITE_DIR_INDEX);
    if (!shaderWriteDir || !shaderWriteDir[0])
    {
        std::cerr << "[ShaderExporter] shaderTemplateWriteDir not configured\n";
        return std::string();
    }

    std::string relPath = sourceShaderPath;
    for (size_t i = 0; i < relPath.size(); ++i)
        if (relPath[i] == '\\') relPath[i] = '/';
    if (relPath.size() >= 7 && (relPath.compare(0, 7, "shader/") == 0 || relPath.compare(0, 7, "shader\\") == 0))
        relPath = relPath.substr(7);
    size_t dot = relPath.find_last_of('.');
    if (dot != std::string::npos)
        relPath = relPath.substr(0, dot);
    if (relPath.empty())
        relPath = "shader";
    std::string outputPath = ensureTrailingSlash(shaderWriteDir) + relPath + ".sht";

    size_t lastSlash = outputPath.find_last_of("/\\");
    if (lastSlash != std::string::npos)
    {
        std::string outDir = outputPath.substr(0, lastSlash);
        MayaUtility::createDirectory(outDir.c_str());
    }

    Iff dest(65536, true);
    Tag topTag = src.getCurrentName();

    if (topTag == TAG_SSHT)
    {
        src.enterForm(TAG_SSHT);
        dest.insertForm(TAG_SSHT);
        Tag verTag = src.getCurrentName();
        src.enterForm(verTag);
        dest.insertForm(verTag);
        copyIffWithContext(src, dest);
        dest.exitForm(verTag);
        src.exitForm(verTag);
        dest.exitForm(TAG_SSHT);
        src.exitForm(TAG_SSHT);
    }
    else if (topTag == TAG_CSHD)
    {
        src.enterForm(TAG_CSHD);
        dest.insertForm(TAG_CSHD);
        Tag verTag = src.getCurrentName();
        src.enterForm(verTag);
        dest.insertForm(verTag);
        copyIffWithContext(src, dest);
        dest.exitForm(verTag);
        src.exitForm(verTag);
        dest.exitForm(TAG_CSHD);
        src.exitForm(TAG_CSHD);
    }
    else
    {
        std::cerr << "[ShaderExporter] Unknown shader format: " << topTag << "\n";
        return std::string();
    }

    src.close();

    if (!dest.write(outputPath.c_str(), true))
    {
        std::cerr << "[ShaderExporter] Failed to write: " << outputPath << "\n";
        return std::string();
    }

    return outputPath;
}
