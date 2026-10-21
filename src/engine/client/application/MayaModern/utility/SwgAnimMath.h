// ======================================================================
//
// SwgAnimMath.h
//
// Rotation conversions shared by the skeleton/animation import and export.
//
// A Maya joint built by importSkeleton holds rotate = mirror(engine bind rotation), rotateAxis = pre-rotation
// and jointOrient = post-rotation, so Maya composes the joint exactly like the engine's Skeleton
// (post * (anim * bind) * pre). Animation deltas therefore live in engine space:
//     engine rotate      = mirror(maya rotate)            (Maya X = -engine X)
//     animated rotate    = delta * bind                   (Hamilton product, applies bind first)
// A quaternion's components describe the same physical rotation in Maya and in the engine; only the
// X mirror differs: mirror(w, x, y, z) = (w, x, -y, -z).
//
// ======================================================================

#ifndef INCLUDED_SwgAnimMath_H
#define INCLUDED_SwgAnimMath_H

#include <maya/MEulerRotation.h>
#include <maya/MFnTransform.h>
#include <maya/MQuaternion.h>

#include <cmath>

namespace SwgAnimMath
{
    struct Quat
    {
        double w = 1.0, x = 0.0, y = 0.0, z = 0.0;

        Quat() = default;
        Quat(double w_, double x_, double y_, double z_) : w(w_), x(x_), y(y_), z(z_) {}

        Quat operator*(const Quat& b) const
        {
            return Quat(w * b.w - x * b.x - y * b.y - z * b.z,
                        w * b.x + x * b.w + y * b.z - z * b.y,
                        w * b.y - x * b.z + y * b.w + z * b.x,
                        w * b.z + x * b.y - y * b.x + z * b.w);
        }
        Quat conjugate() const { return Quat(w, -x, -y, -z); }
        Quat negated() const { return Quat(-w, -x, -y, -z); }
        double dot(const Quat& b) const { return w * b.w + x * b.x + y * b.y + z * b.z; }
        Quat normalized() const
        {
            const double n = std::sqrt(w * w + x * x + y * y + z * z);
            return n > 1e-12 ? Quat(w / n, x / n, y / n, z / n) : Quat();
        }
    };

    inline Quat mirror(const Quat& q) { return Quat(q.w, q.x, -q.y, -q.z); }

    /// A joint's rotation order as an MEulerRotation order. MFnTransform::rotationOrder() returns an
    /// MTransformationMatrix order (kInvalid = 0, kXYZ = 1, ...), which must not be cast to MEulerRotation's
    /// (kXYZ = 0, kYZX = 1, ...); the rotate value carries the right one.
    inline MEulerRotation::RotationOrder eulerOrderOf(const MFnTransform& transformFn)
    {
        MEulerRotation r;
        transformFn.getRotation(r);
        return r.order;
    }

    /// Engine-space rotation of a Maya rotate value (any rotation order).
    inline Quat engineFromMaya(const MEulerRotation& e)
    {
        const MQuaternion m = e.asQuaternion();
        return mirror(Quat(m.w, m.x, m.y, m.z)).normalized();
    }

    /// Maya rotate value for an engine-space rotation, in `order`, picked closest to `closest` when given
    /// (keeps Euler curves continuous between keys).
    inline MEulerRotation mayaFromEngine(const Quat& engine, MEulerRotation::RotationOrder order,
                                         const MEulerRotation* closest = nullptr)
    {
        const Quat m = mirror(engine).normalized();
        MEulerRotation e = MQuaternion(m.x, m.y, m.z, m.w).asEulerRotation();
        e.reorderIt(order);
        if (closest)
            e.setToClosestSolution(*closest);
        return e;
    }

    /// Angle (radians) between two rotations.
    inline double angleBetween(const Quat& a, const Quat& b)
    {
        const double d = std::fabs(a.normalized().dot(b.normalized()));
        return 2.0 * std::acos(d > 1.0 ? 1.0 : d);
    }

    /// Shortest-arc spherical interpolation - the engine's blend between two rotation keys
    /// (Quaternion::slerp, used by KeyframeSkeletalAnimation).
    inline Quat slerp(const Quat& a, const Quat& b, double t)
    {
        const Quat bb = a.dot(b) < 0.0 ? b.negated() : b;
        const double cosTheta = a.dot(bb);
        double c1 = 1.0 - t, c2 = t;
        if (1.0 - cosTheta > 1e-6)
        {
            const double theta = std::acos(cosTheta > 1.0 ? 1.0 : cosTheta);
            const double s = std::sin(theta);
            c1 = std::sin(theta - t * theta) / s;
            c2 = std::sin(t * theta) / s;
        }
        return Quat(c1 * a.w + c2 * bb.w, c1 * a.x + c2 * bb.x, c1 * a.y + c2 * bb.y, c1 * a.z + c2 * bb.z).normalized();
    }
}

#endif
