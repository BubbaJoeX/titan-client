#include "ans.h"
#include "SwgTranslatorNames.h"

#include "Iff.h"
#include "Tag.h"
#include "Globals.h"
#include "MayaUtility.h"
#include "Misc.h"
#include "Quaternion.h"
#include "CompressedQuaternion.h"
#include "Vector.h"
#include "MayaSceneBuilder.h"
#include <maya/MStatus.h>
#include <maya/MStringArray.h>
#include <maya/MPxFileTranslator.h>
#include <maya/MQuaternion.h>
#include <maya/MFnIkJoint.h>
#include <maya/MIntArray.h>
#include <maya/MFloatVectorArray.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>
#include <maya/MEulerRotation.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MItDag.h>
#include <maya/MDagPath.h>
#include <maya/MAnimControl.h>
#include <maya/MTime.h>
#include <maya/MGlobal.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MVector.h>
#include <maya/MFnAttribute.h>
#include <maya/MFnTransform.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>
#include <map>
#include <string>

#ifdef _WIN32
#include <windows.h>
#define STRICMP _stricmp
#define ANS_DEBUGVIEW(s) OutputDebugStringA((s).c_str())
#else
#define STRICMP strcasecmp
#define ANS_DEBUGVIEW(s) ((void)0)
#endif

// MayaExporter keeps ALL keyframes - no filtering. These fixes dropped keyframes and broke playback.
static const bool s_rotationCompressionFix = false;
static const bool s_translationFix = false;

// VectorKeyData constructor definition
AnsTranslator::VectorKeyData::VectorKeyData(float frameNumber, const Vector& vector)
    : m_frameNumber(frameNumber), m_vector(vector), m_oneOverDistanceToNextKeyframe(0.0f)
{}

#define ANS_LOG(fmt, ...) do { \
    char _b[512]; snprintf(_b, sizeof(_b), "ANS: " fmt, ##__VA_ARGS__); \
    MGlobal::displayInfo(_b); \
    std::string _msg = std::string(_b) + "\n"; \
    std::cerr << _msg; \
    ANS_DEBUGVIEW(_msg); \
} while(0)
#define ANS_WARN(fmt, ...) do { \
    char _b[512]; snprintf(_b, sizeof(_b), "ANS: " fmt, ##__VA_ARGS__); \
    MGlobal::displayWarning(_b); \
    std::string _msg = std::string(_b) + "\n"; \
    std::cerr << _msg; \
    ANS_DEBUGVIEW(_msg); \
} while(0)

struct AnimationTransformationData
{
    AnimationTransformationData(std::string jointName, bool hasAnimatedRotations, int16 rotationChannelIndex,
                                uint8 translationMask, int16 translationChannelIndexX, int16 translationChannelIndexY,
                                int16 translationChannelIndexZ) : jointName(std::move(jointName)),
            hasAnimatedRotations(hasAnimatedRotations), rotationChannelIndex(rotationChannelIndex),
            translationMask(translationMask), translationChannelIndexX(translationChannelIndexX),
            translationChannelIndexY(translationChannelIndexY), translationChannelIndexZ(translationChannelIndexZ)
    {}
    
    std::string jointName;
        bool hasAnimatedRotations;
        int16 rotationChannelIndex;
        uint8 translationMask;
        int16 translationChannelIndexX;
        int16 translationChannelIndexY;
        int16 translationChannelIndexZ;
};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

struct AnimationRotationChannel
{
    AnimationRotationChannel(int16 keyCount, uint8 xCompressionFormat, uint8 yCompressionFormat,
                             uint8 zCompressionFormat,
                             const std::vector<AnsTranslator::QuaternionKeyData> &keyDataVector) :
                             keyCount(keyCount),
                             xCompressionFormat(xCompressionFormat),
                             yCompressionFormat(yCompressionFormat),
                             zCompressionFormat(zCompressionFormat),
                             keyDataVector(keyDataVector)
                             {}

    int16 keyCount;
    uint8 xCompressionFormat;
    uint8 yCompressionFormat;
    uint8 zCompressionFormat;
    std::vector<AnsTranslator::QuaternionKeyData> keyDataVector;
};

class AnsTranslator::FullCompressedQuaternion
{
public:

    FullCompressedQuaternion(const CompressedQuaternion &rotation, uint8 xFormat, uint8 yFormat, uint8 zFormat);

    Quaternion  expand() const;

private:

    // Disabled.
    FullCompressedQuaternion();
    FullCompressedQuaternion &operator =(const FullCompressedQuaternion&);

private:

    const CompressedQuaternion m_rotation;
    const uint8                m_xFormat;
    const uint8                m_yFormat;
    const uint8                m_zFormat;

};

// ======================================================================
// class AnsTranslator::FullCompressedQuaternion
// ======================================================================

AnsTranslator::FullCompressedQuaternion::FullCompressedQuaternion(const CompressedQuaternion &rotation, uint8 xFormat, uint8 yFormat, uint8 zFormat) :
        m_rotation(rotation),
        m_xFormat(xFormat),
        m_yFormat(yFormat),
        m_zFormat(zFormat)
{
}

// ----------------------------------------------------------------------

inline Quaternion AnsTranslator::FullCompressedQuaternion::expand() const
{
    return m_rotation.expand(m_xFormat, m_yFormat, m_zFormat);
}

// ======================================================================

// creates an instance of the AnsTranslator
void* AnsTranslator::creator()
{
    return new AnsTranslator();
}

/**
 * Function that handles reading in the file (import/open operations)
 *
 * @param file the file
 * @param options options
 * @param mode file access mode
 * @return the operation success/failure
 */
MStatus AnsTranslator::reader (const MFileObject& file, const MString& options, MPxFileTranslator::FileAccessMode mode)
{
    CompressedQuaternion::install();

    // Clear existing animation and restore bind pose BEFORE reading new animation.
    // This ensures bindQuat/bindTranslation are the original values, not from a previous animation.
    ANS_LOG("clearing existing animation and restoring bind pose...");
    {
        MStatus clearStatus;
        // Select all joints
        MSelectionList jointSel;
        MItDag dagIt(MItDag::kDepthFirst, MFn::kJoint, &clearStatus);
        for (; !dagIt.isDone(); dagIt.next())
        {
            MDagPath path;
            if (dagIt.getPath(path))
                jointSel.add(path);
        }
        if (jointSel.length() > 0)
        {
            MGlobal::setActiveSelectionList(jointSel);
            // Delete animation curves on rotation and translation
            MGlobal::executeCommand("cutKey -clear -at translateX -at translateY -at translateZ "
                                    "-at rotateX -at rotateY -at rotateZ");
            // Restore bind pose
            clearStatus = MGlobal::executeCommand("dagPose -restore -global -bindPose");
            if (!clearStatus)
                ANS_WARN("dagPose failed - bind pose may not be restored correctly");
            MGlobal::clearSelectionList();
        }
        // Go to frame 0
        MAnimControl::setCurrentTime(MTime(0.0, MTime::uiUnit()));
    }

    MString mpath = file.expandedFullName();
    const char *fileName = mpath.asChar();
    ANS_LOG("import start: %s", fileName);
    Iff iff;
    if(!iff.open(fileName, false))
    {
        ANS_WARN("could not open file: %s", fileName);
        return MS::kFailure;
    }
    if(iff.getRawDataSize() < 1)
    {
        ANS_WARN("file was empty: %s", fileName);
        return MS::kFailure;
    }
    ANS_LOG("file opened: %d bytes", iff.getRawDataSize());

    {
        const Tag topTag = iff.getCurrentName();
        if (topTag != TAG_CKAT && topTag != TAG_KFAT)
        {
            std::cerr << fileName << " is NOT an animation file and is not currently supported!" << std::endl;
            return MS::kFailure;
        }

        ANS_LOG("%s", (topTag == TAG_KFAT) ? "loading KFAT (uncompressed)" : "loading CKAT (compressed)");

        float framesPerSecond = 30.0f;
        int frameCount = 0;
        std::vector<AnimationTransformationData> animTransformData;
        std::vector<std::vector<MayaSceneBuilder::QuatKeyframe>> rotKeyframes;
        std::vector<std::vector<MayaSceneBuilder::AnimKeyframe>> transKeyframes;
        std::vector<FullCompressedQuaternion> staticRotations;
        std::vector<float> staticTranslations;

        if (topTag == TAG_KFAT)
        {
            iff.enterForm(TAG_KFAT);
            iff.enterForm(TAG_0003);

            iff.enterChunk(TAG_INFO);
            framesPerSecond = iff.read_float();
            frameCount = static_cast<int>(iff.read_int32());
            const int transformInfoCount = static_cast<int>(iff.read_int32());
            const int rotationChannelCount = static_cast<int>(iff.read_int32());
            const int staticRotationCount = static_cast<int>(iff.read_int32());
            const int translationChannelCount = static_cast<int>(iff.read_int32());
            const int staticTranslationCount = static_cast<int>(iff.read_int32());
            iff.exitChunk(TAG_INFO);
            ANS_LOG("KFAT INFO: fps=%.1f frames=%d xfrm=%d rotCh=%d srot=%d transCh=%d strn=%d",
                framesPerSecond, frameCount, transformInfoCount, rotationChannelCount, staticRotationCount,
                translationChannelCount, staticTranslationCount);

            iff.enterForm(TAG_XFRM);
            animTransformData.reserve(static_cast<size_t>(transformInfoCount));
            ANS_LOG("KFAT XFRM: %d transforms", transformInfoCount);
            // Some assets store channel index in high byte (256->1, 512->2). Normalize when >= 256.
            auto hi = [](uint32 v) -> int16 { return v >= 256 ? static_cast<int16>(v >> 8) : static_cast<int16>(v); };
            for (int i = 0; i < transformInfoCount; ++i)
            {
                iff.enterChunk(TAG_XFIN);
                std::string name;
                iff.read_string(name);
                const bool hasAnim = iff.read_int8() != 0;
                const uint32 rotRaw = iff.read_uint32();
                const uint32 maskRaw = iff.read_uint32();
                const uint32 txRaw = iff.read_uint32();
                const uint32 tyRaw = iff.read_uint32();
                const uint32 tzRaw = iff.read_uint32();
                animTransformData.emplace_back(
                    std::move(name),
                    hasAnim,
                    hi(rotRaw),
                    static_cast<uint8>(maskRaw),
                    hi(txRaw),
                    hi(tyRaw),
                    hi(tzRaw));
                iff.exitChunk(TAG_XFIN);
            }
            iff.exitForm(TAG_XFRM);

            iff.enterForm(TAG_AROT);
            rotKeyframes.resize(static_cast<size_t>(rotationChannelCount));
            for (int rc = 0; rc < rotationChannelCount; ++rc)
            {
                iff.enterChunk(TAG_QCHN);
                const int keyframeCount = static_cast<int>(iff.read_int32());
                std::vector<MayaSceneBuilder::QuatKeyframe>& channel = rotKeyframes[static_cast<size_t>(rc)];
                channel.resize(static_cast<size_t>(keyframeCount));
                for (int k = 0; k < keyframeCount; ++k)
                {
                    MayaSceneBuilder::QuatKeyframe& qk = channel[static_cast<size_t>(k)];
                    qk.frame = static_cast<float>(iff.read_int32());
                    const float w = iff.read_float();
                    const float x = iff.read_float();
                    const float y = iff.read_float();
                    const float z = iff.read_float();
                    qk.rotation[0] = x;
                    qk.rotation[1] = y;
                    qk.rotation[2] = z;
                    qk.rotation[3] = w;
                }
                iff.exitChunk(TAG_QCHN);
            }
            iff.exitForm(TAG_AROT);

            iff.enterChunk(TAG_SROT);
            staticRotations.reserve(static_cast<size_t>(staticRotationCount));
            for (int sr = 0; sr < staticRotationCount; ++sr)
            {
                const float w = iff.read_float();
                const float x = iff.read_float();
                const float y = iff.read_float();
                const float z = iff.read_float();
                Quaternion q(w, x, y, z);
                CompressedQuaternion cq(q, 0, 0, 0);
                staticRotations.emplace_back(cq, 0, 0, 0);
            }
            iff.exitChunk(TAG_SROT);

            iff.enterForm(TAG_ATRN);
            transKeyframes.resize(static_cast<size_t>(translationChannelCount));
            for (int tc = 0; tc < translationChannelCount; ++tc)
            {
                iff.enterChunk(TAG_CHNL);
                const int keyframeCount = static_cast<int>(iff.read_int32());
                std::vector<MayaSceneBuilder::AnimKeyframe>& channel = transKeyframes[static_cast<size_t>(tc)];
                channel.resize(static_cast<size_t>(keyframeCount));
                for (int k = 0; k < keyframeCount; ++k)
                {
                    MayaSceneBuilder::AnimKeyframe& ak = channel[static_cast<size_t>(k)];
                    ak.frame = static_cast<float>(iff.read_int32());
                    ak.value = iff.read_float();
                }
                iff.exitChunk(TAG_CHNL);
            }
            iff.exitForm(TAG_ATRN);

            iff.enterChunk(TAG_STRN);
            staticTranslations.resize(static_cast<size_t>(staticTranslationCount));
            for (int st = 0; st < staticTranslationCount; ++st)
                staticTranslations[static_cast<size_t>(st)] = iff.read_float();
            iff.exitChunk(TAG_STRN);

            // Parse optional MSGS (animation messages)
            std::vector<AnimationMessage> animationMessages;
            if (!iff.atEndOfForm() && iff.isCurrentForm() && iff.getCurrentName() == TAG_MSGS)
            {
                iff.enterForm(TAG_MSGS);
                iff.enterChunk(TAG_INFO);
                const int messageCount = static_cast<int>(iff.read_int16());
                iff.exitChunk(TAG_INFO);
                
                for (int mi = 0; mi < messageCount; ++mi)
                {
                    iff.enterChunk(TAG_MESG);
                    AnimationMessage msg;
                    const int signalCount = static_cast<int>(iff.read_int16());
                    char msgName[256];
                    iff.read_string(msgName, sizeof(msgName) - 1);
                    msg.name = msgName;
                    msg.signalFrameNumbers.reserve(static_cast<size_t>(signalCount));
                    for (int si = 0; si < signalCount; ++si)
                        msg.signalFrameNumbers.push_back(static_cast<int>(iff.read_int16()));
                    iff.exitChunk(TAG_MESG);
                    animationMessages.push_back(msg);
                }
                iff.exitForm(TAG_MSGS);
                ANS_LOG("KFAT: parsed %zu animation messages", animationMessages.size());
            }
            
            // Parse optional LOCT (locomotion translation)
            LocomotionData locomotion;
            if (!iff.atEndOfForm() && !iff.isCurrentForm() && iff.getCurrentName() == TAG_LOCT)
            {
                iff.enterChunk(TAG_LOCT);
                locomotion.averageTranslationSpeed = iff.read_float();
                const int keyCount = static_cast<int>(iff.read_int16());
                locomotion.translationKeys.reserve(static_cast<size_t>(keyCount));
                for (int ki = 0; ki < keyCount; ++ki)
                {
                    const float frame = static_cast<float>(iff.read_int16());
                    const Vector vec = iff.read_floatVector();
                    locomotion.translationKeys.emplace_back(frame, vec);
                }
                iff.exitChunk(TAG_LOCT);
                ANS_LOG("KFAT: parsed locomotion translation (%d keys, avg speed %.2f)", keyCount, locomotion.averageTranslationSpeed);
            }
            
            // Parse optional LOCR (locomotion rotation)
            if (!iff.atEndOfForm() && !iff.isCurrentForm() && iff.getCurrentName() == TAG_LOCR)
            {
                iff.enterChunk(TAG_LOCR);
                const int keyCount = static_cast<int>(iff.read_int16());
                locomotion.rotationKeys.reserve(static_cast<size_t>(keyCount));
                for (int ki = 0; ki < keyCount; ++ki)
                {
                    const float frame = static_cast<float>(iff.read_int16());
                    const Quaternion q = iff.read_floatQuaternion();
                    // Store as QuaternionKeyData with default compression format (8,8,8)
                    CompressedQuaternion cq(q, 8, 8, 8);
                    locomotion.rotationKeys.emplace_back(frame, cq);
                }
                iff.exitChunk(TAG_LOCR);
                ANS_LOG("KFAT: parsed locomotion rotation (%d keys)", keyCount);
            }
            
            // Skip any remaining unknown chunks/forms
            while (!iff.atEndOfForm())
            {
                if (iff.isCurrentForm())
                {
                    iff.enterForm();
                    iff.exitForm();
                }
                else
                {
                    iff.enterChunk();
                    iff.exitChunk();
                }
            }

            iff.exitForm(TAG_0003);
            iff.exitForm(TAG_KFAT);
        }
        else
        {
        //---- Start Form CKAT [CKAT]
        //---- Start of Animation File
        iff.enterForm(TAG_CKAT);
    
            //---- Start Form Version [CKAT > XXXX]
            iff.enterForm(TAG_0001); // there is only version 1
    
                //---- Start Chunk INFO [CKAT > XXXX > INFO]
                iff.enterChunk(TAG_INFO);
                    framesPerSecond = iff.read_float();
                    frameCount = static_cast<int>(iff.read_int16());
                    int transformInfoCount = static_cast<int>(iff.read_int16());
                    int rotationChannelCount = static_cast<int>(iff.read_int16());
                    int staticRotationCount = static_cast<int>(iff.read_int16());
                    int translationChannelCount = static_cast<int>(iff.read_int16());
                    int staticTranslationCount = static_cast<int>(iff.read_int16());
                iff.exitChunk(TAG_INFO);
                ANS_LOG("CKAT INFO (raw): fps=%.1f frames=%d xfrm=%d rotCh=%d srot=%d transCh=%d strn=%d",
                    framesPerSecond, frameCount, transformInfoCount, rotationChannelCount, staticRotationCount,
                    translationChannelCount, staticTranslationCount);

                // Sanity check: prevent "vector too long" from bad/corrupt byte order
                constexpr int MAX_CKAT_COUNT = 4096;
                if (transformInfoCount < 0 || transformInfoCount > MAX_CKAT_COUNT) transformInfoCount = 0;
                if (rotationChannelCount < 0 || rotationChannelCount > MAX_CKAT_COUNT) rotationChannelCount = 0;
                if (staticRotationCount < 0 || staticRotationCount > MAX_CKAT_COUNT) staticRotationCount = 0;
                if (translationChannelCount < 0 || translationChannelCount > MAX_CKAT_COUNT) translationChannelCount = 0;
                if (staticTranslationCount < 0 || staticTranslationCount > MAX_CKAT_COUNT) staticTranslationCount = 0;
                
                //---- Start Form XFRM [CKAT > XXXX > XFRM]
                //---- Transform Information Entries
                iff.enterForm(TAG_XFRM);
                animTransformData.clear();
                animTransformData.reserve(static_cast<size_t>(transformInfoCount));
                    for(int i = 0; i < transformInfoCount; i++)
                    {
                        //---- Start Chunk XFIN [CKAT > XXXX > XFRM > XFIN]
                        iff.enterChunk(TAG_XFIN);
                            std::string xfinName;
                            iff.read_string(xfinName);
                            const bool hasAnim = iff.read_int8() != 0;
                            int16 rotIdx = iff.read_int16();
                            const uint8 mask = iff.read_uint8();
                            int16 tx = iff.read_int16();
                            int16 ty = iff.read_int16();
                            int16 tz = iff.read_int16();
                            // Some assets store index in high byte (256->1, 512->2). Normalize when >= 256.
                            auto hi = [](int16 v) -> int16 { return v >= 256 ? static_cast<int16>(static_cast<uint16>(v) >> 8) : v; };
                            rotIdx = hi(rotIdx);
                            tx = hi(tx);
                            ty = hi(ty);
                            tz = hi(tz);
                            animTransformData.emplace_back(
                                    std::move(xfinName),
                                    hasAnim,
                                    rotIdx,
                                    mask,
                                    tx,
                                    ty,
                                    tz);
                        iff.exitChunk(TAG_XFIN);
                    }
                iff.exitForm(TAG_XFRM);
                for (size_t xi = 0; xi < animTransformData.size() && xi < 5; ++xi) {
                    const auto& x = animTransformData[xi];
                    ANS_LOG("  XFRM[%zu]: %s rotIdx=%d hasAnim=%d mask=0x%02x tx=%d ty=%d tz=%d",
                        xi, x.jointName.c_str(), x.rotationChannelIndex, x.hasAnimatedRotations ? 1 : 0,
                        x.translationMask, x.translationChannelIndexX, x.translationChannelIndexY, x.translationChannelIndexZ);
                }
                if (animTransformData.size() > 5)
                    ANS_LOG("  ... and %zu more transforms", animTransformData.size() - 5);

                //---- Start Form AROT [CKAT > XXXX > AROT]
                //---- Animated Rotation Channels
                const bool hasArotTag = iff.enterForm(TAG_AROT, true);
                std::vector<AnimationRotationChannel> rotationChannels;
                if(hasArotTag)
                {
                    for(int i = 0; i < rotationChannelCount; i++)
                    {
                        //---- Start Chunk QCHN [CKAT > XXXX > AROT > QCHN]
                        iff.enterChunk(TAG_QCHN);

                            int keyCount = static_cast<int>(iff.read_int16());
                            if (keyCount < 0 || keyCount > MAX_CKAT_COUNT) keyCount = 0;
                            uint8 xFormat = iff.read_uint8();
                            uint8 yFormat = iff.read_uint8();
                            uint8 zFormat = iff.read_uint8();

                            std::vector<AnsTranslator::QuaternionKeyData> keyDataVector;
                            keyDataVector.reserve(static_cast<size_t>(keyCount));
                            for(int j = 0; j < keyCount; j++)
                            {
                                const auto frameNumber = static_cast<float>(iff.read_int16());
                                const uint32 compressedValue = iff.read_uint32();
                                keyDataVector.emplace_back(frameNumber, CompressedQuaternion(compressedValue));
                            }

                            rotationChannels.emplace_back(
                                        static_cast<int16>(keyCount),
                                        xFormat,        // x compression format
                                        yFormat,        // y compression format
                                        zFormat,        // z compression format
                                        keyDataVector   // key data vector
                            );

                        iff.exitChunk(TAG_QCHN);
                    }
                    iff.exitForm(TAG_AROT);
                }

                //---- Start Chunk SROT [CKAT > XXXX > SROT]
                //---- Static Rotation Channels
                const bool hasSrotTag = iff.enterChunk(TAG_SROT, true);
                staticRotations.clear();
                if (hasSrotTag)
                {
                    for(int i = 0; i < staticRotationCount; i++)
                    {
                        const uint8  xFormat                 = iff.read_uint8();
                        const uint8  yFormat                 = iff.read_uint8();
                        const uint8  zFormat                 = iff.read_uint8();
                        const uint32 compressedRotationValue = iff.read_uint32();
                        staticRotations.emplace_back(CompressedQuaternion(compressedRotationValue), xFormat, yFormat, zFormat);
                    }
                    iff.exitChunk(TAG_SROT);
                }

                //---- Start Form ATRN [CKAT > XXXX > ATRN]
                //---- Animated Translation Channels
                const bool hasAtrnTag = iff.enterForm(TAG_ATRN, true);
                std::vector<std::vector<RealKeyData>> translationChannels;
                if(hasAtrnTag)
                {
                    for(int i = 0; i < translationChannelCount; i++)
                    {
                        //---- Start Chunk CHNL [CKAT > XXXX > ATRN > CHNL]
                        iff.enterChunk(TAG_CHNL);

                            std::vector<RealKeyData> data;
                            int keyCount = static_cast<int>(iff.read_int16());
                            if (keyCount < 0 || keyCount > MAX_CKAT_COUNT) keyCount = 0;
                            data.reserve(static_cast<size_t>(keyCount));
                            for(int j = 0; j < keyCount; j++)
                            {
                                const float frameNumber = static_cast<float>(iff.read_int16());
                                const float value = iff.read_float();
                                data.emplace_back(frameNumber, value);
                            }

                            translationChannels.push_back(std::move(data));

                        iff.exitChunk(TAG_CHNL);
                    }
                    iff.exitForm(TAG_ATRN);
                }

                //---- Start Form STRN [CKAT > XXXX > STRN]
                //---- Static Translation Channels
                const bool hasStrnTag = iff.enterChunk(TAG_STRN, true);
                staticTranslations.clear();
                if (hasStrnTag)
                {
                    for (size_t i = 0; i < static_cast<size_t>(staticTranslationCount); ++i)
                    {
                        staticTranslations.emplace_back(iff.read_float());
                    }

                    iff.exitChunk(TAG_STRN);
                }


                // Parse optional MSGS (animation messages)
                std::vector<AnimationMessage> animationMessages;
                if (!iff.atEndOfForm() && iff.isCurrentForm() && iff.getCurrentName() == TAG_MSGS)
                {
                    iff.enterForm(TAG_MSGS);
                    iff.enterChunk(TAG_INFO);
                    const int messageCount = static_cast<int>(iff.read_int16());
                    iff.exitChunk(TAG_INFO);
                    
                    for (int mi = 0; mi < messageCount; ++mi)
                    {
                        iff.enterChunk(TAG_MESG);
                        AnimationMessage msg;
                        const int signalCount = static_cast<int>(iff.read_int16());
                        char msgName[256];
                        iff.read_string(msgName, sizeof(msgName) - 1);
                        msg.name = msgName;
                        msg.signalFrameNumbers.reserve(static_cast<size_t>(signalCount));
                        for (int si = 0; si < signalCount; ++si)
                            msg.signalFrameNumbers.push_back(static_cast<int>(iff.read_int16()));
                        iff.exitChunk(TAG_MESG);
                        animationMessages.push_back(msg);
                    }
                    iff.exitForm(TAG_MSGS);
                    ANS_LOG("CKAT: parsed %zu animation messages", animationMessages.size());
                }
                
                // Parse optional LOCT (locomotion translation) - CKAT uses same format
                LocomotionData locomotion;
                if (!iff.atEndOfForm() && !iff.isCurrentForm() && iff.getCurrentName() == TAG_LOCT)
                {
                    iff.enterChunk(TAG_LOCT);
                    locomotion.averageTranslationSpeed = iff.read_float();
                    const int keyCount = static_cast<int>(iff.read_int16());
                    locomotion.translationKeys.reserve(static_cast<size_t>(keyCount));
                    for (int ki = 0; ki < keyCount; ++ki)
                    {
                        const float frame = static_cast<float>(iff.read_int16());
                        const Vector vec = iff.read_floatVector();
                        locomotion.translationKeys.emplace_back(frame, vec);
                    }
                    iff.exitChunk(TAG_LOCT);
                    ANS_LOG("CKAT: parsed locomotion translation (%d keys)", keyCount);
                }
                
                // Parse optional QCHN for locomotion rotation (CKAT uses compressed QCHN, not LOCR)
                if (!iff.atEndOfForm() && !iff.isCurrentForm() && iff.getCurrentName() == TAG_QCHN)
                {
                    iff.enterChunk(TAG_QCHN);
                    const int keyCount = static_cast<int>(iff.read_int16());
                    const uint8 xFmt = iff.read_uint8();
                    const uint8 yFmt = iff.read_uint8();
                    const uint8 zFmt = iff.read_uint8();
                    locomotion.rotationKeys.reserve(static_cast<size_t>(keyCount));
                    for (int ki = 0; ki < keyCount; ++ki)
                    {
                        const float frame = static_cast<float>(iff.read_int16());
                        const uint32 compressed = iff.read_uint32();
                        CompressedQuaternion cq(compressed);
                        // Store the compressed quaternion - it will be expanded with format info when needed
                        locomotion.rotationKeys.emplace_back(frame, cq);
                    }
                    iff.exitChunk(TAG_QCHN);
                    ANS_LOG("CKAT: parsed locomotion rotation (%d keys, compressed)", keyCount);
                }
                
                // Skip any remaining unknown chunks/forms
                while (!iff.atEndOfForm())
                {
                    if (iff.isCurrentForm())
                    {
                        iff.enterForm();
                        iff.exitForm();
                    }
                    else
                    {
                        iff.enterChunk();
                        iff.exitChunk();
                    }
                }

                iff.exitForm(TAG_0001);
                iff.exitForm(TAG_CKAT);

                // Convert rotation channels to QuatKeyframe (delta quaternions)
                rotKeyframes.clear();
                rotKeyframes.resize(rotationChannels.size());
                size_t totalRotKeys = 0;
                for (size_t rc = 0; rc < rotationChannels.size(); ++rc)
                {
                    const AnimationRotationChannel& ch = rotationChannels[rc];
                    for (const auto& kd : ch.keyDataVector)
                    {
                        MayaSceneBuilder::QuatKeyframe qk;
                        qk.frame = kd.m_frameNumber;
                        Quaternion q = AnsTranslator::FullCompressedQuaternion(
                            kd.m_rotation, ch.xCompressionFormat, ch.yCompressionFormat, ch.zCompressionFormat).expand();
                        qk.rotation[0] = q.x;
                        qk.rotation[1] = q.y;
                        qk.rotation[2] = q.z;
                        qk.rotation[3] = q.w;
                        rotKeyframes[rc].push_back(qk);
                    }
                    totalRotKeys += rotKeyframes[rc].size();
                }
                ANS_LOG("CKAT rotCh: %zu channels, %zu total keys", rotKeyframes.size(), totalRotKeys);
                for (size_t rc = 0; rc < rotKeyframes.size() && rc < 5; ++rc)
                    ANS_LOG("  rotCh[%zu]: %zu keys", rc, rotKeyframes[rc].size());

                // Convert translation channels to AnimKeyframe
                transKeyframes.clear();
                transKeyframes.resize(translationChannels.size());
                for (size_t tc = 0; tc < translationChannels.size(); ++tc)
                {
                    for (const auto& kd : translationChannels[tc])
                    {
                        MayaSceneBuilder::AnimKeyframe ak;
                        ak.frame = kd.m_frameNumber;
                        ak.value = kd.m_keyValue;
                        transKeyframes[tc].push_back(ak);
                    }
                }
                size_t totalTransKeys = 0;
                for (size_t tc = 0; tc < transKeyframes.size(); ++tc)
                    totalTransKeys += transKeyframes[tc].size();
                ANS_LOG("CKAT transCh: %zu channels, %zu total keys", transKeyframes.size(), totalTransKeys);
                for (size_t tc = 0; tc < transKeyframes.size() && tc < 5; ++tc)
                    ANS_LOG("  transCh[%zu]: %zu keys", tc, transKeyframes[tc].size());
        }

        // Build joint map from scene: name -> vector of MDagPath (all instances for multi-skeleton)
        std::map<std::string, std::vector<MDagPath>> jointMap;
        std::vector<MDagPath> sceneJointsOrdered;
        {
            MItDag dagIt(MItDag::kDepthFirst, MFn::kJoint);
            for (; !dagIt.isDone(); dagIt.next())
            {
                MDagPath dagPath;
                dagIt.getPath(dagPath);
                MString pathStr = dagPath.partialPathName();
                std::string fullName(pathStr.asChar());


                sceneJointsOrdered.push_back(dagPath);

                // Get last path component (node name)
                const size_t lastPipe = fullName.rfind('|');
                std::string mayaName = (lastPipe != std::string::npos && lastPipe + 1 < fullName.size())
                    ? fullName.substr(lastPipe + 1) : fullName;
                // Strip namespace (e.g. "sarlacc:root__sarlacc" -> "root__sarlacc")
                const size_t nsColon = mayaName.rfind(':');
                if (nsColon != std::string::npos && nsColon + 1 < mayaName.size())
                    mayaName = mayaName.substr(nsColon + 1);
                std::string shortName = mayaName;
                const size_t dbl = mayaName.find("__");
                if (dbl != std::string::npos && dbl > 0)
                    shortName = mayaName.substr(0, dbl);
                auto addToMap = [&jointMap](const std::string& key, const MDagPath& path) {
                    std::vector<MDagPath>& vec = jointMap[key];
                    if (std::find(vec.begin(), vec.end(), path) == vec.end())
                        vec.push_back(path);
                };
                addToMap(shortName, dagPath);
                addToMap(mayaName, dagPath);
                // Scene may have LOD suffix (root__sarlacc_l0) but ANS expects root__sarlacc
                if (mayaName.size() >= 4)
                {
                    size_t lodStart = mayaName.rfind("_l");
                    if (lodStart != std::string::npos && lodStart > 0 && lodStart + 2 < mayaName.size())
                    {
                        bool allDigits = true;
                        for (size_t i = lodStart + 2; i < mayaName.size(); ++i)
                            if (mayaName[i] < '0' || mayaName[i] > '9') { allDigits = false; break; }
                        if (allDigits)
                            addToMap(mayaName.substr(0, lodStart), dagPath);
                    }
                }
            }
        }

        {
            std::string sampleNames;
            for (size_t i = 0; i < sceneJointsOrdered.size() && i < 3; ++i) {
                MString s = sceneJointsOrdered[i].partialPathName();
                std::string full(s.asChar());
                size_t last = full.rfind('|');
                if (last != std::string::npos && last + 1 < full.size()) full = full.substr(last + 1);
                if (i) sampleNames += ", ";
                sampleNames += full;
            }
            ANS_LOG("scene joints: %zu (sample: %s)", sceneJointsOrdered.size(), sampleNames.empty() ? "(none)" : sampleNames.c_str());
        }

        ANS_LOG("fps=%.1f frames=%d transforms=%u rotCh=%zu staticRot=%zu transCh=%zu staticTrans=%zu",
            framesPerSecond, frameCount, static_cast<unsigned>(animTransformData.size()),
            rotKeyframes.size(), staticRotations.size(), transKeyframes.size(), staticTranslations.size());

        if (framesPerSecond >= 29.0f && framesPerSecond <= 31.0f)
            MGlobal::executeCommand("currentUnit -t ntsc");
        else if (framesPerSecond >= 23.0f && framesPerSecond <= 25.0f)
            MGlobal::executeCommand("currentUnit -t film");

        const uint8 MASK_X = 0x08;
                const uint8 MASK_Y = 0x10;
                const uint8 MASK_Z = 0x20;

                auto findJoints = [&jointMap](const std::string& name) -> std::vector<MDagPath> {
                    auto it = jointMap.find(name);
                    if (it != jointMap.end() && !it->second.empty())
                        return it->second;
                    const size_t dbl = name.find("__");
                    std::string baseName = name;
                    if (dbl != std::string::npos && dbl > 0)
                    {
                        baseName = name.substr(0, dbl);
                        it = jointMap.find(baseName);
                        if (it != jointMap.end() && !it->second.empty())
                            return it->second;
                    }
                    std::string lowerName = name;
                    std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    for (auto j = jointMap.begin(); j != jointMap.end(); ++j)
                    {
                        std::string jLower = j->first;
                        std::transform(jLower.begin(), jLower.end(), jLower.begin(),
                            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                        if (jLower == lowerName && !j->second.empty())
                            return j->second;
                    }
                    if (dbl != std::string::npos && dbl > 0)
                    {
                        std::string baseLower = baseName;
                        std::transform(baseLower.begin(), baseLower.end(), baseLower.begin(),
                            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                        for (auto j = jointMap.begin(); j != jointMap.end(); ++j)
                        {
                            std::string jLower = j->first;
                            std::transform(jLower.begin(), jLower.end(), jLower.begin(),
                                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                            if (jLower == baseLower && !j->second.empty())
                                return j->second;
                        }
                    }
                    return std::vector<MDagPath>();
                };

                // MayaExporter uses name-based matching only. Index-based matching applied
                // animation to wrong joints when ANS and scene joint order differed.
                const size_t ansCount = animTransformData.size();
                const size_t sceneCount = sceneJointsOrdered.size();

                int matchedCount = 0;
                for (size_t t = 0; t < animTransformData.size(); ++t)
                {
                    const AnimationTransformationData& ti = animTransformData[t];
                    std::vector<MDagPath> jointPaths = findJoints(ti.jointName);
                    if (jointPaths.empty())
                    {
                        ANS_WARN("skip [%s] - no matching joint in scene", ti.jointName.c_str());
                        continue;
                    }
                    for (const MDagPath& jointPath : jointPaths)
                    {
                    ++matchedCount;
                    MStatus status;
                    MFnIkJoint jointFn(jointPath.node());
                    MVector bindTranslation = jointFn.getTranslation(MSpace::kTransform);

                    if (ti.hasAnimatedRotations)
                    {
                        const int idx = ti.rotationChannelIndex;
                        if (idx >= 0 && idx < static_cast<int>(rotKeyframes.size()))
                        {
                            const size_t kc = rotKeyframes[static_cast<size_t>(idx)].size();
                            if (kc == 0)
                                ANS_WARN("[%s] rot anim ch=%d has 0 keys - no anim curve created", ti.jointName.c_str(), idx);
                            else {
                                MStatus rotStatus = MayaSceneBuilder::setRotationKeyframesFromDeltas(jointPath, rotKeyframes[static_cast<size_t>(idx)], framesPerSecond);
                                if (!rotStatus)
                                    ANS_WARN("[%s] setRotationKeyframesFromDeltas failed: %s", ti.jointName.c_str(), rotStatus.errorString().asChar());
                                else
                                    ANS_LOG("[%s] rot anim ch=%d keys=%zu OK", ti.jointName.c_str(), idx, kc);
                            }
                        }
                        else if (idx >= 0)
                        {
                            ANS_WARN("[%s] rot anim ch=%d OUT OF RANGE (rotCh=%zu)", ti.jointName.c_str(), idx, rotKeyframes.size());
                        }
                    }
                    else if (ti.rotationChannelIndex >= 0 && ti.rotationChannelIndex < static_cast<int>(staticRotations.size()))
                    {
                        ANS_LOG("[%s] rot static ch=%d", ti.jointName.c_str(), ti.rotationChannelIndex);
                        const FullCompressedQuaternion& sr = staticRotations[static_cast<size_t>(ti.rotationChannelIndex)];
                        Quaternion deltaQ = sr.expand();
                        MQuaternion bindQuat;
                        jointFn.getRotation(bindQuat, MSpace::kTransform);
                        MQuaternion mq(deltaQ.x, deltaQ.y, deltaQ.z, deltaQ.w);
                        MQuaternion finalQ = mq * bindQuat;
                        MEulerRotation finalEuler = finalQ.asEulerRotation();
                        finalEuler.reorderIt(static_cast<MEulerRotation::RotationOrder>(jointFn.rotationOrder()));
                        jointFn.setRotation(MEulerRotation(finalEuler.x, -finalEuler.y, -finalEuler.z));
                    }

                    // Translation (delta from bind: final = bind + delta; X negated for engine coords)
                    // Use setTranslation (matches MayaExporter) - get current t, update one component, set
                    if (ti.translationMask & MASK_X)
                    {
                        const int idx = ti.translationChannelIndexX;
                        if (idx >= 0 && idx < static_cast<int>(transKeyframes.size()))
                        {
                            const size_t kc = transKeyframes[static_cast<size_t>(idx)].size();
                            MStatus status = MayaSceneBuilder::setKeyframesFromDeltas(jointPath, "translateX", transKeyframes[static_cast<size_t>(idx)], framesPerSecond, bindTranslation.x, true);
                            if (!status)
                                ANS_WARN("[%s] setKeyframesFromDeltas(translateX) failed: %s", ti.jointName.c_str(), status.errorString().asChar());
                            else if (kc > 0)
                                ANS_LOG("[%s] transX anim ch=%d keys=%zu OK", ti.jointName.c_str(), idx, kc);
                        }
                        else if (idx >= 0)
                            ANS_WARN("[%s] transX ch=%d OUT OF RANGE (transCh=%zu)", ti.jointName.c_str(), idx, transKeyframes.size());
                    }
                    else if (ti.translationChannelIndexX >= 0 && ti.translationChannelIndexX < static_cast<int>(staticTranslations.size()))
                    {
                        MVector t = jointFn.getTranslation(MSpace::kTransform);
                        t.x = bindTranslation.x + static_cast<double>(-staticTranslations[static_cast<size_t>(ti.translationChannelIndexX)]);
                        jointFn.setTranslation(t, MSpace::kTransform);
                    }

                    if (ti.translationMask & MASK_Y)
                    {
                        const int idx = ti.translationChannelIndexY;
                        if (idx >= 0 && idx < static_cast<int>(transKeyframes.size()))
                        {
                            const size_t kc = transKeyframes[static_cast<size_t>(idx)].size();
                            MStatus status = MayaSceneBuilder::setKeyframesFromDeltas(jointPath, "translateY", transKeyframes[static_cast<size_t>(idx)], framesPerSecond, bindTranslation.y, false);
                            if (!status)
                                ANS_WARN("[%s] setKeyframesFromDeltas(translateY) failed: %s", ti.jointName.c_str(), status.errorString().asChar());
                            else if (kc > 0)
                                ANS_LOG("[%s] transY anim ch=%d keys=%zu OK", ti.jointName.c_str(), idx, kc);
                        }
                        else if (idx >= 0)
                            ANS_WARN("[%s] transY ch=%d OUT OF RANGE (transCh=%zu)", ti.jointName.c_str(), idx, transKeyframes.size());
                    }
                    else if (ti.translationChannelIndexY >= 0 && ti.translationChannelIndexY < static_cast<int>(staticTranslations.size()))
                    {
                        MVector t = jointFn.getTranslation(MSpace::kTransform);
                        t.y = bindTranslation.y + static_cast<double>(staticTranslations[static_cast<size_t>(ti.translationChannelIndexY)]);
                        jointFn.setTranslation(t, MSpace::kTransform);
                    }

                    if (ti.translationMask & MASK_Z)
                    {
                        const int idx = ti.translationChannelIndexZ;
                        if (idx >= 0 && idx < static_cast<int>(transKeyframes.size()))
                        {
                            const size_t kc = transKeyframes[static_cast<size_t>(idx)].size();
                            MStatus status = MayaSceneBuilder::setKeyframesFromDeltas(jointPath, "translateZ", transKeyframes[static_cast<size_t>(idx)], framesPerSecond, bindTranslation.z, false);
                            if (!status)
                                ANS_WARN("[%s] setKeyframesFromDeltas(translateZ) failed: %s", ti.jointName.c_str(), status.errorString().asChar());
                            else if (kc > 0)
                                ANS_LOG("[%s] transZ anim ch=%d keys=%zu OK", ti.jointName.c_str(), idx, kc);
                        }
                        else if (idx >= 0)
                            ANS_WARN("[%s] transZ ch=%d OUT OF RANGE (transCh=%zu)", ti.jointName.c_str(), idx, transKeyframes.size());
                    }
                    else if (ti.translationChannelIndexZ >= 0 && ti.translationChannelIndexZ < static_cast<int>(staticTranslations.size()))
                    {
                        MVector t = jointFn.getTranslation(MSpace::kTransform);
                        t.z = bindTranslation.z + static_cast<double>(staticTranslations[static_cast<size_t>(ti.translationChannelIndexZ)]);
                        jointFn.setTranslation(t, MSpace::kTransform);
                    }
                    }
                }

                ANS_LOG("applied to %d joints (file: %u transforms, scene: %u joints, rotCh=%zu transCh=%zu)",
                    matchedCount, static_cast<unsigned>(animTransformData.size()), static_cast<unsigned>(sceneJointsOrdered.size()),
                    rotKeyframes.size(), transKeyframes.size());

                {
                    auto getJointName = [](const MDagPath& path) -> std::string {
                        MString s = path.partialPathName();
                        std::string full(s.asChar());
                        size_t last = full.rfind('|');
                        if (last != std::string::npos && last + 1 < full.size())
                            full = full.substr(last + 1);
                        size_t col = full.rfind(':');
                        if (col != std::string::npos && col + 1 < full.size())
                            full = full.substr(col + 1);
                        return full;
                    };
                    ANS_LOG("joint mapping: [.ans joint] [scene joint] [status]");
                    for (size_t t = 0; t < animTransformData.size(); ++t)
                    {
                        const std::string& ansName = animTransformData[t].jointName;
                        std::string sceneName;
                        const char* status;
                        std::vector<MDagPath> jpaths = findJoints(ansName);
                        if (!jpaths.empty())
                        {
                            sceneName = getJointName(jpaths[0]);
                            if (jpaths.size() > 1)
                                sceneName += " (+" + std::to_string(jpaths.size() - 1) + " more)";
                            status = "matched";
                        }
                        else
                        {
                            sceneName = "-";
                            status = "skipped";
                        }
                        ANS_LOG("  [%s] [%s] [%s]", ansName.c_str(), sceneName.c_str(), status);
                    }
                }

                if (matchedCount == 0)
                {
                    ANS_WARN("No joints matched - animation not applied. Import the SAT that matches this animation first (e.g. rancor.sat for rancor_*.ans).");
                    std::string ansSample;
                    for (size_t i = 0; i < animTransformData.size() && i < 5; ++i)
                        ansSample += (i ? ", " : "") + animTransformData[i].jointName;
                    std::string sceneSample;
                    size_t n = 0;
                    for (auto it = jointMap.begin(); it != jointMap.end() && n < 5; ++it, ++n)
                        sceneSample += (n ? ", " : "") + it->first;
                    ANS_WARN("joints (sample): [%s] | Scene joints (sample): [%s] | ANS count=%u scene count=%u",
                        ansSample.c_str(), sceneSample.c_str(),
                        static_cast<unsigned>(animTransformData.size()), static_cast<unsigned>(sceneJointsOrdered.size()));
                }

                MTime::Unit uiUnit = MTime::uiUnit();
                MTime startTime(0.0, uiUnit);
                MTime endTime(static_cast<double>(frameCount), uiUnit);
                MAnimControl::setAnimationStartEndTime(startTime, endTime);
                MAnimControl::setMinMaxTime(startTime, endTime);
                MAnimControl::setCurrentTime(startTime);
                ANS_LOG("time range: 0 - %d frames (%.1f fps)", frameCount, framesPerSecond);
                {
                    const double startVal = startTime.as(MTime::uiUnit());
                    const double endVal = endTime.as(MTime::uiUnit());
                    char pbCmd[256];
                    snprintf(pbCmd, sizeof(pbCmd),
                        "playbackOptions -minTime %g -maxTime %g -animationStartTime %g -animationEndTime %g",
                        startVal, endVal, startVal, endVal);
                    MGlobal::executeCommand(pbCmd);
                }

                ANS_LOG("import complete: %d joints animated", matchedCount);
        return MS::kSuccess;
    }
}

/**
 * Handles writing out (exporting) the animation
 *
 * @param file the file to write
 * @param options the save options
 * @param mode the access mode of the file
 * @return the status of the operation
 */
MStatus AnsTranslator::writer (const MFileObject& file, const MString& options, MPxFileTranslator::FileAccessMode mode)
{
    MString mpath = file.expandedFullName();
    const char* fileName = mpath.asChar();
    ANS_LOG("export start: %s", fileName);

    MStatus status;

    // Get animation time range
    MTime startTime = MAnimControl::minTime();
    MTime endTime = MAnimControl::maxTime();
    int firstFrame = static_cast<int>(startTime.as(MTime::uiUnit()));
    int lastFrame = static_cast<int>(endTime.as(MTime::uiUnit()));
    int frameCount = lastFrame - firstFrame;
    if (frameCount < 1)
    {
        ANS_WARN("No animation frames to export (range: %d - %d)", firstFrame, lastFrame);
        return MS::kFailure;
    }

    float fps = static_cast<float>(MTime(1.0, MTime::kSeconds).as(MTime::uiUnit()));
    ANS_LOG("exporting frames %d - %d (%.1f fps)", firstFrame, lastFrame, fps);

    // Collect all joints in scene
    std::vector<MDagPath> joints;
    {
        MItDag dagIt(MItDag::kDepthFirst, MFn::kJoint, &status);
        for (; !dagIt.isDone(); dagIt.next())
        {
            MDagPath path;
            if (dagIt.getPath(path))
                joints.push_back(path);
        }
    }
    if (joints.empty())
    {
        ANS_WARN("No joints found in scene");
        return MS::kFailure;
    }
    ANS_LOG("found %zu joints", joints.size());

    // Go to bind pose (frame 0 or first frame) to capture bind pose data
    MAnimControl::setCurrentTime(MTime(static_cast<double>(firstFrame), MTime::uiUnit()));

    // Capture bind pose for each joint
    struct JointBindPose {
        std::string name;
        Quaternion rotation;
        float tx, ty, tz;
    };
    std::vector<JointBindPose> bindPoses;
    bindPoses.reserve(joints.size());

    for (const MDagPath& jp : joints)
    {
        MFnIkJoint jointFn(jp.node(), &status);
        if (!status) continue;

        JointBindPose bp;
        // Get short name (without namespace or path)
        MString fullName = jp.partialPathName();
        std::string nameStr(fullName.asChar());
        size_t lastPipe = nameStr.rfind('|');
        if (lastPipe != std::string::npos) nameStr = nameStr.substr(lastPipe + 1);
        size_t nsColon = nameStr.rfind(':');
        if (nsColon != std::string::npos) nameStr = nameStr.substr(nsColon + 1);
        bp.name = nameStr;

        // Get rotation and convert to game coordinates
        MEulerRotation mayaRot;
        jointFn.getRotation(mayaRot);
        // Game convention: negate Y and Z euler angles
        Quaternion qx(static_cast<float>(mayaRot.x), Vector::unitX);
        Quaternion qy(static_cast<float>(-mayaRot.y), Vector::unitY);
        Quaternion qz(static_cast<float>(-mayaRot.z), Vector::unitZ);
        bp.rotation = qz * (qy * qx); // XYZ order

        // Get translation and convert to game coordinates (negate X)
        MVector mayaTrans = jointFn.getTranslation(MSpace::kTransform);
        bp.tx = static_cast<float>(-mayaTrans.x);
        bp.ty = static_cast<float>(mayaTrans.y);
        bp.tz = static_cast<float>(mayaTrans.z);

        bindPoses.push_back(bp);
    }

    // Data structures for animation channels
    struct RotationChannel {
        std::vector<std::pair<int, Quaternion>> keys; // frame, rotation
    };
    struct TranslationChannel {
        std::vector<std::pair<int, float>> keys; // frame, value
    };

    std::vector<RotationChannel> rotChannels;
    std::vector<TranslationChannel> transXChannels, transYChannels, transZChannels;
    std::vector<Quaternion> staticRotations;
    std::vector<float> staticTranslations;

    struct TransformInfo {
        std::string name;
        bool hasAnimatedRotation;
        int rotationChannelIndex;
        uint8_t translationMask;
        int xTransChannelIndex;
        int yTransChannelIndex;
        int zTransChannelIndex;
    };
    std::vector<TransformInfo> transformInfos;

    // For each joint, sample animation and determine if animated or static
    for (size_t ji = 0; ji < joints.size(); ++ji)
    {
        const MDagPath& jp = joints[ji];
        const JointBindPose& bp = bindPoses[ji];
        MFnIkJoint jointFn(jp.node());

        // Sample rotation and translation at each frame
        std::vector<Quaternion> rotSamples;
        std::vector<float> txSamples, tySamples, tzSamples;
        rotSamples.reserve(static_cast<size_t>(frameCount + 1));
        txSamples.reserve(static_cast<size_t>(frameCount + 1));
        tySamples.reserve(static_cast<size_t>(frameCount + 1));
        tzSamples.reserve(static_cast<size_t>(frameCount + 1));

        for (int f = firstFrame; f <= lastFrame; ++f)
        {
            MAnimControl::setCurrentTime(MTime(static_cast<double>(f), MTime::uiUnit()));

            MEulerRotation mayaRot;
            jointFn.getRotation(mayaRot);
            Quaternion qx(static_cast<float>(mayaRot.x), Vector::unitX);
            Quaternion qy(static_cast<float>(-mayaRot.y), Vector::unitY);
            Quaternion qz(static_cast<float>(-mayaRot.z), Vector::unitZ);
            Quaternion animRot = qz * (qy * qx);

            // Compute delta from bind pose: delta = anim * bind.conjugate()
            Quaternion deltaRot = animRot * bp.rotation.getComplexConjugate();
            rotSamples.push_back(deltaRot);

            MVector mayaTrans = jointFn.getTranslation(MSpace::kTransform);
            float animTx = static_cast<float>(-mayaTrans.x);
            float animTy = static_cast<float>(mayaTrans.y);
            float animTz = static_cast<float>(mayaTrans.z);

            // Delta translation
            txSamples.push_back(animTx - bp.tx);
            tySamples.push_back(animTy - bp.ty);
            tzSamples.push_back(animTz - bp.tz);
        }

        // Determine if rotation is animated (any delta significantly different from identity)
        bool rotAnimated = false;
        const float rotEpsilon = 0.0001f;
        for (const Quaternion& q : rotSamples)
        {
            if (std::abs(q.w - 1.0f) > rotEpsilon || std::abs(q.x) > rotEpsilon ||
                std::abs(q.y) > rotEpsilon || std::abs(q.z) > rotEpsilon)
            {
                rotAnimated = true;
                break;
            }
        }

        // Determine if each translation axis is animated
        auto isAnimated = [](const std::vector<float>& samples, float epsilon) {
            if (samples.empty()) return false;
            float first = samples[0];
            for (float v : samples)
                if (std::abs(v - first) > epsilon) return true;
            return false;
        };
        const float transEpsilon = 0.0001f;
        bool txAnimated = isAnimated(txSamples, transEpsilon);
        bool tyAnimated = isAnimated(tySamples, transEpsilon);
        bool tzAnimated = isAnimated(tzSamples, transEpsilon);

        TransformInfo ti;
        ti.name = bp.name;
        ti.hasAnimatedRotation = rotAnimated;
        ti.translationMask = 0;

        if (rotAnimated)
        {
            ti.rotationChannelIndex = static_cast<int>(rotChannels.size());
            RotationChannel rc;
            for (int f = 0; f <= frameCount; ++f)
                rc.keys.emplace_back(f, rotSamples[static_cast<size_t>(f)]);
            rotChannels.push_back(rc);
        }
        else
        {
            ti.rotationChannelIndex = static_cast<int>(staticRotations.size());
            staticRotations.push_back(rotSamples.empty() ? Quaternion::identity : rotSamples[0]);
        }

        if (txAnimated)
        {
            ti.translationMask |= 0x08; // SATCCF_xTranslation
            ti.xTransChannelIndex = static_cast<int>(transXChannels.size());
            TranslationChannel tc;
            for (int f = 0; f <= frameCount; ++f)
                tc.keys.emplace_back(f, txSamples[static_cast<size_t>(f)]);
            transXChannels.push_back(tc);
        }
        else
        {
            ti.xTransChannelIndex = static_cast<int>(staticTranslations.size());
            staticTranslations.push_back(txSamples.empty() ? 0.0f : txSamples[0]);
        }

        if (tyAnimated)
        {
            ti.translationMask |= 0x10; // SATCCF_yTranslation
            ti.yTransChannelIndex = static_cast<int>(transYChannels.size());
            TranslationChannel tc;
            for (int f = 0; f <= frameCount; ++f)
                tc.keys.emplace_back(f, tySamples[static_cast<size_t>(f)]);
            transYChannels.push_back(tc);
        }
        else
        {
            ti.yTransChannelIndex = static_cast<int>(staticTranslations.size());
            staticTranslations.push_back(tySamples.empty() ? 0.0f : tySamples[0]);
        }

        if (tzAnimated)
        {
            ti.translationMask |= 0x20; // SATCCF_zTranslation
            ti.zTransChannelIndex = static_cast<int>(transZChannels.size());
            TranslationChannel tc;
            for (int f = 0; f <= frameCount; ++f)
                tc.keys.emplace_back(f, tzSamples[static_cast<size_t>(f)]);
            transZChannels.push_back(tc);
        }
        else
        {
            ti.zTransChannelIndex = static_cast<int>(staticTranslations.size());
            staticTranslations.push_back(tzSamples.empty() ? 0.0f : tzSamples[0]);
        }

        transformInfos.push_back(ti);
    }

    // Merge all translation channels into one vector
    std::vector<TranslationChannel> allTransChannels;
    // Remap indices
    int transOffset = 0;
    for (auto& ti : transformInfos)
    {
        if (ti.translationMask & 0x08)
        {
            allTransChannels.push_back(transXChannels[static_cast<size_t>(ti.xTransChannelIndex)]);
            ti.xTransChannelIndex = transOffset++;
        }
    }
    for (auto& ti : transformInfos)
    {
        if (ti.translationMask & 0x10)
        {
            allTransChannels.push_back(transYChannels[static_cast<size_t>(ti.yTransChannelIndex)]);
            ti.yTransChannelIndex = transOffset++;
        }
    }
    for (auto& ti : transformInfos)
    {
        if (ti.translationMask & 0x20)
        {
            allTransChannels.push_back(transZChannels[static_cast<size_t>(ti.zTransChannelIndex)]);
            ti.zTransChannelIndex = transOffset++;
        }
    }

    ANS_LOG("channels: rot=%zu staticRot=%zu trans=%zu staticTrans=%zu transforms=%zu",
        rotChannels.size(), staticRotations.size(), allTransChannels.size(), staticTranslations.size(), transformInfos.size());

    // Write KFAT (uncompressed) format
    Iff iff(2 * 1024 * 1024);
    iff.insertForm(TAG_KFAT);
    {
        iff.insertForm(TAG_0003);
        {
            // INFO chunk
            iff.insertChunk(TAG_INFO);
            {
                iff.insertChunkData(fps);
                iff.insertChunkData(static_cast<int32>(frameCount));
                iff.insertChunkData(static_cast<int32>(transformInfos.size()));
                iff.insertChunkData(static_cast<int32>(rotChannels.size()));
                iff.insertChunkData(static_cast<int32>(staticRotations.size()));
                iff.insertChunkData(static_cast<int32>(allTransChannels.size()));
                iff.insertChunkData(static_cast<int32>(staticTranslations.size()));
            }
            iff.exitChunk(TAG_INFO);

            // XFRM form - transform info
            iff.insertForm(TAG_XFRM);
            {
                for (const TransformInfo& ti : transformInfos)
                {
                    iff.insertChunk(TAG_XFIN);
                    {
                        iff.insertChunkString(ti.name.c_str());
                        iff.insertChunkData(static_cast<int8>(ti.hasAnimatedRotation ? 1 : 0));
                        iff.insertChunkData(static_cast<uint32>(ti.rotationChannelIndex));
                        iff.insertChunkData(static_cast<uint32>(ti.translationMask));
                        iff.insertChunkData(static_cast<uint32>(ti.xTransChannelIndex));
                        iff.insertChunkData(static_cast<uint32>(ti.yTransChannelIndex));
                        iff.insertChunkData(static_cast<uint32>(ti.zTransChannelIndex));
                    }
                    iff.exitChunk(TAG_XFIN);
                }
            }
            iff.exitForm(TAG_XFRM);

            // AROT form - animated rotation channels
            iff.insertForm(TAG_AROT);
            {
                for (const RotationChannel& rc : rotChannels)
                {
                    iff.insertChunk(TAG_QCHN);
                    {
                        iff.insertChunkData(static_cast<int32>(rc.keys.size()));
                        for (const auto& kv : rc.keys)
                        {
                            iff.insertChunkData(static_cast<int32>(kv.first));
                            iff.insertChunkFloatQuaternion(kv.second);
                        }
                    }
                    iff.exitChunk(TAG_QCHN);
                }
            }
            iff.exitForm(TAG_AROT);

            // SROT chunk - static rotations
            iff.insertChunk(TAG_SROT);
            {
                for (const Quaternion& q : staticRotations)
                    iff.insertChunkFloatQuaternion(q);
            }
            iff.exitChunk(TAG_SROT);

            // ATRN form - animated translation channels
            iff.insertForm(TAG_ATRN);
            {
                for (const TranslationChannel& tc : allTransChannels)
                {
                    iff.insertChunk(TAG_CHNL);
                    {
                        iff.insertChunkData(static_cast<int32>(tc.keys.size()));
                        for (const auto& kv : tc.keys)
                        {
                            iff.insertChunkData(static_cast<int32>(kv.first));
                            iff.insertChunkData(kv.second);
                        }
                    }
                    iff.exitChunk(TAG_CHNL);
                }
            }
            iff.exitForm(TAG_ATRN);

            // STRN chunk - static translations
            iff.insertChunk(TAG_STRN);
            {
                for (float v : staticTranslations)
                    iff.insertChunkData(v);
            }
            iff.exitChunk(TAG_STRN);
            
            // Check for animation messages stored as attributes on joints
            // Messages are stored as "swgAnimMsg_<name>" attributes with comma-separated frame numbers
            std::vector<AnimationMessage> animationMessages;
            for (const MDagPath& jp : joints)
            {
                MFnDependencyNode depFn(jp.node());
                for (unsigned ai = 0; ai < depFn.attributeCount(); ++ai)
                {
                    MObject attrObj = depFn.attribute(ai);
                    MFnAttribute attrFn(attrObj);
                    std::string attrName(attrFn.name().asChar());
                    if (attrName.find("swgAnimMsg_") == 0)
                    {
                        std::string msgName = attrName.substr(11); // Remove "swgAnimMsg_" prefix
                        MPlug plug = depFn.findPlug(attrObj, false);
                        MString frameStr;
                        if (plug.getValue(frameStr) == MS::kSuccess && frameStr.length() > 0)
                        {
                            AnimationMessage msg;
                            msg.name = msgName;
                            // Parse comma-separated frame numbers
                            std::string frames(frameStr.asChar());
                            size_t start = 0;
                            while (start < frames.size())
                            {
                                size_t end = frames.find(',', start);
                                if (end == std::string::npos) end = frames.size();
                                std::string numStr = frames.substr(start, end - start);
                                if (!numStr.empty())
                                    msg.signalFrameNumbers.push_back(std::stoi(numStr));
                                start = end + 1;
                            }
                            if (!msg.signalFrameNumbers.empty())
                                animationMessages.push_back(msg);
                        }
                    }
                }
                break; // Only check first joint for messages
            }
            
            // Write MSGS form if we have animation messages
            if (!animationMessages.empty())
            {
                iff.insertForm(TAG_MSGS);
                {
                    iff.insertChunk(TAG_INFO);
                    iff.insertChunkData(static_cast<int16>(animationMessages.size()));
                    iff.exitChunk(TAG_INFO);
                    
                    for (const AnimationMessage& msg : animationMessages)
                    {
                        iff.insertChunk(TAG_MESG);
                        iff.insertChunkData(static_cast<int16>(msg.signalFrameNumbers.size()));
                        iff.insertChunkString(msg.name.c_str());
                        for (int frame : msg.signalFrameNumbers)
                            iff.insertChunkData(static_cast<int16>(frame));
                        iff.exitChunk(TAG_MESG);
                    }
                }
                iff.exitForm(TAG_MSGS);
                ANS_LOG("exported %zu animation messages", animationMessages.size());
            }
            
            // Check for locomotion data - look for a "master" transform above the skeleton root
            // Locomotion is sampled from the parent of the skeleton root
            if (!joints.empty())
            {
                MDagPath rootJoint = joints[0];
                // Find the actual root joint (no joint parent)
                while (true)
                {
                    MDagPath parent = rootJoint;
                    parent.pop();
                    if (parent.hasFn(MFn::kJoint))
                        rootJoint = parent;
                    else
                        break;
                }
                
                // Check if root joint has a non-joint parent (the "master" node)
                MDagPath masterPath = rootJoint;
                masterPath.pop();
                if (masterPath.isValid() && masterPath.hasFn(MFn::kTransform) && !masterPath.hasFn(MFn::kJoint))
                {
                    MFnTransform masterFn(masterPath);
                    
                    // Check if master has swgLocomotion attribute enabled
                    MPlug locoPlug = masterFn.findPlug("swgLocomotion", false);
                    bool hasLocomotion = false;
                    if (!locoPlug.isNull())
                        locoPlug.getValue(hasLocomotion);
                    
                    if (hasLocomotion)
                    {
                        // Sample locomotion data
                        std::vector<std::pair<int, Vector>> locoTransKeys;
                        std::vector<std::pair<int, Quaternion>> locoRotKeys;
                        
                        for (int f = firstFrame; f <= lastFrame; ++f)
                        {
                            MAnimControl::setCurrentTime(MTime(static_cast<double>(f), MTime::uiUnit()));
                            
                            MVector trans = masterFn.getTranslation(MSpace::kTransform);
                            Vector gameVec(static_cast<float>(-trans.x), static_cast<float>(trans.y), static_cast<float>(trans.z));
                            locoTransKeys.emplace_back(f - firstFrame, gameVec);
                            
                            MEulerRotation rot;
                            masterFn.getRotation(rot);
                            Quaternion qx(static_cast<float>(rot.x), Vector::unitX);
                            Quaternion qy(static_cast<float>(-rot.y), Vector::unitY);
                            Quaternion qz(static_cast<float>(-rot.z), Vector::unitZ);
                            Quaternion gameRot = qz * (qy * qx);
                            locoRotKeys.emplace_back(f - firstFrame, gameRot);
                        }
                        
                        // Calculate average translation speed
                        float avgSpeed = 0.0f;
                        if (locoTransKeys.size() >= 2)
                        {
                            Vector delta = locoTransKeys.back().second - locoTransKeys.front().second;
                            float distance = delta.magnitude();
                            float timeSeconds = static_cast<float>(frameCount) / fps;
                            if (timeSeconds > 0.0f)
                                avgSpeed = distance / timeSeconds;
                        }
                        
                        // Write LOCT chunk
                        if (!locoTransKeys.empty())
                        {
                            iff.insertChunk(TAG_LOCT);
                            iff.insertChunkData(avgSpeed);
                            iff.insertChunkData(static_cast<int16>(locoTransKeys.size()));
                            for (const auto& kv : locoTransKeys)
                            {
                                iff.insertChunkData(static_cast<int16>(kv.first));
                                iff.insertChunkFloatVector(kv.second);
                            }
                            iff.exitChunk(TAG_LOCT);
                            ANS_LOG("exported locomotion translation (%zu keys, avg speed %.2f)", locoTransKeys.size(), avgSpeed);
                        }
                        
                        // Write LOCR chunk
                        if (!locoRotKeys.empty())
                        {
                            iff.insertChunk(TAG_LOCR);
                            iff.insertChunkData(static_cast<int16>(locoRotKeys.size()));
                            for (const auto& kv : locoRotKeys)
                            {
                                iff.insertChunkData(static_cast<int16>(kv.first));
                                iff.insertChunkFloatQuaternion(kv.second);
                            }
                            iff.exitChunk(TAG_LOCR);
                            ANS_LOG("exported locomotion rotation (%zu keys)", locoRotKeys.size());
                        }
                    }
                }
            }
        }
        iff.exitForm(TAG_0003);
    }
    iff.exitForm(TAG_KFAT);

    if (!iff.write(fileName))
    {
        ANS_WARN("failed to write file: %s", fileName);
        return MS::kFailure;
    }

    ANS_LOG("export complete: %s (%d frames, %zu joints)", fileName, frameCount, joints.size());
    return MS::kSuccess;
}

/**
 * @return the file type this translator handles
 */
MString AnsTranslator::defaultExtension () const
{
    return "ans";
}

MString AnsTranslator::filter () const
{
    return MString(swg_translator::kFilterAns);
}

/**
 * Validates if the provided file is one that this plug-in supports
 *
 * @param fileName the name of the file
 * @param buffer a buffer for reading into the file
 * @param size the size of the buffer
 * @return whether or not this file type is supported by this translator
 */
MPxFileTranslator::MFileKind AnsTranslator::identifyFile(const MFileObject& fileName, const char* /*buffer*/, short /*size*/) const
{
    const std::string pathStr = MayaUtility::fileObjectPathForIdentify(fileName);
    const int nameLength = static_cast<int>(pathStr.size());
    if (nameLength > 4 && STRICMP(pathStr.c_str() + nameLength - 4, ".ans") == 0)
        return kCouldBeMyFileType;
    return kNotMyFileType;
}

void AnsTranslator::calculateQuaternionDistanceToNextFrame(std::vector<QuaternionKeyData> &keyDataVector)
{
    const size_t keyCount = keyDataVector.size();

    // calculate one over distance between each keyframe and next
    for (size_t i = 0; i < keyCount - 1; ++i)
    {
        QuaternionKeyData &keyData = keyDataVector[i];
        keyData.m_oneOverDistanceToNextKeyframe = 1.0f / (keyDataVector[i + 1].m_frameNumber - keyData.m_frameNumber);
    }

    keyDataVector[keyCount - 1].m_oneOverDistanceToNextKeyframe = 0.0f;
}

void AnsTranslator::calculateFloatDistanceToNextFrame(std::vector<RealKeyData> &keyDataVector)
{

    const size_t keyCount = keyDataVector.size();

    // calculate one over distance between each keyframe and next
    for (size_t i = 0; i < keyCount - 1; ++i)
    {
        RealKeyData &keyData = keyDataVector[i];
        keyData.m_oneOverDistanceToNextKeyframe = 1.0f / (keyDataVector[i + 1].m_frameNumber - keyData.m_frameNumber);
    }

    keyDataVector[keyCount - 1].m_oneOverDistanceToNextKeyframe = 0.0f;
}

bool AnsTranslator::shouldCompressKeyFrame(const float previousValue, const float nextValue)
{
    return !WithinEpsilonInclusive(previousValue, nextValue, std::numeric_limits<float>::epsilon() * 2.0f);
}

