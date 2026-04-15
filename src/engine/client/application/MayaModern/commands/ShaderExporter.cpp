#include "ShaderExporter.h"
#include "TgaToDdsConverter.h"
#include "SetDirectoryCommand.h"
#include "ImportPathResolver.h"
#include "MayaUtility.h"
#include "ConfigFile.h"
#include "Iff.h"
#include "Tag.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{
    const Tag TAG_CSHD = TAG(C,S,H,D);
    const Tag TAG_SSHT = TAG(S,S,H,T);
    const Tag TAG_NAME = TAG(N,A,M,E);
    const Tag TAG_TXMS = TAG(T,X,M,S);
    const Tag TAG_TXM = TAG3(T,X,M);

    struct CopyTxmOpts
    {
        /// Game tree path from publishDiffuseTextureForGame, e.g. texture/foo_d.dds
        std::string publishedDiffuseTreePath;
        /// If true, every TXM NAME is set to publishedDiffuseTreePath (static mesh export).
        bool replaceEveryTxmSlot = false;
        int txmPublishedApplyCount = 0;
        bool didFirstTxmReplace = false;
        /// When not replaceEveryTxmSlot, clear placeholder only on first non-ENVM DATA chunk (pairs with didFirstTxmReplace).
        bool didPatchFirstDiffusePlaceholder = false;
    };

    static uint32 readUint32BE(const unsigned char* p)
    {
        return (static_cast<uint32>(p[0]) << 24) | (static_cast<uint32>(p[1]) << 16) |
               (static_cast<uint32>(p[2]) << 8) | static_cast<uint32>(p[3]);
    }

    /// Client StaticShaderTemplate skips TextureList::fetch when DATA.placeholder is true — prototype .sht often
    /// leaves diffuse as placeholder; clear it when we publish a real diffuse path.
    static void patchTxmDataPlaceholderForPublish(unsigned char* buf, int len, Tag txmVersion, CopyTxmOpts* txmOpts)
    {
        if (!txmOpts || txmOpts->publishedDiffuseTreePath.empty()) return;
        const Tag TAG_ENVM = TAG(E,N,V,M);
        const bool patchAll = txmOpts->replaceEveryTxmSlot;

        if (txmVersion == TAG_0001 || txmVersion == TAG_0002)
        {
            if (len < 5) return;
            const uint32 tag = readUint32BE(buf);
            if (tag == TAG_ENVM) return;
            bool shouldPatch = patchAll || !txmOpts->didPatchFirstDiffusePlaceholder;
            if (shouldPatch && buf[4] != 0)
            {
                buf[4] = 0;
                if (!patchAll) txmOpts->didPatchFirstDiffusePlaceholder = true;
            }
        }
        else if (txmVersion == TAG_0000)
        {
            if (len < 5) return;
            const uint32 tag = readUint32BE(buf + 1);
            if (tag == TAG_ENVM) return;
            const bool shouldPatch = patchAll || !txmOpts->didPatchFirstDiffusePlaceholder;
            if (shouldPatch && buf[0] != 0)
            {
                buf[0] = 0;
                if (!patchAll) txmOpts->didPatchFirstDiffusePlaceholder = true;
            }
        }
    }

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

    static bool looksLikeFilesystemImagePath(const std::string& p)
    {
        if (p.empty()) return false;
        if (p.find(':') != std::string::npos) return true;
        if (p[0] == '/' || p[0] == '\\') return true;
        return false;
    }

    static bool isAbsoluteFilesystemPath(const std::string& p)
    {
        if (p.empty()) return false;
        if (p.size() >= 2 && p[1] == ':') return true;
        if (p[0] == '/' || p[0] == '\\') return true;
        return false;
    }

    static bool endsWithIgnoreCase(const std::string& s, const char* ext)
    {
        const size_t el = strlen(ext);
        if (s.size() < el) return false;
        for (size_t i = 0; i < el; ++i)
        {
            const char a = static_cast<char>(tolower(static_cast<unsigned char>(s[s.size() - el + i])));
            const char b = static_cast<char>(tolower(static_cast<unsigned char>(ext[i])));
            if (a != b) return false;
        }
        return true;
    }

    /// Normalizes virtual tree paths for TXM NAME: texture/<base>.dds (TreeFile opens this exact path).
    static std::string ensureTextureTreePathForTxmNameImpl(const std::string& path)
    {
        if (path.empty()) return path;
        if (looksLikeFilesystemImagePath(path)) return path;

        std::string s = path;
        for (char& c : s)
            if (c == '\\') c = '/';
        while (!s.empty() && s[0] == '/')
            s.erase(0, 1);

        static const char* exts[] = { ".dds", ".tga", ".png", ".jpg", ".jpeg", ".tif", ".tiff", ".bmp" };
        for (const char* ext : exts)
        {
            const size_t el = strlen(ext);
            if (s.size() >= el && endsWithIgnoreCase(s, ext))
            {
                s.resize(s.size() - el);
                break;
            }
        }

        static const char pref[] = "texture/";
        if (s.size() < 8 || s.compare(0, 8, pref) != 0)
            s = std::string(pref) + s;

        if (!endsWithIgnoreCase(s, ".dds"))
            s += ".dds";
        return s;
    }

    /// If path points at an image file on disk, convert to DDS in textureWriteDir and return "texture/<base>.dds".
    static std::string convertFilesystemImageToGameTexture(const std::string& absPath)
    {
        const char* textureWriteDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::TEXTURE_WRITE_DIR_INDEX);
        if (!textureWriteDir || !textureWriteDir[0])
            return absPath;

        std::string baseName = getBaseNameNoExt(absPath);
        if (baseName.empty())
            return absPath;

        std::string texDir = ensureTrailingSlash(textureWriteDir);
        std::string ddsOutputPath = texDir + baseName + ".dds";
        if (TgaToDdsConverter::convertToDds(absPath, ddsOutputPath).empty())
            return absPath;
        return std::string("texture/") + baseName + ".dds";
    }

    /// Convert texture path for shader NAME chunk: tree paths, optional absolute image paths, or textureWriteDir TGAs.
    static std::string processTexturePath(const std::string& treePath)
    {
        if (looksLikeFilesystemImagePath(treePath))
            return convertFilesystemImageToGameTexture(treePath);

        const char* textureWriteDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::TEXTURE_WRITE_DIR_INDEX);
        if (!textureWriteDir || !textureWriteDir[0])
            return ensureTextureTreePathForTxmNameImpl(treePath);

        std::string baseName = getBaseNameNoExt(treePath);
        if (baseName.empty())
            return ensureTextureTreePathForTxmNameImpl(treePath);

        std::string texDir = ensureTrailingSlash(textureWriteDir);
        std::string tgaPath = texDir + baseName + ".tga";

        FILE* f = fopen(tgaPath.c_str(), "rb");
        if (!f)
            return ensureTextureTreePathForTxmNameImpl(treePath);
        fclose(f);

        std::string ddsOutputPath = texDir + baseName + ".dds";
        std::string converted = TgaToDdsConverter::convertToDds(tgaPath, ddsOutputPath);
        if (converted.empty())
            return ensureTextureTreePathForTxmNameImpl(treePath);

        return std::string("texture/") + baseName + ".dds";
    }

    static void copyIffRecursive(Iff& src, Iff& dest, bool insideTxm, Tag txmInnerVersion, CopyTxmOpts* txmOpts)
    {
        while (src.getNumberOfBlocksLeft() > 0)
        {
            if (src.isCurrentForm())
            {
                Tag formTag = src.getCurrentName();
                src.enterForm(formTag);
                dest.insertForm(formTag);

                const bool newInsideTxm = insideTxm || (formTag == TAG_TXM);
                Tag childTxmVer = txmInnerVersion;
                if (insideTxm && (formTag == TAG_0000 || formTag == TAG_0001 || formTag == TAG_0002))
                    childTxmVer = formTag;

                copyIffRecursive(src, dest, newInsideTxm, childTxmVer, txmOpts);

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
                    bool fromMayaPublish = false;
                    if (txmOpts && !txmOpts->publishedDiffuseTreePath.empty())
                    {
                        if (txmOpts->replaceEveryTxmSlot)
                        {
                            path = txmOpts->publishedDiffuseTreePath;
                            fromMayaPublish = true;
                            ++txmOpts->txmPublishedApplyCount;
                        }
                        else if (!txmOpts->didFirstTxmReplace)
                        {
                            path = txmOpts->publishedDiffuseTreePath;
                            txmOpts->didFirstTxmReplace = true;
                            fromMayaPublish = true;
                            ++txmOpts->txmPublishedApplyCount;
                        }
                    }
                    // Do not run processTexturePath on Maya-published paths: it can re-convert a stale .tga
                    // in textureWriteDir and overwrite the .dds we just wrote from the export.
                    if (!fromMayaPublish)
                        path = processTexturePath(path);
                    else
                        path = ensureTextureTreePathForTxmNameImpl(path);
                    dest.insertChunk(TAG_NAME);
                    dest.insertChunkString(path.c_str());
                    dest.exitChunk(TAG_NAME);
                }
                else if (chunkTag == TAG_DATA && insideTxm && txmInnerVersion != 0 && txmOpts
                    && !txmOpts->publishedDiffuseTreePath.empty())
                {
                    const int len = src.getChunkLengthTotal(1);
                    dest.insertChunk(chunkTag);
                    if (len > 0)
                    {
                        std::vector<unsigned char> buf(static_cast<size_t>(len));
                        src.read_char(len, reinterpret_cast<char*>(buf.data()));
                        patchTxmDataPlaceholderForPublish(buf.data(), len, txmInnerVersion, txmOpts);
                        dest.insertChunkData(reinterpret_cast<const char*>(buf.data()), len);
                    }
                    dest.exitChunk(chunkTag);
                }
                else
                {
                    const int len = src.getChunkLengthTotal(1);
                    dest.insertChunk(chunkTag);
                    if (len > 0)
                    {
                        std::vector<char> buf(static_cast<size_t>(len));
                        src.read_char(len, buf.data());
                        dest.insertChunkData(buf.data(), len);
                    }
                    dest.exitChunk(chunkTag);
                }

                src.exitChunk(chunkTag);
            }
        }
    }

    static void copyIffWithContext(Iff& src, Iff& dest, CopyTxmOpts* txmOpts)
    {
        copyIffRecursive(src, dest, false, 0, txmOpts);
    }

    static std::string normalizeShaderRelPath(std::string relPath)
    {
        for (size_t i = 0; i < relPath.size(); ++i)
            if (relPath[i] == '\\') relPath[i] = '/';
        if (relPath.size() >= 7 && (relPath.compare(0, 7, "shader/") == 0 || relPath.compare(0, 7, "shader\\") == 0))
            relPath = relPath.substr(7);
        size_t dot = relPath.find_last_of('.');
        if (dot != std::string::npos)
            relPath = relPath.substr(0, dot);
        return relPath;
    }

    /// Open a shader .sht for reading: tries resolveImportPath (DATA_ROOT / export root), then
    /// shaderTemplateWriteDir + basename so prototypes can live next to exports when trees differ.
    static bool openShaderSourceIff(Iff& iff, const std::string& shaderTreeRelOrAbs, std::string& outTriedPrimary)
    {
        outTriedPrimary = resolveImportPath(shaderTreeRelOrAbs);
        for (size_t i = 0; i < outTriedPrimary.size(); ++i)
            if (outTriedPrimary[i] == '\\') outTriedPrimary[i] = '/';
        if (outTriedPrimary.size() < 4 || outTriedPrimary.substr(outTriedPrimary.size() - 4) != ".sht")
            outTriedPrimary += ".sht";

        if (iff.open(outTriedPrimary.c_str(), true))
            return true;

        if (isAbsoluteFilesystemPath(shaderTreeRelOrAbs))
            return false;

        const char* shaderWriteDir =
            SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::SHADER_TEMPLATE_WRITE_DIR_INDEX);
        if (!shaderWriteDir || !shaderWriteDir[0])
            return false;

        const std::string base = normalizeShaderRelPath(shaderTreeRelOrAbs);
        if (base.empty())
            return false;

        const std::string alt = ensureTrailingSlash(shaderWriteDir) + base + ".sht";
        if (iff.open(alt.c_str(), true))
        {
            std::cerr << "[ShaderExporter] Opened shader from shaderTemplateWriteDir: " << alt << "\n";
            return true;
        }

        return false;
    }

    static std::string resolvePrototypeShtPath(const std::string& userOverride, bool hueable)
    {
        if (!userOverride.empty())
            return userOverride;
        if (hueable)
        {
            const char* envH = getenv("TITAN_SHADER_PROTOTYPE_HUEABLE_SHT");
            if (envH && envH[0])
                return std::string(envH);
            const char* cfgH = ConfigFile::getKeyString("SwgMayaEditor", "shaderPrototypeHueableSht", "");
            if (cfgH && cfgH[0])
                return std::string(cfgH);
        }
        const char* env = getenv("TITAN_SHADER_PROTOTYPE_SHT");
        if (env && env[0])
            return std::string(env);
        const char* cfg = ConfigFile::getKeyString("SwgMayaEditor", "shaderPrototypeSht", "");
        if (cfg && cfg[0])
            return std::string(cfg);
        return std::string("shader/defaultshader.sht");
    }

    static std::string writeClonedShaderIff(Iff& src, const std::string& outputShaderTreeRel, CopyTxmOpts* txmOpts)
    {
        const char* shaderWriteDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::SHADER_TEMPLATE_WRITE_DIR_INDEX);
        if (!shaderWriteDir || !shaderWriteDir[0])
        {
            std::cerr << "[ShaderExporter] shaderTemplateWriteDir not configured\n";
            return std::string();
        }

        std::string relPath = normalizeShaderRelPath(outputShaderTreeRel);
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
            copyIffWithContext(src, dest, txmOpts);
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
            copyIffWithContext(src, dest, txmOpts);
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

        if (!dest.write(outputPath.c_str(), true))
        {
            std::cerr << "[ShaderExporter] Failed to write: " << outputPath << "\n";
            return std::string();
        }

        return outputPath;
    }
}

std::string ShaderExporter::exportShader(const std::string& sourceShaderPath)
{
    if (sourceShaderPath.empty())
        return std::string();

    Iff src;
    std::string triedPath;
    if (!openShaderSourceIff(src, sourceShaderPath, triedPath))
    {
        std::cerr << "[ShaderExporter] Failed to open: " << sourceShaderPath << " (tried " << triedPath
                  << " and shaderTemplateWriteDir)\n";
        return std::string();
    }

    std::string out = writeClonedShaderIff(src, sourceShaderPath, nullptr);
    src.close();
    return out;
}

std::string ShaderExporter::publishDiffuseTextureForGame(const std::string& absoluteSourceImagePath, const std::string& treeBaseNameNoExt)
{
    if (absoluteSourceImagePath.empty() || treeBaseNameNoExt.empty())
        return std::string();

    const char* textureWriteDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::TEXTURE_WRITE_DIR_INDEX);
    if (!textureWriteDir || !textureWriteDir[0])
    {
        std::cerr << "[ShaderExporter] textureWriteDir not set (setBaseDir)\n";
        return std::string();
    }

    std::string texDir(textureWriteDir);
    if (!texDir.empty() && texDir.back() != '/' && texDir.back() != '\\')
        texDir += '\\';
    std::string ddsOutputPath = texDir + treeBaseNameNoExt + ".dds";
    if (TgaToDdsConverter::convertToDds(absoluteSourceImagePath, ddsOutputPath).empty())
        return std::string();
    return std::string("texture/") + treeBaseNameNoExt + ".dds";
}

std::string ShaderExporter::ensureTextureTreePathForTxmName(const std::string& path)
{
    return ensureTextureTreePathForTxmNameImpl(path);
}

std::string ShaderExporter::exportShaderClonedFromPrototype(
    const std::string& outputShaderTreeRel,
    const std::string& prototypeShtPathOverride,
    const std::string& diffuseTextureTreePathNoExt,
    bool hueable,
    bool bindPublishedDiffuseToAllTxmSlots)
{
    if (outputShaderTreeRel.empty())
        return std::string();

    const std::string proto = resolvePrototypeShtPath(prototypeShtPathOverride, hueable);

    Iff src;
    std::string triedPath;
    if (!openShaderSourceIff(src, proto, triedPath))
    {
        std::cerr << "[ShaderExporter] Failed to open prototype shader (tried " << triedPath
                  << " then shaderTemplateWriteDir): prototype=" << proto << "\n";
        std::cerr << "[ShaderExporter] Place shader/defaultshader.sht under DATA_ROOT, set shaderPrototypeSht in SwgMayaEditor.cfg, "
                     "or copy a prototype .sht into shaderTemplateWriteDir as <basename>.sht\n";
        return std::string();
    }

    CopyTxmOpts opts;
    CopyTxmOpts* optsPtr = nullptr;
    if (!diffuseTextureTreePathNoExt.empty())
    {
        opts.publishedDiffuseTreePath = diffuseTextureTreePathNoExt;
        opts.replaceEveryTxmSlot = bindPublishedDiffuseToAllTxmSlots;
        optsPtr = &opts;
    }

    std::string out = writeClonedShaderIff(src, outputShaderTreeRel, optsPtr);
    src.close();

    if (!out.empty() && optsPtr && optsPtr->txmPublishedApplyCount == 0)
        std::cerr << "[ShaderExporter] Warning: prototype had no TXM NAME to replace; output may reference prototype textures\n";

    return out;
}
