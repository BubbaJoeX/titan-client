#ifndef SWGMAYAEDITOR_LSBATTRIBUTES_H
#define SWGMAYAEDITOR_LSBATTRIBUTES_H

#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>
#include <maya/MStatus.h>

#include <string>
#include <vector>

struct LsbBladeRow
{
    std::string shader;
    float length = 1.8f;
    float width = 0.15f;
    float openRate = 0.5f;
    float closeRate = 0.5f;
};

/// Max BLAD forms written (matches UI blade slots).
constexpr int kLsbMaxBlades = 8;

/// After import or for `swgMakeLightsaber`: create/update all LSB custom attrs with values.
MStatus lsbApplyAttributesToTransform(MObject transformObj, const std::string& hiltPath, const std::string& ambientSound,
                                      bool useFlicker, bool flickerDayNight, int flickR, int flickG, int flickB, int flickA,
                                      float flickRangeMin, float flickRangeMax, float flickTimeMin, float flickTimeMax,
                                      const std::vector<LsbBladeRow>& blades);

/// Default cylinder / new rig: same attrs, template defaults.
MStatus lsbApplyDefaultTemplateAttributes(MObject transformObj);

/// Gather data for IFF export (prefers granular attrs; falls back to legacy string attrs).
MStatus lsbGatherExportData(MFnDependencyNode& dep, std::string& hiltPath, std::string& ambientSound, bool& useFlicker,
                            bool& flickerDayNight, int& flickR, int& flickG, int& flickB, int& flickA, float& flickRangeMin,
                            float& flickRangeMax, float& flickTimeMin, float& flickTimeMax, std::vector<LsbBladeRow>& blades);

#endif
