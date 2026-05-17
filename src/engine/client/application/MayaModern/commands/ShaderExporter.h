#ifndef SWGMAYAEDITOR_SHADEREXPORTER_H
#define SWGMAYAEDITOR_SHADEREXPORTER_H

#include <string>
#include <vector>

#include "Tag.h"

/**
 * Exports shader templates (.sht) with image->DDS conversion for edited textures.
 * Reads source .sht, converts textures in textureWriteDir (or absolute paths in NAME chunks) to DDS,
 * and writes the modified shader to shaderTemplateWriteDir.
 */
class ShaderExporter
{
public:
    /// Exports shader at sourcePath (tree path e.g. shader/foo/bar) to shaderTemplateWriteDir.
    /// Converts TGA/PNG/etc. textures in textureWriteDir to DDS. Returns output path on success.
    static std::string exportShader(const std::string& sourceShaderPath);

    /// Converts an on-disk image (TGA/PNG/JPG/BMP) to DDS under textureWriteDir and returns tree path "texture/<base>.dds" (TextureList/TreeFile open this path).
    static std::string publishDiffuseTextureForGame(const std::string& absoluteSourceImagePath, const std::string& treeBaseNameNoExt);

    /// Normalizes a virtual texture path for shader TXM NAME chunks (texture/foo.dds). The client loads DDS by this exact string.
    static std::string ensureTextureTreePathForTxmName(const std::string& path);

    /// Clones a prototype .sht into shaderTemplateWriteDir at outputShaderTreeRel.
    /// Prototype: prototypeShtPathOverride if non-empty; else SwgMayaEditor.cfg / env (see below).
    /// When hueable: TITAN_SHADER_PROTOTYPE_HUEABLE_SHT, then shaderPrototypeHueableSht, then same as non-hueable.
    /// Non-hueable: TITAN_SHADER_PROTOTYPE_SHT, shaderPrototypeSht, shader/defaultshader.sht.
    /// Transparent (alpha blend): optional TITAN_SHADER_PROTOTYPE_TRANSPARENT_SHT / shaderPrototypeTransparentSht (clone that .sht as-is).
    /// If unset, clones the normal opaque prototype and enables alpha blending on PASS/DATA so the viewer sorts/blends like Maya.
    /// If diffuseTextureTreePathNoExt is non-empty (e.g. "texture/myasset_d.dds"), replaces TXM NAME chunk(s); if empty, copies prototype textures unchanged.
    /// When bindPublishedDiffuseToAllTxmSlots is true (static mesh bake), every TXM NAME gets the published path so the viewer does not keep a default slot.
    /// When effectPathOverride is non-null and non-empty, any non-TXM NAME chunk in the prototype whose string ends in ".eft" is rewritten to this path
    /// (e.g. Maya "soe_effectName" / "soe_effect" on the surface shader — normalized to effect/foo.eft).
    static std::string exportShaderClonedFromPrototype(
        const std::string& outputShaderTreeRel,
        const std::string& prototypeShtPathOverride,
        const std::string& diffuseTextureTreePathNoExt,
        bool hueable = false,
        bool bindPublishedDiffuseToAllTxmSlots = false,
        bool transparent = false,
        const std::string* effectPathOverride = nullptr);

    /// Writes a SwitchTextureShaderTemplate (.sht, TAG_SWTS) for frame-animated diffuse textures (terminals, etc.).
    /// \a baseShaderTreeRel is the static shader (SSHT) path without .sht, e.g. "shader/foo_base" or "foo_base".
    /// \a outputShaderTreeRel is the outer SWTS path, e.g. "shader/foo".
    /// \a switcherFormTag is one of DRTS (loop), DPPT (ping-pong), DRFT (random frame/time).
    /// \a frameTextureTreePathsDds entries are full tree paths like "texture/frame01.dds" (same order as frames).
    /// \a fpsMin / fpsMax match legacy soe_animatingFps* (frames per second); timing uses 1/fps like the original exporter.
    static std::string exportSwitchTextureAnimatedShader(
        const std::string& outputShaderTreeRel,
        const std::string& baseShaderTreeRel,
        Tag switcherFormTag,
        float fpsMin,
        float fpsMax,
        const std::vector<std::string>& frameTextureTreePathsDds);
};

#endif
