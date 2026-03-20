#ifndef SWGMAYAEDITOR_TGATODDSCONVERTER_H
#define SWGMAYAEDITOR_TGATODDSCONVERTER_H

#include <string>

/**
 * Converts TGA to DDS using nvtt_export.exe (NVIDIA Texture Tools).
 * Used on export to convert edited TGA textures back to DDS for the game.
 * Requires nvttExporterPath in SwgMayaEditor.cfg, e.g.:
 *   nvttExporterPath = "D:\\Program Files\\NVIDIA Corporation\\NVIDIA Texture Tools\\nvtt_export.exe"
 */
class TgaToDdsConverter
{
public:
    /// Converts TGA to DDS. Returns the DDS path on success, empty string on failure.
    /// Output path defaults to same dir as input with .dds extension.
    /// Format: bc7 (default), bc3, bc1, etc. Quality: normal (1), production (2), highest (3).
    static std::string convertToDds(const std::string& tgaPath,
        const std::string& outputPath = std::string(),
        const std::string& format = "bc7",
        int quality = 1);

    /// Returns the configured nvtt_export.exe path, or default install location.
    static std::string getNvttExporterPath();
};

#endif
