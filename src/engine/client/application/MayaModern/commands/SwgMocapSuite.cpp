#include "SwgMocapSuite.h"

#include <maya/MArgList.h>
#include <maya/MGlobal.h>
#include <maya/MStatus.h>
#include <maya/MString.h>

#include <fstream>
#include <string>
#include <vector>

namespace
{
    std::string toForwardSlashes(std::string s)
    {
        for (char& c : s)
            if (c == '\\')
                c = '/';
        return s;
    }

    bool fileExists(const std::string& path)
    {
        std::ifstream f(path.c_str());
        return f.good();
    }

    std::string dirOf(const std::string& filePath)
    {
        const std::string p = toForwardSlashes(filePath);
        const size_t slash = p.find_last_of('/');
        if (slash == std::string::npos)
            return std::string();
        return p.substr(0, slash + 1);
    }

    std::string findMocapMel()
    {
        std::vector<std::string> dirs;
        MString pluginPath;
        if (MGlobal::executeCommand("pluginInfo -q -path SwgMayaEditor", pluginPath) && pluginPath.length() > 0)
            dirs.push_back(dirOf(pluginPath.asChar()));

        dirs.push_back("C:/Program Files/Autodesk/Maya2027/bin/plug-ins/");
        dirs.push_back("D:/titan/client/src/engine/client/application/MayaModern/scripts/");

        MString scriptPath;
        if (MGlobal::executeCommand("getenv \"MAYA_SCRIPT_PATH\"", scriptPath) && scriptPath.length() > 0)
        {
            std::string raw = scriptPath.asChar();
            std::string cur;
            for (size_t i = 0; i <= raw.size(); ++i)
            {
                if (i == raw.size() || raw[i] == ';')
                {
                    if (!cur.empty())
                    {
                        cur = toForwardSlashes(cur);
                        if (cur.back() != '/')
                            cur += '/';
                        dirs.push_back(cur);
                    }
                    cur.clear();
                    continue;
                }
                cur += raw[i];
            }
        }

        for (const std::string& d : dirs)
        {
            if (d.empty())
                continue;
            const std::string mel = d + "swgMocapSuite.mel";
            if (fileExists(mel))
                return toForwardSlashes(mel);
        }
        return std::string();
    }
}

void* SwgMocapSuite::creator()
{
    return new SwgMocapSuite();
}

MStatus SwgMocapSuite::doIt(const MArgList&)
{
    const std::string melPath = findMocapMel();
    if (melPath.empty())
    {
        MGlobal::displayError(
            "swgMocapSuite: swgMocapSuite.mel not found. Expected next to the .mll or in Maya2027/bin/plug-ins.");
        return MS::kFailure;
    }

    MString cmd = "source \"";
    cmd += melPath.c_str();
    cmd += "\"; swgMocapSuiteShow;";
    const MStatus st = MGlobal::executeCommand(cmd, true);
    if (!st)
    {
        MGlobal::displayError(MString("swgMocapSuite: Failed to source ") + melPath.c_str());
        return st;
    }
    MGlobal::displayInfo(MString("swgMocapSuite: sourced ") + melPath.c_str());
    return MS::kSuccess;
}
