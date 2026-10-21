#include "ImportAnimation.h"
#include "ImportPathResolver.h"
#include "ans.h"

#include <maya/MArgList.h>
#include <maya/MFileIO.h>
#include <maya/MGlobal.h>
#include <maya/MAnimControl.h>
#include <maya/MTime.h>

#include <string>

void* ImportAnimation::creator()
{
    return new ImportAnimation();
}

MStatus ImportAnimation::doIt(const MArgList& args)
{
    MStatus status;
    std::string filename;

    const unsigned argCount = args.length(&status);
    if (!status) return MS::kFailure;

    for (unsigned i = 0; i < argCount; ++i)
    {
        MString argName = args.asString(i, &status);
        if (!status) return MS::kFailure;

        if (argName == "-i")
        {
            MString argValue = args.asString(i + 1, &status);
            if (!status)
            {
                std::cerr << "ImportAnimation: missing filename after -i" << std::endl;
                return MS::kFailure;
            }
            filename = argValue.asChar();
            ++i;
            break;
        }
    }

    if (filename.empty())
    {
        std::cerr << "ImportAnimation: missing -i <filename> argument" << std::endl;
        return MS::kFailure;
    }

    filename = resolveImportPath(filename);

    status = MFileIO::import(filename.c_str(), "SwgAns", true);
    if (!status)
    {
        std::cerr << "ImportAnimation: failed to import " << filename << std::endl;
        return MS::kFailure;
    }

    // Maya's import restores the scene's time unit and playback range: switch to the clip's frame rate
    // (updating keys, so each stays at its time in seconds and lands on a whole clip frame) and frame the clip.
    float fps = 0.0f;
    int frameCount = 0;
    if (AnsTranslator::takeLastImportTiming(fps, frameCount))
    {
        const char* unit = nullptr;
        if (fps > 14.5f && fps < 15.5f) unit = "game";
        else if (fps > 23.5f && fps < 24.5f) unit = "film";
        else if (fps > 24.5f && fps < 25.5f) unit = "pal";
        else if (fps > 29.5f && fps < 30.5f) unit = "ntsc";
        else if (fps > 47.5f && fps < 48.5f) unit = "show";
        else if (fps > 49.5f && fps < 50.5f) unit = "palf";
        else if (fps > 59.5f && fps < 60.5f) unit = "ntscf";
        if (unit)
            MGlobal::executeCommand(MString("currentUnit -time ") + unit);   // keys keep their time in seconds
        else
            std::cerr << "ImportAnimation: no Maya time unit for " << fps << " fps; keeping the scene's" << std::endl;
        const MTime start(0.0, MTime::kSeconds);
        const MTime end(static_cast<double>(frameCount) / static_cast<double>(fps > 0.0f ? fps : 30.0f), MTime::kSeconds);
        MAnimControl::setAnimationStartEndTime(start, end);
        MAnimControl::setMinMaxTime(start, end);
        MAnimControl::setCurrentTime(start);
    }

    return MS::kSuccess;
}
