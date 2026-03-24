#include "ans.h"

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
#include <maya/MVector.h>

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


                // Skip optional MSGS and other trailing forms/chunks
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
    return "Animation - SWG (*.ans)";
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

