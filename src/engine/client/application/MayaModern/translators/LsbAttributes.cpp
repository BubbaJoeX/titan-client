#include "LsbAttributes.h"

#include <maya/MFnAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MPlug.h>
#include <maya/MString.h>

#include <algorithm>
#include <cstdio>
#include <sstream>

namespace
{
    const MString kCatCore("LSB / Core (BASE, ASND)");
    const MString kCatFlicker("LSB / LGHT Flicker");
    const MString kCatBladesSummary("LSB / Blades (summary)");

    static int plugInt(MFnDependencyNode& dep, const char* name, int defVal)
    {
        if (!dep.hasAttribute(name))
            return defVal;
        MStatus st;
        MPlug p = dep.findPlug(name, true, &st);
        if (!st || p.isNull())
            return defVal;
        int v = defVal;
        p.getValue(v);
        return v;
    }

    static double plugDouble(MFnDependencyNode& dep, const char* name, double defVal)
    {
        if (!dep.hasAttribute(name))
            return defVal;
        MStatus st;
        MPlug p = dep.findPlug(name, true, &st);
        if (!st || p.isNull())
            return defVal;
        double v = defVal;
        p.getValue(v);
        return v;
    }

    static std::string plugString(MFnDependencyNode& dep, const char* name)
    {
        if (!dep.hasAttribute(name))
            return {};
        MStatus st;
        MPlug p = dep.findPlug(name, true, &st);
        if (!st || p.isNull())
            return {};
        MString v;
        if (p.getValue(v) != MS::kSuccess)
            return {};
        return std::string(v.asChar());
    }

    static MStatus addStringAttr(MFnDependencyNode& dep, const char* longName, const char* briefName, const MString& niceName,
                                 const MString& category, const std::string& value)
    {
        MStatus st;
        if (!dep.hasAttribute(longName))
        {
            MFnTypedAttribute t;
            MObject a = t.create(longName, briefName, MFnData::kString, MObject::kNullObj, &st);
            if (!st)
                return st;
            t.setStorable(true);
            t.setKeyable(true);
            MFnAttribute fa(a);
            fa.setNiceNameOverride(niceName);
            st = fa.addToCategory(category);
            if (!st)
                return st;
            st = dep.addAttribute(a);
            if (!st)
                return st;
        }
        MPlug p = dep.findPlug(longName, true, &st);
        if (!st || p.isNull())
            return MS::kFailure;
        p.setValue(MString(value.c_str()));
        return MS::kSuccess;
    }

    static MStatus addLongAttr(MFnDependencyNode& dep, const char* longName, const char* briefName, const MString& niceName,
                               const MString& category, int defVal, int minV, int maxV)
    {
        MStatus st;
        if (!dep.hasAttribute(longName))
        {
            MFnNumericAttribute n;
            MObject a = n.create(longName, briefName, MFnNumericData::kLong, static_cast<double>(defVal), &st);
            if (!st)
                return st;
            n.setMin(static_cast<double>(minV));
            n.setMax(static_cast<double>(maxV));
            n.setStorable(true);
            n.setKeyable(true);
            MFnAttribute fa(a);
            fa.setNiceNameOverride(niceName);
            st = fa.addToCategory(category);
            if (!st)
                return st;
            st = dep.addAttribute(a);
            if (!st)
                return st;
        }
        MPlug p = dep.findPlug(longName, true, &st);
        if (!st || p.isNull())
            return MS::kFailure;
        p.setValue(defVal);
        return MS::kSuccess;
    }

    static MStatus addDoubleAttr(MFnDependencyNode& dep, const char* longName, const char* briefName, const MString& niceName,
                                 const MString& category, double defVal)
    {
        MStatus st;
        if (!dep.hasAttribute(longName))
        {
            MFnNumericAttribute n;
            MObject a = n.create(longName, briefName, MFnNumericData::kDouble, defVal, &st);
            if (!st)
                return st;
            n.setStorable(true);
            n.setKeyable(true);
            MFnAttribute fa(a);
            fa.setNiceNameOverride(niceName);
            st = fa.addToCategory(category);
            if (!st)
                return st;
            st = dep.addAttribute(a);
            if (!st)
                return st;
        }
        MPlug p = dep.findPlug(longName, true, &st);
        if (!st || p.isNull())
            return MS::kFailure;
        p.setValue(defVal);
        return MS::kSuccess;
    }

    static MStatus addBoolAsLongAttr(MFnDependencyNode& dep, const char* longName, const char* briefName, const MString& niceName,
                                   const MString& category, int def01)
    {
        return addLongAttr(dep, longName, briefName, niceName, category, def01, 0, 1);
    }

    static MString bladeCategory(int bladeIdx)
    {
        char buf[80];
        snprintf(buf, sizeof(buf), "LSB / BLAD blade %d", bladeIdx);
        return MString(buf);
    }

    static MStatus addBladeSlotAttrs(MFnDependencyNode& dep, int bladeIdx, const LsbBladeRow& b)
    {
        MString cat = bladeCategory(bladeIdx);
        char ln[96];
        char brShader[16], brLen[16], brWid[16], brOp[16], brCl[16];
        snprintf(brShader, sizeof(brShader), "b%dsht", bladeIdx);
        snprintf(brLen, sizeof(brLen), "b%dlen", bladeIdx);
        snprintf(brWid, sizeof(brWid), "b%dwd", bladeIdx);
        snprintf(brOp, sizeof(brOp), "b%dop", bladeIdx);
        snprintf(brCl, sizeof(brCl), "b%dcl", bladeIdx);

        snprintf(ln, sizeof(ln), "swgLsbBlade%dShader", bladeIdx);
        MStatus s = addStringAttr(dep, ln, brShader, MString("Shader template (.sht) - BLAD SHDR"), cat, b.shader);
        if (!s)
            return s;

        snprintf(ln, sizeof(ln), "swgLsbBlade%dLength", bladeIdx);
        s = addDoubleAttr(dep, ln, brLen, MString("Blade length (world units) - BLAD LGTH"), cat, static_cast<double>(b.length));
        if (!s)
            return s;

        snprintf(ln, sizeof(ln), "swgLsbBlade%dWidth", bladeIdx);
        s = addDoubleAttr(dep, ln, brWid, MString("Blade width - BLAD WDTH"), cat, static_cast<double>(b.width));
        if (!s)
            return s;

        snprintf(ln, sizeof(ln), "swgLsbBlade%dOpenRate", bladeIdx);
        s = addDoubleAttr(dep, ln, brOp, MString("Open / extend rate - BLAD OPEN"), cat, static_cast<double>(b.openRate));
        if (!s)
            return s;

        snprintf(ln, sizeof(ln), "swgLsbBlade%dCloseRate", bladeIdx);
        s = addDoubleAttr(dep, ln, brCl, MString("Close / retract rate - BLAD CLOS"), cat, static_cast<double>(b.closeRate));
        return s;
    }
}

MStatus lsbApplyAttributesToTransform(MObject transformObj, const std::string& hiltPath, const std::string& ambientSound,
                                      bool useFlicker, bool flickerDayNight, int flickR, int flickG, int flickB, int flickA,
                                      float flickRangeMin, float flickRangeMax, float flickTimeMin, float flickTimeMax,
                                      const std::vector<LsbBladeRow>& blades)
{
    MFnDependencyNode dep(transformObj);
    MStatus st;

    st = addStringAttr(dep, "swgLsbHiltPath", "lsbHilt",
                       MString("Hilt appearance path - BASE chunk (e.g. appearance/.../*.apt)"), kCatCore, hiltPath);
    if (!st)
        return st;
    st = addStringAttr(dep, "swgLsbAmbientSound", "lsbAsnd",
                       MString("Ambient loop sound - ASND chunk (e.g. sound/.../*.snd)"), kCatCore, ambientSound);
    if (!st)
        return st;
    st = addBoolAsLongAttr(dep, "swgLsbUseLightFlicker", "lsbLgHt", MString("Enable LGHT flicker block (idle light pulse)"), kCatFlicker,
                           useFlicker ? 1 : 0);
    if (!st)
        return st;
    st = addBoolAsLongAttr(dep, "swgLsbFlickerDayNight", "lsbDayN",
                           MString("Link flicker to day/night cycle - LGHT DAYN"), kCatFlicker, flickerDayNight ? 1 : 0);
    if (!st)
        return st;

    st = addLongAttr(dep, "swgLsbFlickerColorR", "lsbFCR", MString("Flicker tint red - LGHT COLR (0-255)"), kCatFlicker, flickR, 0, 255);
    if (!st)
        return st;
    st = addLongAttr(dep, "swgLsbFlickerColorG", "lsbFCG", MString("Flicker tint green - LGHT COLR (0-255)"), kCatFlicker, flickG, 0, 255);
    if (!st)
        return st;
    st = addLongAttr(dep, "swgLsbFlickerColorB", "lsbFCB", MString("Flicker tint blue - LGHT COLR (0-255)"), kCatFlicker, flickB, 0, 255);
    if (!st)
        return st;
    st = addLongAttr(dep, "swgLsbFlickerColorA", "lsbFCA", MString("Flicker tint alpha - LGHT COLR (0-255)"), kCatFlicker, flickA, 0, 255);
    if (!st)
        return st;

    st = addDoubleAttr(dep, "swgLsbFlickerRangeMin", "lsbFRn",
                       MString("Flicker distance min - LGHT RANG (world units, point light range)"), kCatFlicker,
                       static_cast<double>(flickRangeMin));
    if (!st)
        return st;
    st = addDoubleAttr(dep, "swgLsbFlickerRangeMax", "lsbFRx",
                       MString("Flicker distance max - LGHT RANG"), kCatFlicker, static_cast<double>(flickRangeMax));
    if (!st)
        return st;
    st = addDoubleAttr(dep, "swgLsbFlickerTimeMin", "lsbFTn",
                       MString("Flicker interval min (seconds) - LGHT TIME"), kCatFlicker, static_cast<double>(flickTimeMin));
    if (!st)
        return st;
    st = addDoubleAttr(dep, "swgLsbFlickerTimeMax", "lsbFTx",
                       MString("Flicker interval max (seconds) - LGHT TIME"), kCatFlicker, static_cast<double>(flickTimeMax));
    if (!st)
        return st;

    const int bc = static_cast<int>(std::min(blades.size(), static_cast<size_t>(kLsbMaxBlades)));
    st = addLongAttr(dep, "swgLsbBladeCount", "lsbBn", MString("Number of BLAD forms to write"), kCatBladesSummary, bc, 0,
                     kLsbMaxBlades);
    if (!st)
        return st;

    for (int i = 0; i < kLsbMaxBlades; ++i)
    {
        LsbBladeRow row;
        if (i < static_cast<int>(blades.size()))
            row = blades[static_cast<size_t>(i)];
        st = addBladeSlotAttrs(dep, i, row);
        if (!st)
            return st;
    }
    return MS::kSuccess;
}

MStatus lsbApplyDefaultTemplateAttributes(MObject transformObj)
{
    LsbBladeRow one;
    one.shader = "shader/default_blade.sht";
    one.length = 1.8f;
    one.width = 0.15f;
    one.openRate = 0.5f;
    one.closeRate = 0.5f;
    std::vector<LsbBladeRow> blades = {one};
    return lsbApplyAttributesToTransform(transformObj, "", "", false, false, 32, 32, 32, 255, 1.0, 2.0, 0.02, 0.1, blades);
}

static void parseLegacyFlickerParams(const std::string& s, int& r, int& g, int& b, int& a, float& rMin, float& rMax, float& tMin,
                                     float& tMax)
{
    std::istringstream iss(s);
    iss >> r >> g >> b >> a >> rMin >> rMax >> tMin >> tMax;
}

static bool parseLegacyBladeLine(const std::string& line, LsbBladeRow& out)
{
    if (line.empty())
        return false;
    std::istringstream ls(line);
    std::getline(ls, out.shader, '\t');
    ls >> out.length >> out.width >> out.openRate >> out.closeRate;
    return true;
}

MStatus lsbGatherExportData(MFnDependencyNode& dep, std::string& hiltPath, std::string& ambientSound, bool& useFlicker,
                            bool& flickerDayNight, int& flickR, int& flickG, int& flickB, int& flickA, float& flickRangeMin,
                            float& flickRangeMax, float& flickTimeMin, float& flickTimeMax, std::vector<LsbBladeRow>& blades)
{
    hiltPath = plugString(dep, "swgLsbHiltPath");
    ambientSound = plugString(dep, "swgLsbAmbientSound");
    useFlicker = plugInt(dep, "swgLsbUseLightFlicker", 0) != 0;
    flickerDayNight = plugInt(dep, "swgLsbFlickerDayNight", 0) != 0;

    flickR = 32;
    flickG = 32;
    flickB = 32;
    flickA = 255;
    flickRangeMin = 1.f;
    flickRangeMax = 2.f;
    flickTimeMin = 0.02f;
    flickTimeMax = 0.1f;

    if (dep.hasAttribute("swgLsbFlickerColorR"))
    {
        flickR = plugInt(dep, "swgLsbFlickerColorR", 32);
        flickG = plugInt(dep, "swgLsbFlickerColorG", 32);
        flickB = plugInt(dep, "swgLsbFlickerColorB", 32);
        flickA = plugInt(dep, "swgLsbFlickerColorA", 255);
        flickRangeMin = static_cast<float>(plugDouble(dep, "swgLsbFlickerRangeMin", 1.0));
        flickRangeMax = static_cast<float>(plugDouble(dep, "swgLsbFlickerRangeMax", 2.0));
        flickTimeMin = static_cast<float>(plugDouble(dep, "swgLsbFlickerTimeMin", 0.02));
        flickTimeMax = static_cast<float>(plugDouble(dep, "swgLsbFlickerTimeMax", 0.1));
    }
    else
    {
        const std::string legacy = plugString(dep, "swgLsbFlickerParams");
        if (!legacy.empty())
            parseLegacyFlickerParams(legacy, flickR, flickG, flickB, flickA, flickRangeMin, flickRangeMax, flickTimeMin,
                                     flickTimeMax);
    }

    blades.clear();
    int bladeCount = plugInt(dep, "swgLsbBladeCount", 0);

    const bool granular0 = dep.hasAttribute("swgLsbBlade0Shader");
    if (granular0 && bladeCount > 0)
    {
        bladeCount = std::min(bladeCount, kLsbMaxBlades);
        for (int i = 0; i < bladeCount; ++i)
        {
            char shaderAttr[64], lenAttr[64], widAttr[64], opAttr[64], clAttr[64];
            snprintf(shaderAttr, sizeof(shaderAttr), "swgLsbBlade%dShader", i);
            snprintf(lenAttr, sizeof(lenAttr), "swgLsbBlade%dLength", i);
            snprintf(widAttr, sizeof(widAttr), "swgLsbBlade%dWidth", i);
            snprintf(opAttr, sizeof(opAttr), "swgLsbBlade%dOpenRate", i);
            snprintf(clAttr, sizeof(clAttr), "swgLsbBlade%dCloseRate", i);
            LsbBladeRow row;
            row.shader = plugString(dep, shaderAttr);
            row.length = static_cast<float>(plugDouble(dep, lenAttr, 1.8));
            row.width = static_cast<float>(plugDouble(dep, widAttr, 0.15));
            row.openRate = static_cast<float>(plugDouble(dep, opAttr, 0.5));
            row.closeRate = static_cast<float>(plugDouble(dep, clAttr, 0.5));
            blades.push_back(row);
        }
    }
    else if (bladeCount > 0)
    {
        blades.resize(static_cast<size_t>(bladeCount));
        for (int i = 0; i < bladeCount; ++i)
        {
            std::string an = "swgLsbBlade" + std::to_string(i);
            const std::string line = plugString(dep, an.c_str());
            if (!line.empty())
                parseLegacyBladeLine(line, blades[static_cast<size_t>(i)]);
        }
    }

    return MS::kSuccess;
}
