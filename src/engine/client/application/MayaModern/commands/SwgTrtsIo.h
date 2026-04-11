#pragma once

#include <maya/MTypes.h>

#include <cstdint>
#include <string>
#include <vector>

class Iff;
class MDagPath;

namespace SwgTrtsIo
{
    struct Entry
    {
        int32_t shaderIndex = 0;
        uint32_t textureTag = 0;
    };

    struct Header
    {
        std::string templateName;
        std::vector<Entry> entries;
    };

    /** FORM TRTS optional at current position (uses enterForm(TRTS, true)). On success, cursor is after TRTS. */
    bool tryConsumeOptionalTrtsForm(Iff& iff, std::vector<Header>& outAppend, std::string& errMsg);

    /** Already inside FORM TRTS (after enterForm(TRTS)). Parses contents and exits TRTS. */
    bool consumeTrtsFromEnteredForm(Iff& iff, std::vector<Header>& outAppend, std::string& errMsg);

    MStatus applyBindingsToTransform(const MDagPath& transformPath, const std::vector<Header>& headers,
        const char* sourceFileComment);
}
