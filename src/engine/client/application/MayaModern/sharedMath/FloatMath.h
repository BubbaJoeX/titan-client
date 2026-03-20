// ======================================================================
//
// FloatMath.h
// Portions Copyright 1998 Bootprint Entertainment
// Portions Copyright 2002 Sony Online Entertainment, Inc.
// All Rights Reserved.
//
// ======================================================================

#ifndef INCLUDED_FloatMath_H
#define INCLUDED_FloatMath_H

#include "Globals.h"
#include "Misc.h"

#include <corecrt_math.h>

// ======================================================================

float const PI          = 3.14159265358979323846f;
float const PI_TIMES_2  = PI * 2.0f;
float const PI_OVER_2   = PI / 2.0f;
float const PI_OVER_3   = PI / 3.0f;
float const PI_OVER_4   = PI / 4.0f;
float const PI_OVER_6   = PI / 6.0f;
float const PI_OVER_8   = PI / 8.0f;
float const PI_OVER_12  = PI / 12.0f;
float const PI_OVER_16  = PI / 16.0f;
float const PI_OVER_180 = PI / 180.0f;

float const E           = 2.7182818284590452f;

// ======================================================================

inline float convertDegreesToRadians (float degrees)
{
    return degrees * PI / 180.0f;
}

inline float convertRadiansToDegrees (float radians)
{
    return radians * 180.0f / PI;
}

inline float cot(float f)
{
    return RECIP(tanf(f));
}

inline float GaussianDistribution(float variate, float standardDeviation, float mean)
{
    // equation from http://mathworld.wolfram.com/NormalDistribution.html
    return (1.0f / (standardDeviation * sqrt(2.0f * PI))) * pow(E, -(sqr(variate - mean) / (2.0f * sqr(standardDeviation))));
}

inline float withinEpsilon(float const rhs, float const lhs, float const epsilon = 1.0e-3f)
{
    return ((lhs - epsilon) <= rhs) && (rhs <= (lhs + epsilon));
}

// ======================================================================

#endif