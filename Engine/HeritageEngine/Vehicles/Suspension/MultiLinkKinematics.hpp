#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>

namespace heritage::vehicles::suspension
{

// SUSP14 standalone geometry owner. Runtime integration should adapt Heritage's
// Vec3/Quaternion at the boundary; this file intentionally owns no engine-wide math type.
struct MultiLinkVec3
{
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

inline MultiLinkVec3 operator+(const MultiLinkVec3& a, const MultiLinkVec3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline MultiLinkVec3 operator-(const MultiLinkVec3& a, const MultiLinkVec3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline MultiLinkVec3 operator*(const MultiLinkVec3& v, double s)
{
    return {v.x * s, v.y * s, v.z * s};
}
inline MultiLinkVec3 operator/(const MultiLinkVec3& v, double s)
{
    return {v.x / s, v.y / s, v.z / s};
}
inline double dot(const MultiLinkVec3& a, const MultiLinkVec3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
inline MultiLinkVec3 cross(const MultiLinkVec3& a, const MultiLinkVec3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x};
}
inline double lengthSquared(const MultiLinkVec3& v) { return dot(v, v); }
inline double length(const MultiLinkVec3& v) { return std::sqrt(lengthSquared(v)); }
inline MultiLinkVec3 normalized(const MultiLinkVec3& v)
{
    const double l = length(v);
    return l > 1.0e-12 ? v / l : MultiLinkVec3{};
}

struct MultiLinkQuat
{
    double w = 1.0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

inline MultiLinkQuat normalized(const MultiLinkQuat& q)
{
    const double l = std::sqrt(q.w*q.w + q.x*q.x + q.y*q.y + q.z*q.z);
    if (l <= 1.0e-15) return {};
    return {q.w/l, q.x/l, q.y/l, q.z/l};
}
inline MultiLinkQuat operator*(const MultiLinkQuat& a, const MultiLinkQuat& b)
{
    return {
        a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z,
        a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y,
        a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x,
        a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w};
}
inline MultiLinkVec3 rotate(const MultiLinkQuat& qIn, const MultiLinkVec3& v)
{
    const MultiLinkQuat q = normalized(qIn);
    const MultiLinkVec3 u{q.x, q.y, q.z};
    const double s = q.w;
    return u * (2.0 * dot(u, v))
        + v * (s*s - dot(u, u))
        + cross(u, v) * (2.0 * s);
}
inline MultiLinkQuat worldRotationDelta(const MultiLinkVec3& rv)
{
    const double a = length(rv);
    if (a < 1.0e-12)
        return normalized(MultiLinkQuat{1.0, rv.x*0.5, rv.y*0.5, rv.z*0.5});
    const double h = 0.5 * a;
    const double k = std::sin(h) / a;
    return {std::cos(h), rv.x*k, rv.y*k, rv.z*k};
}

struct MultiLinkConstraint
{
    // Fixed chassis-side spherical joint in vehicle/chassis local space.
    MultiLinkVec3 chassisPoint{};
    // Matching joint in upright-local coordinates relative to wheel centre.
    MultiLinkVec3 uprightPoint{};
    // Rest rod length. <=0 means derive it from the neutral pose.
    double lengthMetres = 0.0;
    // Steering/rack/actuator motion can translate the inboard point per solve.
    MultiLinkVec3 chassisPointOffset{};
    double weight = 1.0;
};

struct MultiLinkDescription
{
    static constexpr std::size_t MaxConstraints = 8;
    MultiLinkVec3 restWheelCentre{};
    MultiLinkQuat restUprightOrientation{};
    MultiLinkVec3 travelAxis{0.0, 1.0, 0.0};
    std::array<MultiLinkConstraint, MaxConstraints> constraints{};
    std::size_t constraintCount = 0;
    int maxIterations = 10;
    double convergenceToleranceMetres = 2.5e-7;
    double normalEquationDamping = 1.0e-10;
    double maximumCorrectionStepMetres = 0.025;
    double maximumRotationStepRadians = 0.08;
};

struct MultiLinkState
{
    MultiLinkVec3 wheelCentre{};
    MultiLinkQuat uprightOrientation{};
    MultiLinkVec3 wheelCentreScrub{};
    double maximumLinkErrorMetres = 0.0;
    double rmsLinkErrorMetres = 0.0;
    int iterations = 0;
    bool converged = false;
};

class MultiLinkKinematics
{
public:
    static bool initialize(MultiLinkDescription& d)
    {
        if (d.constraintCount < 5 || d.constraintCount > MultiLinkDescription::MaxConstraints)
            return false;
        d.travelAxis = normalized(d.travelAxis);
        if (lengthSquared(d.travelAxis) < 0.5)
            return false;
        d.restUprightOrientation = normalized(d.restUprightOrientation);
        for (std::size_t i = 0; i < d.constraintCount; ++i)
        {
            auto& c = d.constraints[i];
            if (c.weight <= 0.0) c.weight = 1.0;
            if (c.lengthMetres <= 0.0)
            {
                const MultiLinkVec3 p = d.restWheelCentre
                    + rotate(d.restUprightOrientation, c.uprightPoint);
                c.lengthMetres = length(p - c.chassisPoint);
            }
            if (!(c.lengthMetres > 1.0e-6) || !std::isfinite(c.lengthMetres))
                return false;
        }
        return true;
    }

    static MultiLinkState solve(
        const MultiLinkDescription& d,
        double requestedTravelMetres,
        const MultiLinkState* warmStart = nullptr)
    {
        MultiLinkState out;
        if (d.constraintCount < 5 || d.constraintCount > MultiLinkDescription::MaxConstraints)
            return out;

        MultiLinkVec3 axis = normalized(d.travelAxis);
        MultiLinkVec3 seed = std::abs(axis.y) < 0.9 ? MultiLinkVec3{0.0,1.0,0.0} : MultiLinkVec3{1.0,0.0,0.0};
        MultiLinkVec3 t1 = normalized(cross(axis, seed));
        MultiLinkVec3 t2 = normalized(cross(axis, t1));
        const MultiLinkVec3 nominal = d.restWheelCentre + axis * requestedTravelMetres;

        double u = 0.0;
        double v = 0.0;
        MultiLinkQuat q = d.restUprightOrientation;
        if (warmStart && std::isfinite(warmStart->wheelCentre.x))
        {
            const MultiLinkVec3 offset = warmStart->wheelCentre - nominal;
            u = dot(offset, t1);
            v = dot(offset, t2);
            q = normalized(warmStart->uprightOrientation);
        }

        constexpr int N = 5;
        for (int iteration = 0; iteration < std::max(1, d.maxIterations); ++iteration)
        {
            double a[N][N]{};
            double b[N]{};
            double maxError = 0.0;
            double sumError2 = 0.0;
            const MultiLinkVec3 centre = nominal + t1*u + t2*v;

            for (std::size_t i = 0; i < d.constraintCount; ++i)
            {
                const auto& c = d.constraints[i];
                const MultiLinkVec3 rb = rotate(q, c.uprightPoint);
                const MultiLinkVec3 worldPoint = centre + rb;
                const MultiLinkVec3 chassis = c.chassisPoint + c.chassisPointOffset;
                const MultiLinkVec3 rod = worldPoint - chassis;
                const double rodLength = std::max(length(rod), 1.0e-12);
                const MultiLinkVec3 n = rod / rodLength;
                const double residual = rodLength - c.lengthMetres;
                const double w = c.weight;
                maxError = std::max(maxError, std::abs(residual));
                sumError2 += residual * residual;

                // For a world-space small rotation dtheta: dp = dtheta x rb.
                // n dot (dtheta x rb) = dtheta dot (rb x n).
                const MultiLinkVec3 rotGrad = cross(rb, n);
                const double j[N] = {
                    dot(n, t1),
                    dot(n, t2),
                    rotGrad.x,
                    rotGrad.y,
                    rotGrad.z};
                for (int r = 0; r < N; ++r)
                {
                    b[r] += w * j[r] * residual;
                    for (int cidx = 0; cidx < N; ++cidx)
                        a[r][cidx] += w * j[r] * j[cidx];
                }
            }

            out.maximumLinkErrorMetres = maxError;
            out.rmsLinkErrorMetres = std::sqrt(sumError2 / static_cast<double>(d.constraintCount));
            out.iterations = iteration + 1;
            if (maxError <= std::max(1.0e-10, d.convergenceToleranceMetres))
            {
                out.converged = true;
                break;
            }

            for (int i = 0; i < N; ++i)
                a[i][i] += std::max(0.0, d.normalEquationDamping);

            double delta[N]{};
            if (!solve5x5(a, b, delta))
                break;
            for (double& dval : delta) dval = -dval;

            const double translationStep = std::hypot(delta[0], delta[1]);
            if (translationStep > d.maximumCorrectionStepMetres && translationStep > 0.0)
            {
                const double s = d.maximumCorrectionStepMetres / translationStep;
                delta[0] *= s; delta[1] *= s;
            }
            MultiLinkVec3 rv{delta[2], delta[3], delta[4]};
            const double rotationStep = length(rv);
            if (rotationStep > d.maximumRotationStepRadians && rotationStep > 0.0)
                rv = rv * (d.maximumRotationStepRadians / rotationStep);

            u += delta[0];
            v += delta[1];
            q = normalized(worldRotationDelta(rv) * q);
        }

        out.wheelCentre = nominal + t1*u + t2*v;
        out.uprightOrientation = q;
        out.wheelCentreScrub = out.wheelCentre - nominal;

        // Recompute final closure error after the last accepted Newton step.
        double maxError = 0.0;
        double sumError2 = 0.0;
        for (std::size_t i = 0; i < d.constraintCount; ++i)
        {
            const auto& c = d.constraints[i];
            const MultiLinkVec3 p = out.wheelCentre + rotate(q, c.uprightPoint);
            const double e = length(p - (c.chassisPoint + c.chassisPointOffset)) - c.lengthMetres;
            maxError = std::max(maxError, std::abs(e));
            sumError2 += e*e;
        }
        out.maximumLinkErrorMetres = maxError;
        out.rmsLinkErrorMetres = std::sqrt(sumError2 / static_cast<double>(d.constraintCount));
        out.converged = maxError <= std::max(1.0e-10, d.convergenceToleranceMetres);
        return out;
    }

private:
    static bool solve5x5(double a[5][5], const double bIn[5], double x[5])
    {
        double m[5][6]{};
        for (int r = 0; r < 5; ++r)
        {
            for (int c = 0; c < 5; ++c) m[r][c] = a[r][c];
            m[r][5] = bIn[r];
        }
        for (int col = 0; col < 5; ++col)
        {
            int pivot = col;
            for (int r = col + 1; r < 5; ++r)
                if (std::abs(m[r][col]) > std::abs(m[pivot][col])) pivot = r;
            if (std::abs(m[pivot][col]) < 1.0e-14) return false;
            if (pivot != col)
                for (int c = col; c < 6; ++c) std::swap(m[pivot][c], m[col][c]);
            const double inv = 1.0 / m[col][col];
            for (int c = col; c < 6; ++c) m[col][c] *= inv;
            for (int r = 0; r < 5; ++r)
            {
                if (r == col) continue;
                const double f = m[r][col];
                for (int c = col; c < 6; ++c) m[r][c] -= f * m[col][c];
            }
        }
        for (int i = 0; i < 5; ++i) x[i] = m[i][5];
        return true;
    }
};

} // namespace heritage::vehicles::suspension
