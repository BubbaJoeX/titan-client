#include "ShaderExporter.h"
#include "ExportDirectoryBootstrap.h"
#include "TgaToDdsConverter.h"
#include "SetDirectoryCommand.h"
#include "ImportPathResolver.h"
#include "MayaUtility.h"
#include "ConfigFile.h"
#include "Iff.h"
#include "Tag.h"

#include <maya/MGlobal.h>
#include <maya/MString.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static const Tag gSwtsTag = TAG(S,W,T,S);
static const Tag gSwtsTextTag = TAG(T,E,X,T);
static const Tag gSwtsMainTag = TAG(M,A,I,N);
static const Tag gSwtsDRTS = TAG(D,R,T,S);
static const Tag gSwtsDRFT = TAG(D,R,F,T);
static const Tag gSwtsDPPT = TAG(D,P,P,T);

namespace
{
    const Tag TAG_CSHD = TAG(C,S,H,D);
    const Tag TAG_SSHT = TAG(S,S,H,T);
    const Tag TAG_PASS = TAG(P,A,S,S);
    const Tag TAG_DATA = TAG(D,A,T,A);
    const Tag TAG_TXMS = TAG(T,X,M,S);
    const Tag TAG_TXM = TAG3(T,X,M);

    struct CopyTxmOpts
    {
        /// Game tree path from publishDiffuseTextureForGame, e.g. texture/foo_d.dds
        std::string publishedDiffuseTreePath;
        /// If true, every TXM NAME is set to publishedDiffuseTreePath (legacy; static mesh export leaves false).
        bool replaceEveryTxmSlot = false;
        int txmPublishedApplyCount = 0;
        /// Slot tag from the current TXM DATA chunk (MRNC, NIAM, MVNE, …).
        Tag currentTxmSlotTag = 0;
        /// When true, only MRNC gets the published diffuse (multi-slot env/spec prototypes). When false, NIAM only (defaultshader).
        bool prototypeHasMrncSlot = false;
    };

    static uint32 readUint32BE(const unsigned char* p)
    {
        return (static_cast<uint32>(p[0]) << 24) | (static_cast<uint32>(p[1]) << 16) |
               (static_cast<uint32>(p[2]) << 8) | static_cast<uint32>(p[3]);
    }

    static Tag readTxmDataSlotTag(const unsigned char* buf, int len, Tag txmVersion)
    {
        if (txmVersion == TAG_0001 || txmVersion == TAG_0002)
        {
            if (len < 4) return 0;
            return static_cast<Tag>(readUint32BE(buf));
        }
        if (txmVersion == TAG_0000)
        {
            if (len < 5) return 0;
            return static_cast<Tag>(readUint32BE(buf + 1));
        }
        return 0;
    }

    static bool shouldPublishDiffuseToCurrentTxm(const CopyTxmOpts& o)
    {
        if (o.replaceEveryTxmSlot) return true;
        if (o.publishedDiffuseTreePath.empty()) return false;
        const Tag TAG_MRNC = TAG(M,R,N,C);
        const Tag TAG_NIAM = TAG(N,I,A,M);
        if (o.prototypeHasMrncSlot)
            return o.currentTxmSlotTag == TAG_MRNC;
        if (o.currentTxmSlotTag == TAG_NIAM)
            return true;
        if (o.currentTxmSlotTag == 0)
            return o.txmPublishedApplyCount == 0;
        return false;
    }

    static bool shouldPatchTxmDataPlaceholderForSlot(Tag slotTag, const CopyTxmOpts& o)
    {
        if (o.publishedDiffuseTreePath.empty()) return false;
        const Tag TAG_ENVM = TAG(E,N,V,M);
        const Tag TAG_MRNC = TAG(M,R,N,C);
        const Tag TAG_NIAM = TAG(N,I,A,M);
        if (o.replaceEveryTxmSlot)
            return slotTag != TAG_ENVM;
        if (o.prototypeHasMrncSlot)
            return slotTag == TAG_MRNC;
        if (slotTag == TAG_NIAM)
            return true;
        if (slotTag == 0)
            return o.txmPublishedApplyCount == 0;
        return false;
    }

    /// Client StaticShaderTemplate skips TextureList::fetch when DATA.placeholder is true — prototype .sht often
    /// leaves diffuse as placeholder; clear it when we publish a real diffuse path.
    static void patchTxmDataPlaceholderForPublish(unsigned char* buf, int len, Tag txmVersion, CopyTxmOpts* txmOpts)
    {
        if (!txmOpts || txmOpts->publishedDiffuseTreePath.empty()) return;
        const Tag slotTag = readTxmDataSlotTag(buf, len, txmVersion);
        if (!shouldPatchTxmDataPlaceholderForSlot(slotTag, *txmOpts)) return;

        if (txmVersion == TAG_0001 || txmVersion == TAG_0002)
        {
            if (len < 5) return;
            if (buf[4] != 0) buf[4] = 0;
        }
        else if (txmVersion == TAG_0000)
        {
            if (len < 5) return;
            if (buf[0] != 0) buf[0] = 0;
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

    /// Byte offset of m_alphaBlendEnable in PASS/<ver>/DATA (ShaderImplementationPass::load_0000..0010).
    static int alphaBlendEnableByteOffsetInPassData(Tag passVer)
    {
        if (passVer == TAG_0000)
            return 6;
        if (passVer == TAG_0001)
            return 10;
        if (passVer == TAG_0002 || passVer == TAG_0003 || passVer == TAG_0004)
            return 8;
        if (passVer == TAG_0005 || passVer == TAG_0006 || passVer == TAG_0007 || passVer == TAG_0008 || passVer == TAG_0009)
            return 7;
        if (passVer == TAG_0010)
            return 8;
        return -1;
    }

    static void patchPassDataChunkAlphaBlendEnable(unsigned char* buf, int len, Tag passVer)
    {
        const int off = alphaBlendEnableByteOffsetInPassData(passVer);
        if (off < 0 || len <= off)
            return;
        buf[off] = 1;
    }

    static bool pathLooksLikeEffectReference(const std::string& path)
    {
        if (path.size() < 4)
            return false;
        size_t dot = path.rfind('.');
        if (dot == std::string::npos || dot + 1 >= path.size())
            return false;
        std::string ext = path.substr(dot);
        for (char& c : ext)
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        return ext == ".eft";
    }

    static void copyIffRecursive(
        Iff& src,
        Iff& dest,
        bool insideTxm,
        Tag txmInnerVersion,
        CopyTxmOpts* txmOpts,
        bool patchPassForTransparent,
        bool passNextChildIsVersionTag,
        Tag passVersionTag,
        const std::string* effectPathOverride)
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
                if (formTag == TAG_TXM && txmOpts)
                    txmOpts->currentTxmSlotTag = 0;
                if (insideTxm && (formTag == TAG_0000 || formTag == TAG_0001 || formTag == TAG_0002))
                    childTxmVer = formTag;

                Tag nextPassVer = passVersionTag;
                bool nextPassChild = passNextChildIsVersionTag;
                if (formTag == TAG_PASS)
                {
                    nextPassChild = true;
                    nextPassVer = 0;
                }
                else if (passNextChildIsVersionTag)
                {
                    nextPassVer = formTag;
                    nextPassChild = false;
                }

                copyIffRecursive(
                    src,
                    dest,
                    newInsideTxm,
                    childTxmVer,
                    txmOpts,
                    patchPassForTransparent,
                    nextPassChild,
                    nextPassVer,
                    effectPathOverride);

                dest.exitForm(formTag);
                src.exitForm(formTag);
            }
            else
            {
                Tag chunkTag = src.getCurrentName();
                src.enterChunk(chunkTag);

                if (chunkTag == TAG_NAME && !insideTxm && effectPathOverride && !effectPathOverride->empty())
                {
                    std::string path = src.read_stdstring();
                    if (pathLooksLikeEffectReference(path))
                        path = *effectPathOverride;
                    dest.insertChunk(TAG_NAME);
                    dest.insertChunkString(path.c_str());
                    dest.exitChunk(TAG_NAME);
                }
                else if (chunkTag == TAG_NAME && insideTxm)
                {
                    std::string path = src.read_stdstring();
                    bool fromMayaPublish = false;
                    if (txmOpts && !txmOpts->publishedDiffuseTreePath.empty())
                    {
                        if (shouldPublishDiffuseToCurrentTxm(*txmOpts))
                        {
                            path = txmOpts->publishedDiffuseTreePath;
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
                else if (chunkTag == TAG_DATA && insideTxm && txmInnerVersion != 0)
                {
                    const int len = src.getChunkLengthTotal(1);
                    dest.insertChunk(chunkTag);
                    if (len > 0)
                    {
                        std::vector<unsigned char> buf(static_cast<size_t>(len));
                        src.read_char(len, reinterpret_cast<char*>(buf.data()));
                        if (txmOpts)
                        {
                            txmOpts->currentTxmSlotTag = readTxmDataSlotTag(buf.data(), len, txmInnerVersion);
                            if (!txmOpts->publishedDiffuseTreePath.empty())
                                patchTxmDataPlaceholderForPublish(buf.data(), len, txmInnerVersion, txmOpts);
                        }
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
                        std::vector<unsigned char> buf(static_cast<size_t>(len));
                        src.read_char(len, reinterpret_cast<char*>(buf.data()));
                        if (chunkTag == TAG_DATA && patchPassForTransparent && passVersionTag != 0 && !insideTxm)
                            patchPassDataChunkAlphaBlendEnable(buf.data(), len, passVersionTag);
                        dest.insertChunkData(reinterpret_cast<const char*>(buf.data()), len);
                    }
                    dest.exitChunk(chunkTag);
                }

                src.exitChunk(chunkTag);
            }
        }
    }

    static void copyIffWithContext(
        Iff& src, Iff& dest, CopyTxmOpts* txmOpts, bool patchPassForTransparent, const std::string* effectPathOverride)
    {
        copyIffRecursive(src, dest, false, 0, txmOpts, patchPassForTransparent, false, 0, effectPathOverride);
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

    /// Open a shader .sht for reading: game data (TITAN_DATA_ROOT), import root, then shaderTemplateWriteDir.
    static bool openShaderSourceIff(Iff& iff, const std::string& shaderTreeRelOrAbs, std::string& outTriedPrimary)
    {
        auto tryOpen = [&](const std::string& absPath) -> bool {
            if (absPath.empty())
                return false;
            if (iff.open(absPath.c_str(), true))
            {
                outTriedPrimary = absPath;
                std::cerr << "[ShaderExporter] Opened shader: " << absPath << "\n";
                return true;
            }
            return false;
        };

        if (isAbsoluteFilesystemPath(shaderTreeRelOrAbs))
            return tryOpen(shaderTreeRelOrAbs);

        std::string rel = shaderTreeRelOrAbs;
        for (char& c : rel)
            if (c == '\\')
                c = '/';
        while (!rel.empty() && (rel[0] == '/' || rel[0] == '\\'))
            rel.erase(0, 1);

        const std::string base = normalizeShaderRelPath(rel);
        if (base.empty())
            return false;

        const std::string treeRel = std::string("shader/") + base + ".sht";

        std::vector<std::string> candidates;
        auto pushUnique = [&](const std::string& p) {
            if (p.empty())
                return;
            for (const std::string& existing : candidates)
                if (existing == p)
                    return;
            candidates.push_back(p);
        };

        pushUnique(resolveGameAssetPath(treeRel));
        pushUnique(resolveImportPath(treeRel));

        const char* shaderWriteDir =
            SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::SHADER_TEMPLATE_WRITE_DIR_INDEX);
        if (shaderWriteDir && shaderWriteDir[0])
            pushUnique(ensureTrailingSlash(shaderWriteDir) + base + ".sht");

        for (const std::string& candidate : candidates)
        {
            if (tryOpen(candidate))
                return true;
        }

        outTriedPrimary = candidates.empty() ? treeRel : candidates.back();
        std::cerr << "[ShaderExporter] Failed to open shader (tried " << candidates.size() << " paths), last: "
                  << outTriedPrimary << "\n";
        return false;
    }

    static std::string resolvePrototypeShtPath(const std::string& userOverride, bool hueable, bool transparent)
    {
        if (!userOverride.empty())
            return userOverride;
        if (transparent)
        {
            const char* envT = getenv("TITAN_SHADER_PROTOTYPE_TRANSPARENT_SHT");
            if (envT && envT[0])
                return std::string(envT);
            const char* cfgT = ConfigFile::getKeyString("SwgMayaEditor", "shaderPrototypeTransparentSht", "");
            if (cfgT && cfgT[0])
                return std::string(cfgT);
            std::cerr << "[ShaderExporter] Transparent export: no shaderPrototypeTransparentSht / "
                         "TITAN_SHADER_PROTOTYPE_TRANSPARENT_SHT — using opaque prototype with PASS alpha-blend patch.\n";
        }
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

    static void bootstrapDependenciesForWrittenShader(const std::string& absShaderPath);

    static bool scanIffForTxmDataSlotTag(
        Iff& iff, bool insideTxm, Tag txmInnerVersion, Tag searchTag)
    {
        while (iff.getNumberOfBlocksLeft() > 0)
        {
            if (iff.isCurrentForm())
            {
                const Tag formTag = iff.getCurrentName();
                iff.enterForm(formTag);
                Tag childTxmVer = txmInnerVersion;
                if (insideTxm && (formTag == TAG_0000 || formTag == TAG_0001 || formTag == TAG_0002))
                    childTxmVer = formTag;
                const bool found = scanIffForTxmDataSlotTag(
                    iff, insideTxm || formTag == TAG_TXM, childTxmVer, searchTag);
                iff.exitForm(formTag);
                if (found)
                    return true;
            }
            else
            {
                const Tag chunkTag = iff.getCurrentName();
                iff.enterChunk(chunkTag);
                const int len = iff.getChunkLengthTotal(1);
                if (chunkTag == TAG_DATA && insideTxm && txmInnerVersion != 0 && len > 0)
                {
                    std::vector<unsigned char> buf(static_cast<size_t>(len));
                    iff.read_char(len, reinterpret_cast<char*>(buf.data()));
                    if (readTxmDataSlotTag(buf.data(), len, txmInnerVersion) == searchTag)
                    {
                        iff.exitChunk(chunkTag);
                        return true;
                    }
                }
                else if (chunkTag == TAG_NAME)
                    (void)iff.read_stdstring();
                else if (len > 0)
                {
                    std::vector<unsigned char> skip(static_cast<size_t>(len));
                    iff.read_char(len, reinterpret_cast<char*>(skip.data()));
                }
                iff.exitChunk(chunkTag);
            }
        }
        return false;
    }

    static bool prototypeShaderHasMrncSlot(Iff& src)
    {
        const Tag topTag = src.getCurrentName();
        if (topTag != TAG_SSHT && topTag != TAG_CSHD)
            return false;
        src.enterForm(topTag);
        const Tag verTag = src.getCurrentName();
        src.enterForm(verTag);
        const bool hasMrnc = scanIffForTxmDataSlotTag(src, false, 0, TAG(M,R,N,C));
        src.exitForm(verTag);
        src.exitForm(topTag);
        return hasMrnc;
    }

    static std::string writeClonedShaderIff(
        Iff& src,
        const std::string& outputShaderTreeRel,
        CopyTxmOpts* txmOpts,
        bool patchPassForTransparent,
        const std::string* effectPathOverride)
    {
        const char* shaderWriteDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::SHADER_TEMPLATE_WRITE_DIR_INDEX);
        if (!shaderWriteDir || !shaderWriteDir[0])
        {
            std::cerr << "[ShaderExporter] shaderTemplateWriteDir not configured\n";
            MGlobal::displayError("[ShaderExporter] shaderTemplateWriteDir not set — run setBaseDir before export.");
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
            copyIffWithContext(src, dest, txmOpts, patchPassForTransparent, effectPathOverride);
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
            copyIffWithContext(src, dest, txmOpts, patchPassForTransparent, effectPathOverride);
            dest.exitForm(verTag);
            src.exitForm(verTag);
            dest.exitForm(TAG_CSHD);
            src.exitForm(TAG_CSHD);
        }
        else
        {
            std::cerr << "[ShaderExporter] Unknown shader format: " << topTag << "\n";
            MGlobal::displayError("[ShaderExporter] Unknown shader IFF format in prototype (expected SSHT or CSHD).");
            return std::string();
        }

    if (!dest.write(outputPath.c_str(), true))
    {
        std::cerr << "[ShaderExporter] Failed to write: " << outputPath << "\n";
        MGlobal::displayError(MString("[ShaderExporter] Failed to write shader file: ") + outputPath.c_str());
        return std::string();
    }

        std::cerr << "[ShaderExporter] Wrote shader: " << outputPath << "\n";
        bootstrapDependenciesForWrittenShader(outputPath);
        return outputPath;
    }

    static bool pathLooksLikeTextureTreeReference(const std::string& path)
    {
        if (path.size() < 5) return false;
        std::string s = path;
        for (char& c : s)
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        return s.find("texture/") == 0 || s.find("texture\\") == 0;
    }

    static void collectShaderTreeReferencesRecursive(
        Iff& iff, bool insideTxm, std::vector<std::string>& effects, std::vector<std::string>& textures)
    {
        while (iff.getNumberOfBlocksLeft() > 0)
        {
            if (iff.isCurrentForm())
            {
                const Tag formTag = iff.getCurrentName();
                iff.enterForm(formTag);
                collectShaderTreeReferencesRecursive(
                    iff, insideTxm || formTag == TAG_TXM, effects, textures);
                iff.exitForm(formTag);
            }
            else
            {
                const Tag chunkTag = iff.getCurrentName();
                iff.enterChunk(chunkTag);
                const int len = iff.getChunkLengthTotal(1);
                if (chunkTag == TAG_NAME)
                {
                    std::string path = iff.read_stdstring();
                    for (char& c : path)
                        if (c == '\\')
                            c = '/';
                    if (!insideTxm && pathLooksLikeEffectReference(path))
                        effects.push_back(path);
                    else if (pathLooksLikeTextureTreeReference(path))
                        textures.push_back(path);
                }
                else if (len > 0)
                {
                    std::vector<unsigned char> skip(static_cast<size_t>(len));
                    iff.read_char(len, reinterpret_cast<char*>(skip.data()));
                }
                iff.exitChunk(chunkTag);
            }
        }
    }

    static void bootstrapDependenciesForWrittenShader(const std::string& absShaderPath)
    {
        Iff iff;
        if (!iff.open(absShaderPath.c_str(), true))
            return;

        std::vector<std::string> effects;
        std::vector<std::string> textures;
        const Tag topTag = iff.getCurrentName();
        if (topTag == TAG_SSHT || topTag == TAG_CSHD)
        {
            iff.enterForm(topTag);
            const Tag verTag = iff.getCurrentName();
            iff.enterForm(verTag);
            collectShaderTreeReferencesRecursive(iff, false, effects, textures);
            iff.exitForm(verTag);
            iff.exitForm(topTag);
        }
        iff.close();

        for (const std::string& e : effects)
            ExportDirectoryBootstrap::bootstrapEffectTreeFile(e);
        for (const std::string& t : textures)
            ExportDirectoryBootstrap::bootstrapTextureTreeFile(t);
    }

    static std::string readEffectPathFromShaderFileImpl(const std::string& absShaderPath)
    {
        Iff iff;
        if (!iff.open(absShaderPath.c_str(), true))
            return std::string();

        std::vector<std::string> effects;
        std::vector<std::string> texturesUnused;
        const Tag topTag = iff.getCurrentName();
        if (topTag == TAG_SSHT || topTag == TAG_CSHD)
        {
            iff.enterForm(topTag);
            const Tag verTag = iff.getCurrentName();
            iff.enterForm(verTag);
            collectShaderTreeReferencesRecursive(iff, false, effects, texturesUnused);
            iff.exitForm(verTag);
            iff.exitForm(topTag);
        }
        iff.close();

        return effects.empty() ? std::string() : effects.front();
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

    std::string out = writeClonedShaderIff(src, sourceShaderPath, nullptr, false, nullptr);
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
        MGlobal::displayError("[ShaderExporter] textureWriteDir not set — run setBaseDir before export so textures publish to texture/");
        return std::string();
    }

    std::string texDir(textureWriteDir);
    if (!texDir.empty() && texDir.back() != '/' && texDir.back() != '\\')
        texDir += '\\';
    std::string ddsOutputPath = texDir + treeBaseNameNoExt + ".dds";
    const std::string converted = TgaToDdsConverter::convertToDds(absoluteSourceImagePath, ddsOutputPath);
    if (converted.empty())
    {
        MGlobal::displayError(MString("[ShaderExporter] Failed to publish DDS from ") + absoluteSourceImagePath.c_str() +
                               " — see Script Editor [TgaToDds] lines (nvtt path, image format).");
        return std::string();
    }

    // Optional: mirror source beside .dds so artists see what was converted (no separate .tga stage — PNG/JPG go straight to nvtt).
    const char* mirrorCfg = ConfigFile::getKeyString("SwgMayaEditor", "textureMirrorSourceBesideDds", "0");
    const bool mirror = mirrorCfg && mirrorCfg[0] && (mirrorCfg[0] == '1' || strcmp(mirrorCfg, "true") == 0 ||
                                                       strcmp(mirrorCfg, "yes") == 0 || strcmp(mirrorCfg, "on") == 0);
    if (mirror)
    {
        size_t dot = absoluteSourceImagePath.find_last_of('.');
        if (dot != std::string::npos && dot < absoluteSourceImagePath.size() - 1)
        {
            const std::string extWithDot = absoluteSourceImagePath.substr(dot);
            const std::string mirrorPath = texDir + treeBaseNameNoExt + "_src" + extWithDot;
            if (MayaUtility::copyFile(absoluteSourceImagePath, mirrorPath))
            {
                MGlobal::displayInfo(MString("[ShaderExporter] Mirrored source for inspection: ") + mirrorPath.c_str());
            }
        }
    }

    {
        const std::string rel = std::string("texture/") + treeBaseNameNoExt + ".dds";
        MString infoMsg("[ShaderExporter] Published ");
        infoMsg += rel.c_str();
        infoMsg += " from ";
        infoMsg += absoluteSourceImagePath.c_str();
        MGlobal::displayInfo(infoMsg);
    }
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
    bool bindPublishedDiffuseToAllTxmSlots,
    bool transparent,
    const std::string* effectPathOverride)
{
    if (outputShaderTreeRel.empty())
        return std::string();

    const std::string protoRequested = resolvePrototypeShtPath(prototypeShtPathOverride, hueable, transparent);
    std::string protoUsed = protoRequested;

    Iff src;
    std::string triedPath;
    if (!openShaderSourceIff(src, protoRequested, triedPath))
    {
        if (!prototypeShtPathOverride.empty())
        {
            MGlobal::displayWarning(MString("[ShaderExporter] Cannot open swgShaderPath prototype \"")
                + prototypeShtPathOverride.c_str() + "\" (tried " + triedPath.c_str()
                + "). Falling back to shader/defaultshader.sht.");
            protoUsed = "shader/defaultshader.sht";
            if (!openShaderSourceIff(src, protoUsed, triedPath))
            {
                MGlobal::displayError(MString("[ShaderExporter] Failed to open fallback shader/defaultshader.sht (tried ")
                    + triedPath.c_str() + "). Run setBaseDir and ensure TITAN_DATA_ROOT is set.");
                std::cerr << "[ShaderExporter] Failed to open prototype or fallback shader. Requested=" << protoRequested
                          << " tried=" << triedPath << "\n";
                return std::string();
            }
        }
        else
        {
            MGlobal::displayError(MString("[ShaderExporter] Failed to open prototype shader (tried ") + triedPath.c_str()
                + "). Set TITAN_DATA_ROOT, run setBaseDir, or set shaderPrototypeSht in SwgMayaEditor.cfg.");
            std::cerr << "[ShaderExporter] Failed to open prototype shader: " << protoRequested << " tried " << triedPath
                      << "\n";
            return std::string();
        }
    }

    if (effectPathOverride && !effectPathOverride->empty())
        ExportDirectoryBootstrap::bootstrapEffectTreeFile(*effectPathOverride);

    bool prototypeHasMrnc = false;
    {
        Iff scanIff;
        std::string scanPath;
        if (openShaderSourceIff(scanIff, protoUsed, scanPath))
            prototypeHasMrnc = prototypeShaderHasMrncSlot(scanIff);
        scanIff.close();
    }

    CopyTxmOpts opts;
    CopyTxmOpts* optsPtr = nullptr;
    if (!diffuseTextureTreePathNoExt.empty())
    {
        opts.publishedDiffuseTreePath = diffuseTextureTreePathNoExt;
        opts.replaceEveryTxmSlot = bindPublishedDiffuseToAllTxmSlots;
        opts.prototypeHasMrncSlot = prototypeHasMrnc;
        optsPtr = &opts;
    }

    std::string out = writeClonedShaderIff(src, outputShaderTreeRel, optsPtr, transparent, effectPathOverride);
    src.close();

    if (out.empty())
    {
        MGlobal::displayError(MString("[ShaderExporter] Failed to clone prototype shader to ") + outputShaderTreeRel.c_str()
            + " — see Script Editor for [ShaderExporter] lines (prototype: " + protoUsed.c_str() + ")");
    }
    else if (optsPtr && optsPtr->txmPublishedApplyCount == 0)
        std::cerr << "[ShaderExporter] Warning: prototype had no TXM NAME to replace; output may reference prototype textures\n";

    return out;
}

std::string ShaderExporter::exportSwitchTextureAnimatedShader(
    const std::string& outputShaderTreeRel,
    const std::string& baseShaderTreeRel,
    Tag switcherFormTag,
    float fpsMin,
    float fpsMax,
    const std::vector<std::string>& frameTextureTreePathsDds)
{
    if (outputShaderTreeRel.empty() || baseShaderTreeRel.empty() || frameTextureTreePathsDds.empty())
        return std::string();

    if (switcherFormTag != gSwtsDRTS && switcherFormTag != gSwtsDRFT && switcherFormTag != gSwtsDPPT)
    {
        std::cerr << "[ShaderExporter] exportSwitchTextureAnimatedShader: switcher tag must be DRTS, DRFT, or DPPT\n";
        return std::string();
    }

    const char* shaderWriteDir = SetDirectoryCommand::getDirectoryString(SetDirectoryCommand::SHADER_TEMPLATE_WRITE_DIR_INDEX);
    if (!shaderWriteDir || !shaderWriteDir[0])
    {
        std::cerr << "[ShaderExporter] shaderTemplateWriteDir not configured\n";
        MGlobal::displayError("[ShaderExporter] shaderTemplateWriteDir not set — run setBaseDir before animated shader export.");
        return std::string();
    }

    if (fpsMin <= 0.f)
        fpsMin = 8.f;
    if (fpsMax <= 0.f)
        fpsMax = fpsMin;
    if (fpsMax < fpsMin)
    {
        const float t = fpsMin;
        fpsMin = fpsMax;
        fpsMax = t;
    }

    const float timeMax = 1.f / fpsMin;
    const float timeMin = 1.f / fpsMax;
    const int frameCount = static_cast<int>(frameTextureTreePathsDds.size());

    std::string relOut = normalizeShaderRelPath(outputShaderTreeRel);
    if (relOut.empty())
        relOut = "shader";
    std::string outputPath = ensureTrailingSlash(shaderWriteDir) + relOut + ".sht";

    {
        size_t lastSlash = outputPath.find_last_of("/\\");
        if (lastSlash != std::string::npos)
        {
            std::string outDir = outputPath.substr(0, lastSlash);
            MayaUtility::createDirectory(outDir.c_str());
        }
    }

    std::string relBase = normalizeShaderRelPath(baseShaderTreeRel);
    if (relBase.empty())
    {
        std::cerr << "[ShaderExporter] exportSwitchTextureAnimatedShader: invalid base shader path\n";
        return std::string();
    }
    const std::string nameRefToBase = std::string("shader/") + relBase + ".sht";

    Iff dest(65536, true);
    dest.insertForm(gSwtsTag);
    dest.insertForm(TAG_0000);
    dest.insertChunk(TAG_NAME);
    dest.insertChunkString(nameRefToBase.c_str());
    dest.exitChunk(TAG_NAME);

    dest.insertForm(switcherFormTag);
    dest.insertChunk(TAG_0000);
    dest.insertChunkData(frameCount);
    dest.insertChunkData(timeMin);
    dest.insertChunkData(timeMax);
    dest.exitChunk(TAG_0000);
    dest.exitForm(switcherFormTag);

    for (const std::string& rawTp : frameTextureTreePathsDds)
    {
        const std::string tp = ensureTextureTreePathForTxmNameImpl(rawTp);
        dest.insertChunk(gSwtsTextTag);
        dest.insertChunkData(gSwtsMainTag);
        dest.insertChunkString(tp.c_str());
        dest.exitChunk(gSwtsTextTag);
    }

    dest.exitForm(TAG_0000);
    dest.exitForm(gSwtsTag);

    if (!dest.write(outputPath.c_str(), true))
    {
        std::cerr << "[ShaderExporter] Failed to write SWTS: " << outputPath << "\n";
        MGlobal::displayError(MString("[ShaderExporter] Failed to write animated shader: ") + outputPath.c_str());
        return std::string();
    }

    std::cerr << "[ShaderExporter] wrote SwitchTexture (animated) " << outputPath << " -> base " << nameRefToBase << " frames=" << frameCount
              << " fps=" << fpsMin << ".." << fpsMax << "\n";
    MGlobal::displayInfo(MString("[ShaderExporter] Animated .sht (SWTS): ") + outputPath.c_str());
    ShaderExporter::bootstrapDependenciesForWrittenShader(outputPath);
    return outputPath;
}

std::string ShaderExporter::effectPathFromWrittenShader(const std::string& absShaderPath)
{
    return readEffectPathFromShaderFileImpl(absShaderPath);
}

void ShaderExporter::bootstrapDependenciesForWrittenShader(const std::string& absShaderPath)
{
    ::bootstrapDependenciesForWrittenShader(absShaderPath);
}
