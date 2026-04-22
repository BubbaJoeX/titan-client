// ======================================================================
//
// SseMath.cpp
// Copyright 2002 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#include "sharedMath/FirstSharedMath.h"
#include "sharedMath/SseMath.h"

#include "sharedMath/Transform.h"
#include "sharedMath/Vector.h"

#include <intrin.h>

// ======================================================================

#ifndef _WIN64
#define SSE_ALIGN  __declspec(align(16))
#define SSE_VARIABLE_COUNT 5

// ======================================================================

namespace
{
	SSE_ALIGN float sseVariable[SSE_VARIABLE_COUNT][4];
}
#endif

// ======================================================================

bool SseMath::canDoSseMath()
{
#ifdef _WIN64
	int cpuInfo[4] = {0};
	__cpuid(cpuInfo, 1);
	const bool cpuHasSse         = (cpuInfo[3] & 0x02000000) != 0;
	const bool cpuHasSaveRestore = (cpuInfo[3] & 0x01000000) != 0;
	return cpuHasSse && cpuHasSaveRestore;
#else
	bool    cpuHasSse              = false;
	bool    cpuHasSaveRestore      = false;

	uint32  featureBits;

	try
	{
		__asm {
			mov    eax, 1
			cpuid
			mov    featureBits, edx
		}

		cpuHasSse              = ((featureBits      & 0x02000000) != 0);
		cpuHasSaveRestore      = ((featureBits      & 0x01000000) != 0);
	}
	catch (...)
    {
	}
	
	return cpuHasSse && cpuHasSaveRestore;
#endif
}

// ----------------------------------------------------------------------

Vector SseMath::rotateTranslateScale_l2p(const Transform &transform, const Vector &vector, float scale)
{
#ifdef _WIN64
	return transform.rotateTranslate_l2p(vector) * scale;
#else
	__asm {
		push    ebx
		mov     ebx, transform
		movaps  xmm0, [ebx + 0]
		movaps  xmm1, [ebx + 16]
		movaps  xmm2, [ebx + 32]
	}

	sseVariable[0][0] = vector.x;
	sseVariable[0][1] = vector.y;
	sseVariable[0][2] = vector.z;
	sseVariable[0][3] = 1.0f;

	__asm {
		mov     ebx, offset sseVariable
		movaps  xmm3, [ebx]
		movaps  xmm4, xmm3
		movaps  xmm5, xmm3
	}

	__asm {
		movss   xmm6, scale
		shufps  xmm6, xmm6, 0x00
		movlhps xmm6, xmm6

		mulps   xmm3, xmm0
		mulps   xmm4, xmm1
		mulps   xmm5, xmm2

		mulps   xmm3, xmm6
		mulps   xmm4, xmm6
		mulps   xmm5, xmm6

		movaps  [ebx + 32], xmm3
		movaps  [ebx + 48], xmm4
		movaps  [ebx + 64], xmm5

		pop     ebx
	}

	return Vector(
		sseVariable[2][0] + sseVariable[2][1] + sseVariable[2][2] + sseVariable[2][3],
		sseVariable[3][0] + sseVariable[3][1] + sseVariable[3][2] + sseVariable[3][3],
		sseVariable[4][0] + sseVariable[4][1] + sseVariable[4][2] + sseVariable[4][3]);
#endif
}

// ----------------------------------------------------------------------

Vector SseMath::rotateScale_l2p(const Transform &transform, const Vector &vector, float scale)
{
#ifdef _WIN64
	return transform.rotate_l2p(vector) * scale;
#else
	__asm {
		push    ebx
		mov     ebx, transform
		movaps  xmm0, [ebx + 0]
		movaps  xmm1, [ebx + 16]
		movaps  xmm2, [ebx + 32]
	}

	sseVariable[0][0] = vector.x;
	sseVariable[0][1] = vector.y;
	sseVariable[0][2] = vector.z;
	sseVariable[0][3] = 0.0f;

	__asm {
		mov     ebx, offset sseVariable
		movaps  xmm3, [ebx]
		movaps  xmm4, xmm3
		movaps  xmm5, xmm3
	}

	__asm {
		movss   xmm6, scale
		shufps  xmm6, xmm6, 0x00
		movlhps xmm6, xmm6

		mulps   xmm3, xmm0
		mulps   xmm4, xmm1
		mulps   xmm5, xmm2

		mulps   xmm3, xmm6
		mulps   xmm4, xmm6
		mulps   xmm5, xmm6

		movaps  [ebx + 32], xmm3
		movaps  [ebx + 48], xmm4
		movaps  [ebx + 64], xmm5

		pop     ebx
	}

	return Vector(
		sseVariable[2][0] + sseVariable[2][1] + sseVariable[2][2],
		sseVariable[3][0] + sseVariable[3][1] + sseVariable[3][2],
		sseVariable[4][0] + sseVariable[4][1] + sseVariable[4][2]);
#endif
}

// ----------------------------------------------------------------------

void SseMath::skinPositionNormal_l2p(const Transform &transform, const Vector &sourcePosition, const Vector &sourceNormal, float scale, Vector &destPosition, Vector &destNormal)
{
#ifdef _WIN64
	destPosition = transform.rotateTranslate_l2p(sourcePosition) * scale;
	destNormal   = transform.rotate_l2p(sourceNormal) * scale;
#else
	__asm {
		push    ebx
		mov     ebx, transform
		movaps  xmm0, [ebx + 0]
		movaps  xmm1, [ebx + 16]
		movaps  xmm2, [ebx + 32]
	}

	sseVariable[0][0] = sourcePosition.x;
	sseVariable[0][1] = sourcePosition.y;
	sseVariable[0][2] = sourcePosition.z;
	sseVariable[0][3] = 1.0f;

	__asm {
		mov     ebx, offset sseVariable
		movaps  xmm3, [ebx]
		movaps  xmm4, xmm3
		movaps  xmm5, xmm3
		movss   xmm6, scale
		shufps  xmm6, xmm6, 0x00
		movlhps xmm6, xmm6
		mulps   xmm3, xmm0
		mulps   xmm4, xmm1
		mulps   xmm5, xmm2
		mulps   xmm3, xmm6
		mulps   xmm4, xmm6
		mulps   xmm5, xmm6
		movaps  [ebx + 32], xmm3
		movaps  [ebx + 48], xmm4
		movaps  [ebx + 64], xmm5
	}

	destPosition.x = sseVariable[2][0] + sseVariable[2][1] + sseVariable[2][2] + sseVariable[2][3];
	destPosition.y = sseVariable[3][0] + sseVariable[3][1] + sseVariable[3][2] + sseVariable[3][3];
	destPosition.z = sseVariable[4][0] + sseVariable[4][1] + sseVariable[4][2] + sseVariable[4][3];

	sseVariable[0][0] = sourceNormal.x;
	sseVariable[0][1] = sourceNormal.y;
	sseVariable[0][2] = sourceNormal.z;
	sseVariable[0][3] = 1.0f;

	__asm {
		movaps  xmm3, [ebx]
		movaps  xmm4, xmm3
		movaps  xmm5, xmm3
		mulps   xmm3, xmm0
		mulps   xmm4, xmm1
		mulps   xmm5, xmm2
		mulps   xmm3, xmm6
		mulps   xmm4, xmm6
		mulps   xmm5, xmm6
		movaps  [ebx + 32], xmm3
		movaps  [ebx + 48], xmm4
		movaps  [ebx + 64], xmm5
		pop     ebx
	}

	destNormal.x = sseVariable[2][0] + sseVariable[2][1] + sseVariable[2][2];
	destNormal.y = sseVariable[3][0] + sseVariable[3][1] + sseVariable[3][2];
	destNormal.z = sseVariable[4][0] + sseVariable[4][1] + sseVariable[4][2];
#endif
}

// ----------------------------------------------------------------------

void SseMath::skinPositionNormalAdd_l2p(const Transform &transform, const Vector &sourcePosition, const Vector &sourceNormal, float scale, Vector &destPosition, Vector &destNormal)
{
#ifdef _WIN64
	destPosition += transform.rotateTranslate_l2p(sourcePosition) * scale;
	destNormal   += transform.rotate_l2p(sourceNormal) * scale;
#else
	__asm {
		push    ebx
		mov     ebx, transform
		movaps  xmm0, [ebx + 0]
		movaps  xmm1, [ebx + 16]
		movaps  xmm2, [ebx + 32]
	}

	sseVariable[0][0] = sourcePosition.x;
	sseVariable[0][1] = sourcePosition.y;
	sseVariable[0][2] = sourcePosition.z;
	sseVariable[0][3] = 1.0f;

	__asm {
		mov     ebx, offset sseVariable
		movaps  xmm3, [ebx]
		movaps  xmm4, xmm3
		movaps  xmm5, xmm3
		movss   xmm6, scale
		shufps  xmm6, xmm6, 0x00
		movlhps xmm6, xmm6
		mulps   xmm3, xmm0
		mulps   xmm4, xmm1
		mulps   xmm5, xmm2
		mulps   xmm3, xmm6
		mulps   xmm4, xmm6
		mulps   xmm5, xmm6
		movaps  [ebx + 32], xmm3
		movaps  [ebx + 48], xmm4
		movaps  [ebx + 64], xmm5
	}

	destPosition.x += sseVariable[2][0] + sseVariable[2][1] + sseVariable[2][2] + sseVariable[2][3];
	destPosition.y += sseVariable[3][0] + sseVariable[3][1] + sseVariable[3][2] + sseVariable[3][3];
	destPosition.z += sseVariable[4][0] + sseVariable[4][1] + sseVariable[4][2] + sseVariable[4][3];

	sseVariable[0][0] = sourceNormal.x;
	sseVariable[0][1] = sourceNormal.y;
	sseVariable[0][2] = sourceNormal.z;
	sseVariable[0][3] = 1.0f;

	__asm {
		movaps  xmm3, [ebx]
		movaps  xmm4, xmm3
		movaps  xmm5, xmm3
		mulps   xmm3, xmm0
		mulps   xmm4, xmm1
		mulps   xmm5, xmm2
		mulps   xmm3, xmm6
		mulps   xmm4, xmm6
		mulps   xmm5, xmm6
		movaps  [ebx + 32], xmm3
		movaps  [ebx + 48], xmm4
		movaps  [ebx + 64], xmm5
		pop     ebx
	}

	destNormal.x += sseVariable[2][0] + sseVariable[2][1] + sseVariable[2][2];
	destNormal.y += sseVariable[3][0] + sseVariable[3][1] + sseVariable[3][2];
	destNormal.z += sseVariable[4][0] + sseVariable[4][1] + sseVariable[4][2];
#endif
}

// ======================================================================
