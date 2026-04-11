#include "SwgTrtsIo.h"

#include "Iff.h"
#include "Tag.h"

#include <maya/MDagPath.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MPlug.h>
#include <maya/MString.h>

#include <iomanip>
#include <sstream>

namespace
{
    const Tag TAG_TRTS = TAG(T, R, T, S);
    const Tag TAG_TRT = TAG3(T, R, T);
}

namespace SwgTrtsIo
{
    bool tryConsumeOptionalTrtsForm(Iff& iff, std::vector<Header>& outAppend, std::string& errMsg)
    {
        errMsg.clear();
        if (!iff.enterForm(TAG_TRTS, true))
            return true;
        return consumeTrtsFromEnteredForm(iff, outAppend, errMsg);
    }

    bool consumeTrtsFromEnteredForm(Iff& iff, std::vector<Header>& outAppend, std::string& errMsg)
    {
        errMsg.clear();
        iff.enterChunk(TAG_INFO);
        const int headerCount = iff.read_int32();
        const int entryCount = iff.read_int32();
        iff.exitChunk(TAG_INFO);

        int parsedEntries = 0;
        for (int i = 0; i < headerCount; ++i)
        {
            iff.enterChunk(TAG_TRT);
            char nameBuf[2048];
            iff.read_string(nameBuf, sizeof(nameBuf) - 1);
            const int affectedShaderCount = iff.read_int32();

            Header h;
            h.templateName = nameBuf;
            h.entries.reserve(static_cast<size_t>(affectedShaderCount));
            for (int j = 0; j < affectedShaderCount; ++j)
            {
                Entry e;
                e.shaderIndex = iff.read_int32();
                e.textureTag = iff.read_uint32();
                h.entries.push_back(e);
                ++parsedEntries;
            }
            iff.exitChunk(TAG_TRT);
            outAppend.push_back(std::move(h));
        }

        iff.exitForm(TAG_TRTS);

        if (parsedEntries != entryCount)
        {
            errMsg = "TRTS INFO entryCount does not match parsed TRT entries";
            return false;
        }
        return true;
    }

    MStatus applyBindingsToTransform(const MDagPath& transformPath, const std::vector<Header>& headers,
        const char* sourceFileComment)
    {
        if (headers.empty())
            return MS::kSuccess;

        MObject node = transformPath.node();
        MFnDependencyNode fn(node);
        const MString attrName("swgTrtBindings");
        if (!fn.hasAttribute(attrName))
        {
            MFnTypedAttribute tAttr;
            MObject attrObj = tAttr.create(attrName, "swgTRT", MFnData::kString);
            tAttr.setKeyable(false);
            tAttr.setStorable(true);
            fn.addAttribute(attrObj);
        }

        std::ostringstream oss;
        if (sourceFileComment && sourceFileComment[0] != '\0')
            oss << "# " << sourceFileComment << '\n';
        for (const Header& h : headers)
        {
            for (const Entry& e : h.entries)
            {
                oss << h.templateName << '\t' << e.shaderIndex << '\t' << "0x" << std::hex << std::setfill('0')
                    << std::setw(8) << e.textureTag << std::dec << std::setfill(' ') << '\n';
            }
        }

        MPlug plug = fn.findPlug(attrName, true);
        if (plug.isNull())
            return MS::kFailure;
        plug.setString(MString(oss.str().c_str()));
        return MS::kSuccess;
    }
}
