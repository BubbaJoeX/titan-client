// ======================================================================
//
// ImportAnimation.cpp
// Copyright 2006 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#include "FirstMayaExporter.h"
#include "ImportPathResolver.h"
#include "ImportAnimation.h"

#include "MayaCompoundString.h"
#include "MayaSceneBuilder.h"
#include "Messenger.h"

#include "sharedFile/Iff.h"
#include "sharedFoundation/Tag.h"
#include "sharedMath/CompressedQuaternion.h"

#include "maya/MArgList.h"
#include "maya/MGlobal.h"
#include "maya/MSelectionList.h"
#include "maya/MDagPath.h"
#include "maya/MDagPathArray.h"
#include "maya/MFnDagNode.h"
#include "maya/MFnSkinCluster.h"
#include "maya/MItDag.h"
#include "maya/MItDependencyNodes.h"
#include "maya/MFnIkJoint.h"
#include "maya/MQuaternion.h"
#include "maya/MEulerRotation.h"
#include "maya/MVector.h"
#include "maya/MFnTransform.h"

#include <string>
#include <vector>
#include <map>
#include <set>

// ======================================================================

namespace
{
	Messenger *messenger;

	const Tag TAG_KFAT = TAG(K,F,A,T);
	const Tag TAG_CKAT = TAG(C,K,A,T);
	const Tag TAG_XFRM = TAG(X,F,R,M);
	const Tag TAG_XFIN = TAG(X,F,I,N);
	const Tag TAG_AROT = TAG(A,R,O,T);
	const Tag TAG_QCHN = TAG(Q,C,H,N);
	const Tag TAG_SROT = TAG(S,R,O,T);
	const Tag TAG_ATRN = TAG(A,T,R,N);
	const Tag TAG_CHNL = TAG(C,H,N,L);
	const Tag TAG_STRN = TAG(S,T,R,N);
	const Tag TAG_MSGS = TAG(M,S,G,S);
	const Tag TAG_MESG = TAG(M,E,S,G);
	const Tag TAG_LOCT = TAG(L,O,C,T);
	const Tag TAG_LOCR = TAG(L,O,C,R);

	const uint32 MASK_X = 0x08; // SATCCF_xTranslation
	const uint32 MASK_Y = 0x10; // SATCCF_yTranslation
	const uint32 MASK_Z = 0x20; // SATCCF_zTranslation

	struct TransformInfo
	{
		std::string  name;
		int8         hasAnimatedRotation;
		uint32       rotationChannelIndex;
		uint32       translationMask;
		uint32       xTranslationChannelIndex;
		uint32       yTranslationChannelIndex;
		uint32       zTranslationChannelIndex;
	};

	struct AnimMessage
	{
		std::string        name;
		std::vector<int16> signalFrames;
	};

	struct LocomotionTranslationKey
	{
		int16 frame;
		float x, y, z;
	};

	struct LocomotionRotationKey
	{
		int16 frame;
		float qx, qy, qz, qw;
	};

	// ------------------------------------------------------------------

	bool readUncompressedAnimation(
		Iff &iff,
		float &fps,
		int32 &frameCount,
		std::vector<TransformInfo> &transforms,
		std::vector< std::vector<MayaSceneBuilder::QuatKeyframe> > &rotationChannels,
		std::vector<float> &staticRotations,
		std::vector< std::vector<MayaSceneBuilder::AnimKeyframe> > &translationChannels,
		std::vector<float> &staticTranslations,
		std::vector<AnimMessage> &messages,
		float &locomotionSpeed,
		std::vector<LocomotionTranslationKey> &locoTransKeys,
		std::vector<LocomotionRotationKey> &locoRotKeys)
	{
		iff.enterForm(TAG_KFAT);
		iff.enterForm(TAG_0003);

		//-- INFO
		int32 transformCount          = 0;
		int32 rotationChannelCount    = 0;
		int32 staticRotationCount     = 0;
		int32 translationChannelCount = 0;
		int32 staticTranslationCount  = 0;

		iff.enterChunk(TAG_INFO);
		{
			fps                     = iff.read_float();
			frameCount              = iff.read_int32();
			transformCount          = iff.read_int32();
			rotationChannelCount    = iff.read_int32();
			staticRotationCount     = iff.read_int32();
			translationChannelCount = iff.read_int32();
			staticTranslationCount  = iff.read_int32();
		}
		iff.exitChunk(TAG_INFO);

		MESSENGER_LOG(("  KFAT INFO: fps=%.1f frames=%d transforms=%d rotCh=%d staticRot=%d transCh=%d staticTrans=%d\n",
			fps, frameCount, transformCount, rotationChannelCount, staticRotationCount, translationChannelCount, staticTranslationCount));

		//-- XFRM
		transforms.resize(static_cast<size_t>(transformCount));
		iff.enterForm(TAG_XFRM);
		{
			for (int t = 0; t < transformCount; ++t)
			{
				TransformInfo &ti = transforms[static_cast<size_t>(t)];
				iff.enterChunk(TAG_XFIN);
				{
					std::string name;
					iff.read_string(name);
					ti.name = name;
					ti.hasAnimatedRotation       = iff.read_int8();
					ti.rotationChannelIndex      = iff.read_uint32();
					ti.translationMask           = iff.read_uint32();
					ti.xTranslationChannelIndex  = iff.read_uint32();
					ti.yTranslationChannelIndex  = iff.read_uint32();
					ti.zTranslationChannelIndex  = iff.read_uint32();
				}
				iff.exitChunk(TAG_XFIN);
			}
		}
		iff.exitForm(TAG_XFRM);

		//-- AROT
		rotationChannels.resize(static_cast<size_t>(rotationChannelCount));
		iff.enterForm(TAG_AROT);
		{
			for (int rc = 0; rc < rotationChannelCount; ++rc)
			{
				iff.enterChunk(TAG_QCHN);
				{
					const int32 keyframeCount = iff.read_int32();
					std::vector<MayaSceneBuilder::QuatKeyframe> &channel = rotationChannels[static_cast<size_t>(rc)];
					channel.resize(static_cast<size_t>(keyframeCount));

					for (int32 k = 0; k < keyframeCount; ++k)
					{
						MayaSceneBuilder::QuatKeyframe &qk = channel[static_cast<size_t>(k)];
						qk.frame = static_cast<int>(iff.read_int32());
						const float w = iff.read_float();
						const float x = iff.read_float();
						const float y = iff.read_float();
						const float z = iff.read_float();
						qk.rotation[0] = x;
						qk.rotation[1] = y;
						qk.rotation[2] = z;
						qk.rotation[3] = w;
					}
				}
				iff.exitChunk(TAG_QCHN);
			}
		}
		iff.exitForm(TAG_AROT);

		//-- SROT
		staticRotations.resize(static_cast<size_t>(staticRotationCount) * 4);
		iff.enterChunk(TAG_SROT);
		{
			for (int32 sr = 0; sr < staticRotationCount; ++sr)
			{
				const size_t base = static_cast<size_t>(sr) * 4;
				const float w = iff.read_float();
				const float x = iff.read_float();
				const float y = iff.read_float();
				const float z = iff.read_float();
				staticRotations[base + 0] = x;
				staticRotations[base + 1] = y;
				staticRotations[base + 2] = z;
				staticRotations[base + 3] = w;
			}
		}
		iff.exitChunk(TAG_SROT);

		//-- ATRN
		translationChannels.resize(static_cast<size_t>(translationChannelCount));
		iff.enterForm(TAG_ATRN);
		{
			for (int tc = 0; tc < translationChannelCount; ++tc)
			{
				iff.enterChunk(TAG_CHNL);
				{
					const int32 keyframeCount = iff.read_int32();
					std::vector<MayaSceneBuilder::AnimKeyframe> &channel = translationChannels[static_cast<size_t>(tc)];
					channel.resize(static_cast<size_t>(keyframeCount));

					for (int32 k = 0; k < keyframeCount; ++k)
					{
						MayaSceneBuilder::AnimKeyframe &ak = channel[static_cast<size_t>(k)];
						ak.frame = static_cast<int>(iff.read_int32());
						ak.value = iff.read_float();
					}
				}
				iff.exitChunk(TAG_CHNL);
			}
		}
		iff.exitForm(TAG_ATRN);

		//-- STRN
		staticTranslations.resize(static_cast<size_t>(staticTranslationCount));
		iff.enterChunk(TAG_STRN);
		{
			for (int32 st = 0; st < staticTranslationCount; ++st)
				staticTranslations[static_cast<size_t>(st)] = iff.read_float();
		}
		iff.exitChunk(TAG_STRN);

		//-- optional MSGS, LOCT, LOCR
		while (!iff.atEndOfForm())
		{
			if (iff.isCurrentForm())
			{
				const Tag formTag = iff.getCurrentName();

				if (formTag == TAG_MSGS)
				{
					iff.enterForm(TAG_MSGS);

					iff.enterChunk(TAG_INFO);
					const int16 messageCount = iff.read_int16();
					iff.exitChunk(TAG_INFO);

					for (int16 mi = 0; mi < messageCount; ++mi)
					{
						iff.enterChunk(TAG_MESG);
						{
							AnimMessage msg;
							const int16 signalFrameCount = iff.read_int16();

							std::string msgName;
							iff.read_string(msgName);
							msg.name = msgName;

							for (int16 si = 0; si < signalFrameCount; ++si)
								msg.signalFrames.push_back(iff.read_int16());

							messages.push_back(msg);
						}
						iff.exitChunk(TAG_MESG);
					}

					iff.exitForm(TAG_MSGS);
				}
				else
				{
					iff.enterForm();
					iff.exitForm();
				}
			}
			else
			{
				const Tag chunkTag = iff.getCurrentName();

				if (chunkTag == TAG_LOCT)
				{
					iff.enterChunk(TAG_LOCT);
					{
						locomotionSpeed = iff.read_float();
						const int16 keyCount = iff.read_int16();
						for (int16 ki = 0; ki < keyCount; ++ki)
						{
							LocomotionTranslationKey ltk;
							ltk.frame = iff.read_int16();
							ltk.x     = iff.read_float();
							ltk.y     = iff.read_float();
							ltk.z     = iff.read_float();
							locoTransKeys.push_back(ltk);
						}
					}
					iff.exitChunk(TAG_LOCT);
				}
				else if (chunkTag == TAG_LOCR)
				{
					iff.enterChunk(TAG_LOCR);
					{
						const int16 keyCount = iff.read_int16();
						for (int16 ki = 0; ki < keyCount; ++ki)
						{
							LocomotionRotationKey lrk;
							lrk.frame = iff.read_int16();
							const float w = iff.read_float();
							const float x = iff.read_float();
							const float y = iff.read_float();
							const float z = iff.read_float();
							lrk.qx = x;
							lrk.qy = y;
							lrk.qz = z;
							lrk.qw = w;
							locoRotKeys.push_back(lrk);
						}
					}
					iff.exitChunk(TAG_LOCR);
				}
				else
				{
					iff.enterChunk();
					iff.exitChunk();
				}
			}
		}

		iff.exitForm(TAG_0003);
		iff.exitForm(TAG_KFAT);

		return true;
	}

	// ------------------------------------------------------------------

	bool readCompressedAnimation(
		Iff &iff,
		float &fps,
		int32 &frameCount,
		std::vector<TransformInfo> &transforms,
		std::vector< std::vector<MayaSceneBuilder::QuatKeyframe> > &rotationChannels,
		std::vector<float> &staticRotations,
		std::vector< std::vector<MayaSceneBuilder::AnimKeyframe> > &translationChannels,
		std::vector<float> &staticTranslations,
		std::vector<AnimMessage> &messages,
		float &locomotionSpeed,
		std::vector<LocomotionTranslationKey> &locoTransKeys,
		std::vector<LocomotionRotationKey> &locoRotKeys)
	{
		iff.enterForm(TAG_CKAT);
		iff.enterForm(TAG_0001);

		//-- INFO (compressed uses int16 for counts)
		int16 transformCount16          = 0;
		int16 rotationChannelCount16    = 0;
		int16 staticRotationCount16     = 0;
		int16 translationChannelCount16 = 0;
		int16 staticTranslationCount16  = 0;

		iff.enterChunk(TAG_INFO);
		{
			fps                       = iff.read_float();
			frameCount                = static_cast<int32>(iff.read_int16());
			transformCount16          = iff.read_int16();
			rotationChannelCount16    = iff.read_int16();
			staticRotationCount16     = iff.read_int16();
			translationChannelCount16 = iff.read_int16();
			staticTranslationCount16  = iff.read_int16();
		}
		iff.exitChunk(TAG_INFO);

		const int transformCount          = static_cast<int>(transformCount16);
		const int rotationChannelCount    = static_cast<int>(rotationChannelCount16);
		const int staticRotationCount     = static_cast<int>(staticRotationCount16);
		const int translationChannelCount = static_cast<int>(translationChannelCount16);
		const int staticTranslationCount  = static_cast<int>(staticTranslationCount16);

		MESSENGER_LOG(("  CKAT INFO: fps=%.1f frames=%d transforms=%d rotCh=%d staticRot=%d transCh=%d staticTrans=%d\n",
			fps, frameCount, transformCount, rotationChannelCount, staticRotationCount, translationChannelCount, staticTranslationCount));

		//-- XFRM (compressed uses int16/uint8/uint16 for indices)
		transforms.resize(static_cast<size_t>(transformCount));
		iff.enterForm(TAG_XFRM);
		{
			for (int t = 0; t < transformCount; ++t)
			{
				TransformInfo &ti = transforms[static_cast<size_t>(t)];
				iff.enterChunk(TAG_XFIN);
				{
					std::string name;
					iff.read_string(name);
					ti.name = name;
					ti.hasAnimatedRotation       = iff.read_int8();
					ti.rotationChannelIndex      = static_cast<uint32>(iff.read_int16());
					ti.translationMask           = static_cast<uint32>(iff.read_uint8());
					ti.xTranslationChannelIndex  = static_cast<uint32>(iff.read_uint16());
					ti.yTranslationChannelIndex  = static_cast<uint32>(iff.read_uint16());
					ti.zTranslationChannelIndex  = static_cast<uint32>(iff.read_uint16());
				}
				iff.exitChunk(TAG_XFIN);
			}
		}
		iff.exitForm(TAG_XFRM);

		//-- AROT (compressed quaternions)
		rotationChannels.resize(static_cast<size_t>(rotationChannelCount));
		iff.enterForm(TAG_AROT);
		{
			for (int rc = 0; rc < rotationChannelCount; ++rc)
			{
				iff.enterChunk(TAG_QCHN);
				{
					const int16 keyframeCount = iff.read_int16();
					const uint8 xFormat = iff.read_uint8();
					const uint8 yFormat = iff.read_uint8();
					const uint8 zFormat = iff.read_uint8();

					std::vector<MayaSceneBuilder::QuatKeyframe> &channel = rotationChannels[static_cast<size_t>(rc)];
					channel.resize(static_cast<size_t>(keyframeCount));

					for (int16 k = 0; k < keyframeCount; ++k)
					{
						MayaSceneBuilder::QuatKeyframe &qk = channel[static_cast<size_t>(k)];
						qk.frame = static_cast<int>(iff.read_int16());

						const uint32 compressedValue = iff.read_uint32();
						CompressedQuaternion cq(compressedValue);
						float w, x, y, z;
						cq.expand(xFormat, yFormat, zFormat, w, x, y, z);

						qk.rotation[0] = x;
						qk.rotation[1] = y;
						qk.rotation[2] = z;
						qk.rotation[3] = w;
					}
				}
				iff.exitChunk(TAG_QCHN);
			}
		}
		iff.exitForm(TAG_AROT);

		//-- SROT (compressed static rotations)
		staticRotations.resize(static_cast<size_t>(staticRotationCount) * 4);
		iff.enterChunk(TAG_SROT);
		{
			for (int sr = 0; sr < staticRotationCount; ++sr)
			{
				const uint8 xFormat = iff.read_uint8();
				const uint8 yFormat = iff.read_uint8();
				const uint8 zFormat = iff.read_uint8();
				const uint32 compressedValue = iff.read_uint32();

				CompressedQuaternion cq(compressedValue);
				float w, x, y, z;
				cq.expand(xFormat, yFormat, zFormat, w, x, y, z);

				const size_t base = static_cast<size_t>(sr) * 4;
				staticRotations[base + 0] = x;
				staticRotations[base + 1] = y;
				staticRotations[base + 2] = z;
				staticRotations[base + 3] = w;
			}
		}
		iff.exitChunk(TAG_SROT);

		//-- ATRN (compressed uses int16 frame numbers)
		translationChannels.resize(static_cast<size_t>(translationChannelCount));
		iff.enterForm(TAG_ATRN);
		{
			for (int tc = 0; tc < translationChannelCount; ++tc)
			{
				iff.enterChunk(TAG_CHNL);
				{
					const int16 keyframeCount = iff.read_int16();
					std::vector<MayaSceneBuilder::AnimKeyframe> &channel = translationChannels[static_cast<size_t>(tc)];
					channel.resize(static_cast<size_t>(keyframeCount));

					for (int16 k = 0; k < keyframeCount; ++k)
					{
						MayaSceneBuilder::AnimKeyframe &ak = channel[static_cast<size_t>(k)];
						ak.frame = static_cast<int>(iff.read_int16());
						ak.value = iff.read_float();
					}
				}
				iff.exitChunk(TAG_CHNL);
			}
		}
		iff.exitForm(TAG_ATRN);

		//-- STRN
		staticTranslations.resize(static_cast<size_t>(staticTranslationCount));
		iff.enterChunk(TAG_STRN);
		{
			for (int st = 0; st < staticTranslationCount; ++st)
				staticTranslations[static_cast<size_t>(st)] = iff.read_float();
		}
		iff.exitChunk(TAG_STRN);

		//-- optional MSGS, LOCT, locomotion rotation (QCHN)
		while (!iff.atEndOfForm())
		{
			if (iff.isCurrentForm())
			{
				const Tag formTag = iff.getCurrentName();

				if (formTag == TAG_MSGS)
				{
					iff.enterForm(TAG_MSGS);

					iff.enterChunk(TAG_INFO);
					const int16 messageCount = iff.read_int16();
					iff.exitChunk(TAG_INFO);

					for (int16 mi = 0; mi < messageCount; ++mi)
					{
						iff.enterChunk(TAG_MESG);
						{
							AnimMessage msg;
							const int16 signalFrameCount = iff.read_int16();

							std::string msgName;
							iff.read_string(msgName);
							msg.name = msgName;

							for (int16 si = 0; si < signalFrameCount; ++si)
								msg.signalFrames.push_back(iff.read_int16());

							messages.push_back(msg);
						}
						iff.exitChunk(TAG_MESG);
					}

					iff.exitForm(TAG_MSGS);
				}
				else
				{
					iff.enterForm();
					iff.exitForm();
				}
			}
			else
			{
				const Tag chunkTag = iff.getCurrentName();

				if (chunkTag == TAG_LOCT)
				{
					iff.enterChunk(TAG_LOCT);
					{
						locomotionSpeed = iff.read_float();
						const int16 keyCount = iff.read_int16();
						for (int16 ki = 0; ki < keyCount; ++ki)
						{
							LocomotionTranslationKey ltk;
							ltk.frame = iff.read_int16();
							ltk.x     = iff.read_float();
							ltk.y     = iff.read_float();
							ltk.z     = iff.read_float();
							locoTransKeys.push_back(ltk);
						}
					}
					iff.exitChunk(TAG_LOCT);
				}
				else if (chunkTag == TAG_QCHN)
				{
					// Compressed locomotion rotation stored as QCHN chunk
					iff.enterChunk(TAG_QCHN);
					{
						const int16 keyCount = iff.read_int16();
						const uint8 xFormat = iff.read_uint8();
						const uint8 yFormat = iff.read_uint8();
						const uint8 zFormat = iff.read_uint8();

						for (int16 ki = 0; ki < keyCount; ++ki)
						{
							LocomotionRotationKey lrk;
							lrk.frame = iff.read_int16();
							const uint32 compressedValue = iff.read_uint32();
							CompressedQuaternion cq(compressedValue);
							float w, x, y, z;
							cq.expand(xFormat, yFormat, zFormat, w, x, y, z);
							lrk.qx = x;
							lrk.qy = y;
							lrk.qz = z;
							lrk.qw = w;
							locoRotKeys.push_back(lrk);
						}
					}
					iff.exitChunk(TAG_QCHN);
				}
				else
				{
					iff.enterChunk();
					iff.exitChunk();
				}
			}
		}

		iff.exitForm(TAG_0001);
		iff.exitForm(TAG_CKAT);

		return true;
	}
}

// ======================================================================

void ImportAnimation::install(Messenger *newMessenger)
{
	messenger = newMessenger;
}

// ----------------------------------------------------------------------

void ImportAnimation::remove()
{
	messenger = 0;
}

// ----------------------------------------------------------------------

void *ImportAnimation::creator()
{
	return new ImportAnimation();
}

// ======================================================================

ImportAnimation::ImportAnimation()
{
}

// ======================================================================

MStatus ImportAnimation::doIt(const MArgList &args)
{
	MStatus status;

	//-- parse arguments: -i <filename> [-skeleton <name>]
	const unsigned argCount = args.length(&status);
	MESSENGER_REJECT_STATUS(!status, ("failed to get argument count\n"));
	MESSENGER_REJECT_STATUS(argCount < 2, ("usage: importAnimation -i <filename> [-skeleton <name>]\n"));

	std::string filename;
	std::string skeletonFilter;

	for (unsigned i = 0; i < argCount; ++i)
	{
		MString arg = args.asString(i, &status);
		MESSENGER_REJECT_STATUS(!status, ("failed to get argument %u\n", i));

		if (arg == "-i" && (i + 1) < argCount)
		{
			MString val = args.asString(i + 1, &status);
			MESSENGER_REJECT_STATUS(!status, ("failed to get filename argument\n"));
			filename = val.asChar();
			++i;
		}
		else if (arg == "-skeleton" && (i + 1) < argCount)
		{
			MString val = args.asString(i + 1, &status);
			MESSENGER_REJECT_STATUS(!status, ("failed to get skeleton argument\n"));
			skeletonFilter = val.asChar();
			++i;
		}
	}

	MESSENGER_REJECT_STATUS(filename.empty(), ("no filename specified, use -i <filename>\n"));

	filename = resolveImportPath(filename);
	MESSENGER_LOG(("ImportAnimation: opening [%s]\n", filename.c_str()));

	//-- open the IFF file
	Iff iff(filename.c_str());

	float fps = 30.0f;
	int32 frameCount = 0;
	std::vector<TransformInfo> transforms;
	std::vector< std::vector<MayaSceneBuilder::QuatKeyframe> > rotationChannels;
	std::vector<float> staticRotations;
	std::vector< std::vector<MayaSceneBuilder::AnimKeyframe> > translationChannels;
	std::vector<float> staticTranslations;
	std::vector<AnimMessage> messages;
	float locomotionSpeed = 0.0f;
	std::vector<LocomotionTranslationKey> locoTransKeys;
	std::vector<LocomotionRotationKey> locoRotKeys;

	const Tag topTag = iff.getCurrentName();

	if (topTag == TAG_KFAT)
	{
		if (!readUncompressedAnimation(iff, fps, frameCount, transforms, rotationChannels, staticRotations, translationChannels, staticTranslations, messages, locomotionSpeed, locoTransKeys, locoRotKeys))
			return MS::kFailure;
	}
	else if (topTag == TAG_CKAT)
	{
		if (!readCompressedAnimation(iff, fps, frameCount, transforms, rotationChannels, staticRotations, translationChannels, staticTranslations, messages, locomotionSpeed, locoTransKeys, locoRotKeys))
			return MS::kFailure;
	}
	else
	{
		MESSENGER_LOG_ERROR(("ImportAnimation: unrecognized top-level form tag in [%s]\n", filename.c_str()));
		return MS::kFailure;
	}

	const int rotationChannelCount    = static_cast<int>(rotationChannels.size());
	const int staticRotationCount     = static_cast<int>(staticRotations.size()) / 4;
	const int translationChannelCount = static_cast<int>(translationChannels.size());
	const int staticTranslationCount  = static_cast<int>(staticTranslations.size());
	const int transformCount          = static_cast<int>(transforms.size());

	//-- ANS format: quaternions (w,x,y,z) in IFF -> Maya (x,y,z,w). Rotations: negate Y,Z on import.
	//   Translations: negate X for translateX. See MayaConversions and setRotationKeyframes.
	MESSENGER_LOG(("ImportAnimation: ANS %d transforms, %d rotCh, %d transCh, fps=%.1f frames=%d\n",
		transformCount, rotationChannelCount, translationChannelCount, fps, frameCount));

	//-- build set of skin cluster influence paths (joints the mesh is bound to)
	std::set<std::string> skinInfluencePaths;
	{
		MItDependencyNodes dnIt(MFn::kSkinClusterFilter, &status);
		if (status)
		{
			for (; !dnIt.isDone(); dnIt.next())
			{
				MObject skinObj = dnIt.thisNode();
				MFnSkinCluster skinFn(skinObj, &status);
				if (!status) continue;
				MDagPathArray influencePaths;
				skinFn.influenceObjects(influencePaths, &status);
				if (!status) continue;
				for (unsigned int i = 0; i < influencePaths.length(); ++i)
					skinInfluencePaths.insert(influencePaths[i].fullPathName().asChar());
			}
		}
		if (!skinInfluencePaths.empty())
			MESSENGER_LOG(("ImportAnimation: skin influences: %u joints (restricting to these)\n",
				static_cast<unsigned>(skinInfluencePaths.size())));
	}

	//-- build joint name -> DAG path; apply -skeleton filter and/or skin influence filter
	// ANS stores transform names as short names (e.g. "lthigh", "root") - same as mesh XFNM.
	// Use MayaCompoundString::getComponentString(0) to match exporter/ImportSkeletalMesh logic.
	std::map<std::string, MDagPath> jointMap;
	{
		std::string filter = skeletonFilter;
		if (!filter.empty())
		{
			const std::string::size_type ls = filter.find_last_of("/\\");
			if (ls != std::string::npos) filter = filter.substr(ls + 1);
			const std::string::size_type dot = filter.find_last_of('.');
			if (dot != std::string::npos) filter = filter.substr(0, dot);
			skeletonFilter = filter;
		}

		MItDag dagIt(MItDag::kDepthFirst, MFn::kJoint, &status);
		for (; !dagIt.isDone(); dagIt.next())
		{
			MDagPath dagPath;
			dagIt.getPath(dagPath);
			MFnDagNode dagFn(dagPath);
			const MString mayaName(dagFn.name());
			std::string pathStr(dagPath.fullPathName().asChar());

			if (!skeletonFilter.empty() && pathStr.find(skeletonFilter) == std::string::npos)
				continue;
			if (!skinInfluencePaths.empty() && skinInfluencePaths.find(pathStr) == skinInfluencePaths.end())
				continue;

			// Use same short name as exporter: getComponentString(0) from MayaCompoundString
			MayaCompoundString compoundName(mayaName);
			const MString gameTransformName = (compoundName.getComponentCount() > 0) ? compoundName.getComponentString(0) : mayaName;
			const std::string shortName(gameTransformName.asChar());

			if (jointMap.find(shortName) == jointMap.end())
				jointMap[shortName] = dagPath;
		}

		if (!skeletonFilter.empty())
			MESSENGER_LOG(("ImportAnimation: skeleton [%s] -> %u joints\n",
				skeletonFilter.c_str(), static_cast<unsigned>(jointMap.size())));
		else if (!skinInfluencePaths.empty())
		{
			MESSENGER_LOG(("ImportAnimation: skin-only -> %u joints\n", static_cast<unsigned>(jointMap.size())));
			if (jointMap.empty())
				MESSENGER_LOG_WARNING(("ImportAnimation: skin filter yielded 0 joints - animation names may not match skin influences\n"));
		}
		else
			MESSENGER_LOG(("ImportAnimation: all joints -> %u joints\n", static_cast<unsigned>(jointMap.size())));
	}

	int appliedCount = 0;
	int skippedCount = 0;
	std::string firstAnimatedPath;

	//-- apply animation data to matching joints
	for (int t = 0; t < transformCount; ++t)
	{
		const TransformInfo &ti = transforms[static_cast<size_t>(t)];

		std::map<std::string, MDagPath>::const_iterator it = jointMap.find(ti.name);
		if (it == jointMap.end())
		{
			MESSENGER_LOG(("  no matching joint for transform [%s], skipping\n", ti.name.c_str()));
			++skippedCount;
			continue;
		}

		const MDagPath &jointPath = it->second;
		if (firstAnimatedPath.empty())
			firstAnimatedPath = jointPath.fullPathName().asChar();
		MESSENGER_LOG(("  animating [%s] -> %s\n", ti.name.c_str(), jointPath.fullPathName().asChar()));

		// ANS stores deltas from bind pose. Read bind pose from joint (skeleton rest pose).
		MFnIkJoint jointFn(jointPath.node());
		MVector bindTranslation = jointFn.getTranslation(MSpace::kTransform);

		//-- animated rotation (delta from bind pose: final = delta * bindPose)
		if (ti.hasAnimatedRotation)
		{
			const uint32 idx = ti.rotationChannelIndex;
			if (idx < static_cast<uint32>(rotationChannelCount))
			{
				status = MayaSceneBuilder::setRotationKeyframesFromDeltas(jointPath, rotationChannels[idx], fps);
				if (!status)
					MESSENGER_LOG_WARNING(("  failed to set rotation keyframes on [%s]\n", ti.name.c_str()));
			}
		}
		else
		{
			// Static rotation: delta (identity = no change). final = delta * bindPose.
			const uint32 idx = ti.rotationChannelIndex;
			if (idx < static_cast<uint32>(staticRotationCount))
			{
				const size_t base = static_cast<size_t>(idx) * 4;
				MQuaternion deltaQ(
					staticRotations[base + 0],
					staticRotations[base + 1],
					staticRotations[base + 2],
					staticRotations[base + 3]);
				MQuaternion bindQuat;
				jointFn.getRotation(bindQuat, MSpace::kTransform);
				MQuaternion finalQ = deltaQ * bindQuat;
				MEulerRotation finalEuler = finalQ.asEulerRotation();
				MEulerRotation corrected(finalEuler.x, -finalEuler.y, -finalEuler.z);
				jointFn.setRotation(corrected);
			}
		}

		//-- animated X translation (delta from bind pose: final = bindPose + delta, X negated)
		if (ti.translationMask & MASK_X)
		{
			const uint32 idx = ti.xTranslationChannelIndex;
			if (idx < static_cast<uint32>(translationChannelCount))
			{
				status = MayaSceneBuilder::setKeyframesFromDeltas(jointPath, "translateX",
					translationChannels[idx], fps, bindTranslation.x, true);
				if (!status)
					MESSENGER_LOG_WARNING(("  failed to set translateX keyframes on [%s]\n", ti.name.c_str()));
			}
		}
		else
		{
			const uint32 idx = ti.xTranslationChannelIndex;
			if (idx < static_cast<uint32>(staticTranslationCount))
			{
				MVector t = jointFn.getTranslation(MSpace::kTransform);
				t.x = bindTranslation.x + static_cast<double>(-staticTranslations[idx]);
				jointFn.setTranslation(t, MSpace::kTransform);
			}
		}

		//-- animated Y translation (delta from bind pose)
		if (ti.translationMask & MASK_Y)
		{
			const uint32 idx = ti.yTranslationChannelIndex;
			if (idx < static_cast<uint32>(translationChannelCount))
			{
				status = MayaSceneBuilder::setKeyframesFromDeltas(jointPath, "translateY",
					translationChannels[idx], fps, bindTranslation.y, false);
				if (!status)
					MESSENGER_LOG_WARNING(("  failed to set translateY keyframes on [%s]\n", ti.name.c_str()));
			}
		}
		else
		{
			const uint32 idx = ti.yTranslationChannelIndex;
			if (idx < static_cast<uint32>(staticTranslationCount))
			{
				MVector t = jointFn.getTranslation(MSpace::kTransform);
				t.y = bindTranslation.y + static_cast<double>(staticTranslations[idx]);
				jointFn.setTranslation(t, MSpace::kTransform);
			}
		}

		//-- animated Z translation (delta from bind pose)
		if (ti.translationMask & MASK_Z)
		{
			const uint32 idx = ti.zTranslationChannelIndex;
			if (idx < static_cast<uint32>(translationChannelCount))
			{
				status = MayaSceneBuilder::setKeyframesFromDeltas(jointPath, "translateZ",
					translationChannels[idx], fps, bindTranslation.z, false);
				if (!status)
					MESSENGER_LOG_WARNING(("  failed to set translateZ keyframes on [%s]\n", ti.name.c_str()));
			}
		}
		else
		{
			const uint32 idx = ti.zTranslationChannelIndex;
			if (idx < static_cast<uint32>(staticTranslationCount))
			{
				MVector t = jointFn.getTranslation(MSpace::kTransform);
				t.z = bindTranslation.z + static_cast<double>(staticTranslations[idx]);
				jointFn.setTranslation(t, MSpace::kTransform);
			}
		}

		++appliedCount;
	}

	// Summary: skeleton hierarchy of animated joints (compare with "Skin cluster: skeleton" from ImportSkeletalMesh)
	std::string animSkeletonHint;
	if (!firstAnimatedPath.empty())
	{
		std::string::size_type p = firstAnimatedPath.find('|', 1);
		animSkeletonHint = (p != std::string::npos) ? firstAnimatedPath.substr(0, p) : firstAnimatedPath;
	}
	MESSENGER_LOG(("ImportAnimation: done, applied %d skipped %d, skeleton [%s]\n",
		appliedCount, skippedCount, animSkeletonHint.c_str()));

	if (!messages.empty())
	{
		MESSENGER_LOG(("ImportAnimation: %d animation messages:\n", static_cast<int>(messages.size())));
		for (size_t mi = 0; mi < messages.size(); ++mi)
		{
			MESSENGER_LOG(("  [%s] (%d signal frames)\n",
				messages[mi].name.c_str(),
				static_cast<int>(messages[mi].signalFrames.size())));
		}
	}

	if (!locoTransKeys.empty())
		MESSENGER_LOG(("ImportAnimation: locomotion translation: speed=%.2f, %d keys\n", locomotionSpeed, static_cast<int>(locoTransKeys.size())));

	if (!locoRotKeys.empty())
		MESSENGER_LOG(("ImportAnimation: locomotion rotation: %d keys\n", static_cast<int>(locoRotKeys.size())));

	return MStatus(MStatus::kSuccess);
}

// ======================================================================
