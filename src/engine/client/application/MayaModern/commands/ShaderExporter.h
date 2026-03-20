#ifndef SWGMAYAEDITOR_SHADEREXPORTER_H
#define SWGMAYAEDITOR_SHADEREXPORTER_H

#include <string>

/**
 * Exports shader templates (.sht) with TGA->DDS conversion for edited textures.
 * Reads source .sht, converts any TGA textures in textureWriteDir to DDS,
 * and writes the modified shader to shaderTemplateWriteDir.
 */
class ShaderExporter
{
public:
    /// Exports shader at sourcePath (tree path e.g. shader/foo/bar) to shaderTemplateWriteDir.
    /// Converts TGA textures in textureWriteDir to DDS. Returns output path on success.
    static std::string exportShader(const std::string& sourceShaderPath);
};

#endif
