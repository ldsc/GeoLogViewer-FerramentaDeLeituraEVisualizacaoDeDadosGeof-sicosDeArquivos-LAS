#include "CLogCurveUtilities.h"
#include <algorithm>
#include <cmath>

namespace CLogCurveUtilities {

bool MinMax(const std::vector<double>& v, double& vmin, double& vmax)
{
    bool ok = false;
    vmin = +1e300;
    vmax = -1e300;
    for (double x : v) {
        if (std::isfinite(x)) {
            ok   = true;
            vmin = std::min(vmin, x);
            vmax = std::max(vmax, x);
        }
    }
    if (!ok) {
        vmin = 0.0;
        vmax = 0.0;
    }
    return ok;
}

double Mean(const std::vector<double>& v)
{
    double acc = 0.0;
    int    n   = 0;
    for (double x : v) {
        if (std::isfinite(x)) {
            acc += x;
            ++n;
        }
    }
    return (n > 0) ? acc / n : NAN;
}

void SmoothMA(std::vector<double>& v, int w)
{
    if (w <= 1 || v.empty())
        return;

    const int half = w / 2;
    const size_t N = v.size();
    std::vector<double> y(N, NAN);

    for (size_t i = 0; i < N; ++i) {
        int i0 = static_cast<int>(i) - half;
        int i1 = static_cast<int>(i) + half;
        if (i0 < 0) i0 = 0;
        if (i1 >= static_cast<int>(N)) i1 = static_cast<int>(N) - 1;

        double acc = 0.0;
        int    n   = 0;
        for (int k = i0; k <= i1; ++k) {
            if (std::isfinite(v[k])) {
                acc += v[k];
                ++n;
            }
        }
        if (n > 0)
            y[i] = acc / n;
    }

    v.swap(y);
}

void Normalize01(const std::vector<double>& in, std::vector<double>& out)
{
    out.assign(in.begin(), in.end());

    double mn, mx;
    if (!MinMax(out, mn, mx) || mx == mn) {
        std::fill(out.begin(), out.end(), 0.0);
        return;
    }

    const double inv = 1.0 / (mx - mn);
    for (double& x : out) {
        if (std::isfinite(x))
            x = (x - mn) * inv;
        else
            x = NAN;
    }
}

} 
