#ifndef SWGMAYAEDITOR_SKELETONTEMPLATEWRITER_H
#define SWGMAYAEDITOR_SKELETONTEMPLATEWRITER_H

#include <vector>

class Iff;
class Quaternion;
class Vector;

class SkeletonTemplateWriter
{
public:
    enum JointRotationOrder { JRO_xyz, JRO_xzy, JRO_yxz, JRO_yzx, JRO_zxy, JRO_zyx, JRO_COUNT };

    SkeletonTemplateWriter();
    ~SkeletonTemplateWriter();

    bool addJoint(int parentIndex, const char* jointName,
        const Quaternion& preMultiplyRotation, const Quaternion& postMultiplyRotation,
        const Vector& bindPoseTranslation, const Quaternion& bindPoseRotation,
        JointRotationOrder jointRotationOrder, int* newIndex);

    void write(Iff& iff) const;
    int getJointCount() const;

private:
    struct Joint;
    std::vector<Joint*> m_joints;

    SkeletonTemplateWriter(const SkeletonTemplateWriter&) = delete;
    SkeletonTemplateWriter& operator=(const SkeletonTemplateWriter&) = delete;
};

#endif
