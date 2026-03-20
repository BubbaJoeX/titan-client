#include "MayaConversions.h"
#include "Quaternion.h"
#include "Vector.h"

Quaternion MayaConversions::convertRotation(const MEulerRotation& euler)
{
    Quaternion qx(static_cast<float>(euler.x), Vector::unitX);
    Quaternion qy(static_cast<float>(-euler.y), Vector::unitY);
    Quaternion qz(static_cast<float>(-euler.z), Vector::unitZ);

    switch (euler.order)
    {
    case MEulerRotation::kXYZ: return qz * (qy * qx);
    case MEulerRotation::kYZX: return qx * (qz * qy);
    case MEulerRotation::kZXY: return qy * (qx * qz);
    case MEulerRotation::kXZY: return qy * (qz * qx);
    case MEulerRotation::kYXZ: return qz * (qx * qy);
    case MEulerRotation::kZYX: return qx * (qy * qz);
    default:
        return Quaternion::identity;
    }
}

Vector MayaConversions::convertVector(const MVector& mayaVector)
{
    return Vector(static_cast<float>(-mayaVector.x), static_cast<float>(mayaVector.y), static_cast<float>(mayaVector.z));
}

Vector MayaConversions::convertVector(const MFloatVector& mayaVector)
{
    return Vector(static_cast<float>(-mayaVector.x), static_cast<float>(mayaVector.y), static_cast<float>(mayaVector.z));
}
