#ifndef SWGMAYAEDITOR_MAYACONVERSIONS_H
#define SWGMAYAEDITOR_MAYACONVERSIONS_H

#include <maya/MEulerRotation.h>
#include <maya/MFloatVector.h>
#include <maya/MVector.h>

class Quaternion;
class Vector;

/**
 * Maya to SWG/engine coordinate and rotation conversions.
 * Matches MayaExporter MayaConversions for round-trip compatibility.
 */
class MayaConversions
{
public:
    static Quaternion convertRotation(const MEulerRotation& euler);
    static Vector convertVector(const MVector& mayaVector);
    static Vector convertVector(const MFloatVector& mayaVector);
};

#endif
