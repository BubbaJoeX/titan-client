#include "SkeletonTemplateWriter.h"
#include "Iff.h"
#include "Tag.h"
#include "Quaternion.h"
#include "Vector.h"

#include <cstring>
#include <string>
#include <vector>

namespace
{
    const Tag TAG_BPRO = TAG(B,P,R,O);
    const Tag TAG_BPTR = TAG(B,P,T,R);
    const Tag TAG_JROR = TAG(J,R,O,R);
    const Tag TAG_PRNT = TAG(P,R,N,T);
    const Tag TAG_RPRE = TAG(R,P,R,E);
    const Tag TAG_RPST = TAG(R,P,S,T);
    const Tag TAG_SKTM = TAG(S,K,T,M);
}

struct SkeletonTemplateWriter::Joint
{
    int parentIndex;
    std::string name;
    Quaternion preMultiplyRotation;
    Quaternion postMultiplyRotation;
    Vector bindPoseTranslation;
    Quaternion bindPoseRotation;
    JointRotationOrder rotationOrder;

    Joint(int p, const char* n, const Quaternion& pre, const Quaternion& post,
          const Vector& trans, const Quaternion& rot, JointRotationOrder ro)
        : parentIndex(p), name(n ? n : ""), preMultiplyRotation(pre), postMultiplyRotation(post),
          bindPoseTranslation(trans), bindPoseRotation(rot), rotationOrder(ro) {}
};

SkeletonTemplateWriter::SkeletonTemplateWriter()
{
    m_joints.reserve(64);
}

SkeletonTemplateWriter::~SkeletonTemplateWriter()
{
    for (Joint* j : m_joints)
        delete j;
    m_joints.clear();
}

bool SkeletonTemplateWriter::addJoint(int parentIndex, const char* jointName,
    const Quaternion& preMultiplyRotation, const Quaternion& postMultiplyRotation,
    const Vector& bindPoseTranslation, const Quaternion& bindPoseRotation,
    JointRotationOrder jointRotationOrder, int* newIndex)
{
    if (!jointName || !*jointName || !newIndex)
        return false;

    if (parentIndex < -1)
        parentIndex = -1;
    if (parentIndex >= 0 && static_cast<size_t>(parentIndex) >= m_joints.size())
        return false;

    Joint* joint = new Joint(parentIndex, jointName, preMultiplyRotation, postMultiplyRotation,
        bindPoseTranslation, bindPoseRotation, jointRotationOrder);
    m_joints.push_back(joint);
    *newIndex = static_cast<int>(m_joints.size() - 1);
    return true;
}

void SkeletonTemplateWriter::write(Iff& iff) const
{
    const size_t jointCount = m_joints.size();

    iff.insertForm(TAG_SKTM);
    iff.insertForm(TAG_0002);

    iff.insertChunk(TAG_INFO);
    iff.insertChunkData(static_cast<int32>(jointCount));
    iff.exitChunk(TAG_INFO);

    iff.insertChunk(TAG_NAME);
    for (size_t i = 0; i < jointCount; ++i)
        iff.insertChunkString(m_joints[i]->name.c_str());
    iff.exitChunk(TAG_NAME);

    iff.insertChunk(TAG_PRNT);
    for (size_t i = 0; i < jointCount; ++i)
        iff.insertChunkData(static_cast<int32>(m_joints[i]->parentIndex));
    iff.exitChunk(TAG_PRNT);

    iff.insertChunk(TAG_RPRE);
    for (size_t i = 0; i < jointCount; ++i)
        iff.insertChunkFloatQuaternion(m_joints[i]->preMultiplyRotation);
    iff.exitChunk(TAG_RPRE);

    iff.insertChunk(TAG_RPST);
    for (size_t i = 0; i < jointCount; ++i)
        iff.insertChunkFloatQuaternion(m_joints[i]->postMultiplyRotation);
    iff.exitChunk(TAG_RPST);

    iff.insertChunk(TAG_BPTR);
    for (size_t i = 0; i < jointCount; ++i)
        iff.insertChunkFloatVector(m_joints[i]->bindPoseTranslation);
    iff.exitChunk(TAG_BPTR);

    iff.insertChunk(TAG_BPRO);
    for (size_t i = 0; i < jointCount; ++i)
        iff.insertChunkFloatQuaternion(m_joints[i]->bindPoseRotation);
    iff.exitChunk(TAG_BPRO);

    iff.insertChunk(TAG_JROR);
    for (size_t i = 0; i < jointCount; ++i)
        iff.insertChunkData(static_cast<uint32>(m_joints[i]->rotationOrder));
    iff.exitChunk(TAG_JROR);

    iff.exitForm(TAG_0002);
    iff.exitForm(TAG_SKTM);
}

int SkeletonTemplateWriter::getJointCount() const
{
    return static_cast<int>(m_joints.size());
}
