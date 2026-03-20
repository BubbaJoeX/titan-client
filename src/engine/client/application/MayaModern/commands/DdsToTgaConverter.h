#ifndef SWGMAYAEDITOR_DDSTOTGACONVERTER_H
#define SWGMAYAEDITOR_DDSTOTGACONVERTER_H

#include <string>

class DdsToTgaConverter
{
public:
    /// Converts a DDS file to TGA. Returns the TGA path on success, empty string on failure.
    /// By default writes TGA next to the DDS. If outputDir is non-empty, writes there instead
    /// (preserving relative path for editable round-trip, e.g. texture/foo.tga).
    /// Non-DDS or unsupported DDS formats return empty string (caller should use original path).
    static std::string convertToTga(const std::string& ddsPath, const std::string& outputDir = std::string());
};

#endif
