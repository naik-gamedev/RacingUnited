#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace heritage::vehicles::suspension
{
using SuspVec6 = std::array<double, 6>;
using SuspMat6 = std::array<std::array<double, 6>, 6>;

inline SuspMat6 suspZeroMat6()
{
    SuspMat6 m{};
    return m;
}

inline SuspMat6 suspDiagonalMat6(const SuspVec6& d)
{
    SuspMat6 m{};
    for (std::size_t i = 0; i < 6; ++i) m[i][i] = d[i];
    return m;
}

inline SuspVec6 suspMatVec(const SuspMat6& m, const SuspVec6& v)
{
    SuspVec6 out{};
    for (std::size_t r = 0; r < 6; ++r)
        for (std::size_t c = 0; c < 6; ++c)
            out[r] += m[r][c] * v[c];
    return out;
}

inline SuspVec6 suspAdd(const SuspVec6& a, const SuspVec6& b)
{
    SuspVec6 out{};
    for (std::size_t i = 0; i < 6; ++i) out[i] = a[i] + b[i];
    return out;
}

inline SuspVec6 suspSub(const SuspVec6& a, const SuspVec6& b)
{
    SuspVec6 out{};
    for (std::size_t i = 0; i < 6; ++i) out[i] = a[i] - b[i];
    return out;
}

inline SuspVec6 suspScale(const SuspVec6& a, double s)
{
    SuspVec6 out{};
    for (std::size_t i = 0; i < 6; ++i) out[i] = a[i] * s;
    return out;
}

inline bool suspFinite6(const SuspVec6& a)
{
    for (double v : a) if (!std::isfinite(v)) return false;
    return true;
}

inline bool suspSolveLinear6(SuspMat6 a, SuspVec6 b, SuspVec6& x)
{
    for (std::size_t col = 0; col < 6; ++col)
    {
        std::size_t pivot = col;
        double best = std::abs(a[col][col]);
        for (std::size_t r = col + 1; r < 6; ++r)
        {
            const double v = std::abs(a[r][col]);
            if (v > best) { best = v; pivot = r; }
        }
        if (best < 1.0e-18 || !std::isfinite(best)) return false;
        if (pivot != col)
        {
            std::swap(a[pivot], a[col]);
            std::swap(b[pivot], b[col]);
        }
        const double inv = 1.0 / a[col][col];
        for (std::size_t c = col; c < 6; ++c) a[col][c] *= inv;
        b[col] *= inv;
        for (std::size_t r = 0; r < 6; ++r)
        {
            if (r == col) continue;
            const double f = a[r][col];
            if (f == 0.0) continue;
            for (std::size_t c = col; c < 6; ++c) a[r][c] -= f * a[col][c];
            b[r] -= f * b[col];
        }
    }
    x = b;
    return suspFinite6(x);
}

inline std::uint64_t suspHash64(std::uint64_t h, const void* data, std::size_t n)
{
    const auto* bytes = static_cast<const unsigned char*>(data);
    constexpr std::uint64_t prime = 1099511628211ull;
    for (std::size_t i = 0; i < n; ++i) { h ^= bytes[i]; h *= prime; }
    return h;
}
}
