// ======================================================================
//
// DdsToTgaConverter.h
// Converts DDS textures to TGA for Maya import (Maya 8 does not display DDS correctly).
//
// ======================================================================

#ifndef INCLUDED_DdsToTgaConverter_H
#define INCLUDED_DdsToTgaConverter_H

#include <string>

// ======================================================================

class DdsToTgaConverter
{
public:
	/// Converts a DDS file to TGA. Returns the TGA path on success, empty string on failure.
	/// The TGA is written next to the DDS with the same base name.
	/// Non-DDS or unsupported DDS formats return empty string (caller should use original path).
	static std::string convertToTga(const std::string &ddsPath);
};

// ======================================================================

#endif
