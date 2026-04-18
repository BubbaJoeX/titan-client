#ifndef SWGMAYAEDITOR_TGATODDSCONVERTER_H
#define SWGMAYAEDITOR_TGATODDSCONVERTER_H

#include <string>

/**
 * Builds game DDS under textureWriteDir via nvtt_export.exe (PNG/JPG/TGA/etc. → BC3 DDS).
 * There is no intermediate .tga written to disk unless you enable textureMirrorSourceBesideDds in cfg.
 * If the source file is already .dds, copies it to the output name (passthrough).
 * Requires nvttExporterPath in SwgMayaEditor.cfg when converting from raster formats, e.g.:
 *   nvttExporterPath = "D:\\Program Files\\NVIDIA Corporation\\NVIDIA Texture Tools\\nvtt_export.exe"
 */
class TgaToDdsConverter
{
public:
    /// Converts TGA to DDS. Returns the DDS path on success, empty string on failure.
    /// Output path defaults to same dir as input with .dds extension.
    /// Format: bc3/DXT5 (default — Swg client loads DXT1/DXT3/DXT5 only, not BC7). bc1, bc7, etc. Quality: 1..3.
    static std::string convertToDds(const std::string& tgaPath,
        const std::string& outputPath = std::string(),
        const std::string& format = "bc3",
        int quality = 1);

    /// Returns the configured nvtt_export.exe path, or default install location.
    static std::string getNvttExporterPath();
};

#endif
