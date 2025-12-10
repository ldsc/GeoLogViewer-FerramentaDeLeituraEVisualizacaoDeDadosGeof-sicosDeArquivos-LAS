#include "CInterpreter.h"
#include "CWellLog.h"
#include "CLogCurve.h"
#include "CPetrophysicsCalculator.h"
#include "CFormationZone.h"

#include <algorithm>
#include <cmath>
#include <fstream>

// ----------------- helpers -----------------
namespace {

inline double Clamp01(double x)
{
    if (x < 0.0) return 0.0;
    if (x > 1.0) return 1.0;
    return x;
}

inline bool Finite(double x)
{
    return std::isfinite(x);
}

// percentil simples [0,100]
double Percentile(std::vector<double> v, double p)
{
    if (v.empty())
        return NAN;

    if (p <= 0.0)
        return *std::min_element(v.begin(), v.end());
    if (p >= 100.0)
        return *std::max_element(v.begin(), v.end());

    std::sort(v.begin(), v.end());
    const double pos  = p / 100.0 * (v.size() - 1);
    const size_t i0   = static_cast<size_t>(pos);
    const double frac = pos - i0;

    if (i0 + 1 < v.size())
        return v[i0] * (1.0 - frac) + v[i0 + 1] * frac;
    else
        return v[i0];
}

// media movel simples, NaN-safe
void SmoothMA(std::vector<double>& v, int w)
{
    if (w <= 1 || v.empty())
        return;

    const int half = w / 2;
    const size_t N = v.size();
    std::vector<double> out(N, NAN);

    for (size_t i = 0; i < N; ++i) {
        int i0 = static_cast<int>(i) - half;
        int i1 = static_cast<int>(i) + half;
        if (i0 < 0) i0 = 0;
        if (i1 >= static_cast<int>(N)) i1 = static_cast<int>(N) - 1;

        double acc = 0.0;
        int    n   = 0;
        for (int k = i0; k <= i1; ++k) {
            if (Finite(v[k])) {
                acc += v[k];
                ++n;
            }
        }
        if (n > 0)
            out[i] = acc / n;
    }

    v.swap(out);
}

} // namespace

// ----------------- ComputeDerived -----------------

Derived CInterpreter::ComputeDerived(const CWellLog& wl,
                                     const VshParams& vp,
                                     const SepParams& sp)
{
    Derived D;

    const auto& zf = wl.GetDepths();
    const size_t N = zf.size();
    D.depth.resize(N);
    for (size_t i = 0; i < N; ++i)
        D.depth[i] = static_cast<double>(zf[i]);

    const auto& curves = wl.GetCurves();

    auto CopyCurve = [&](const std::string& curveName,
                         std::vector<double>& out)
    {
        out.assign(N, NAN);
        if (curveName.empty())
            return;

        auto it = curves.find(curveName);
        if (it == curves.end())
            return;

        const auto& data = it->second.GetData();
        const size_t nCopy = std::min(N, data.size());
        for (size_t i = 0; i < nCopy; ++i) {
            const float v = data[i];
            if (std::isfinite(v))
                out[i] = static_cast<double>(v);
        }
    };

    // Resistividades
    CopyCurve(sp.rdeep_curve, D.rdeep);
    CopyCurve(sp.rmed_curve,  D.rmed);
    CopyCurve(sp.rshal_curve, D.rshal);

    D.dR.assign(N, NAN);
    D.RR.assign(N, NAN);

    for (size_t i = 0; i < N; ++i) {
        const double rd = (i < D.rdeep.size()) ? D.rdeep[i] : NAN;
        const double rs = (i < D.rshal.size()) ? D.rshal[i] : NAN;

        if (Finite(rd) && Finite(rs) && rd > 0.0 && rs > 0.0) {
            D.dR[i] = rd - rs;
            D.RR[i] = rd / rs;
        }
    }

    // GR -> VSH via CPetrophysicsCalculator

    std::vector<double> gr;
    CopyCurve(vp.gr_curve, gr);

    std::vector<double> grValid;
    grValid.reserve(N);
    for (double g : gr)
        if (Finite(g)) grValid.push_back(g);

    D.vsh.assign(N, NAN);

    if (!grValid.empty()) {
        const double gr_clean = Percentile(grValid, vp.p_clean);
        double       gr_shale = Percentile(grValid, vp.p_shale);

        if (gr_shale == gr_clean)
            gr_shale = gr_clean + 1e-6;

        std::vector<double> igr(N, NAN);
        for (size_t i = 0; i < N; ++i) {
            if (!Finite(gr[i])) continue;
            igr[i] = Clamp01((gr[i] - gr_clean) / (gr_shale - gr_clean));
        }

        switch (vp.method) {
            case VshParams::Method::Linear:
                D.vsh = CPetroPhysicsCalculator::VshLinear(igr);
                break;
            case VshParams::Method::LarionovTertiary:
                D.vsh = CPetroPhysicsCalculator::VshLarionovTertiary(igr);
                break;
            case VshParams::Method::LarionovOlder:
                D.vsh = CPetroPhysicsCalculator::VshLarionovOlder(igr);
                break;
        }

        if (vp.smooth_window_samples > 1)
            SmoothMA(D.vsh, vp.smooth_window_samples);
    }

    return D;
}

// ----------------- PickReservoirs -----------------

std::vector<Interval> CInterpreter::PickReservoirs(const Derived& D,
                                                   const PayParams& pp)
{
    std::vector<Interval> out;

    const size_t N = D.depth.size();
    if (N == 0)
        return out;

    std::vector<bool> isPay(N, false);

    for (size_t i = 0; i < N; ++i) {
        const double vsh = (i < D.vsh.size())   ? D.vsh[i]   : NAN;
        const double RR  = (i < D.RR.size())    ? D.RR[i]    : NAN;
        const double dR  = (i < D.dR.size())    ? D.dR[i]    : NAN;

        if (!Finite(vsh) || !Finite(RR))
            continue;

        if (vsh > pp.vsh_max)
            continue;

        if (RR < pp.RR_min)
            continue;

        if (pp.dR_min > 0.0) {
            if (!Finite(dR) || dR < pp.dR_min)
                continue;
        }

        isPay[i] = true;
    }

    size_t i = 0;
    while (i < N) {
        while (i < N && !isPay[i])
            ++i;
        if (i >= N)
            break;

        const size_t iStart = i;
        while (i < N && isPay[i])
            ++i;
        const size_t iEnd = i - 1;

        const double top  = D.depth[iStart];
        const double base = D.depth[iEnd];

        if ((base - top) < pp.min_thickness)
            continue;

        double sum_vsh   = 0.0; int n_vsh   = 0;
        double sum_rdeep = 0.0; int n_rdeep = 0;
        double sum_dR    = 0.0; int n_dR    = 0;
        double sum_RR    = 0.0; int n_RR    = 0;

        for (size_t k = iStart; k <= iEnd; ++k) {
            if (k < D.vsh.size() && Finite(D.vsh[k])) {
                sum_vsh += D.vsh[k];
                ++n_vsh;
            }
            if (k < D.rdeep.size() && Finite(D.rdeep[k])) {
                sum_rdeep += D.rdeep[k];
                ++n_rdeep;
            }
            if (k < D.dR.size() && Finite(D.dR[k])) {
                sum_dR += D.dR[k];
                ++n_dR;
            }
            if (k < D.RR.size() && Finite(D.RR[k])) {
                sum_RR += D.RR[k];
                ++n_RR;
            }
        }

        Interval iv;
        iv.top   = top;
        iv.base  = base;
        iv.vsh_mean   = (n_vsh   > 0) ? (sum_vsh   / n_vsh)   : NAN;
        iv.rdeep_mean = (n_rdeep > 0) ? (sum_rdeep / n_rdeep) : NAN;
        iv.dR_mean    = (n_dR    > 0) ? (sum_dR    / n_dR)    : NAN;
        iv.RR_mean    = (n_RR    > 0) ? (sum_RR    / n_RR)    : NAN;

        out.push_back(iv);
    }

    return out;
}

// ----------------- WriteCSV -----------------

void CInterpreter::WriteCSV(const std::string& path,
                            const std::vector<Interval>& ivals)
{
    std::ofstream f(path.c_str());
    if (!f) return;

    f.setf(std::ios::fixed);
    f.precision(3);

    f << "Top_m,Base_m,Thick_m,VSH_mean,Rdeep_mean,dR_mean,RR_mean\n";
    for (const auto& I : ivals) {
        f << I.top  << ","
          << I.base << ","
          << I.h()  << ","
          << I.vsh_mean   << ","
          << I.rdeep_mean << ","
          << I.dR_mean    << ","
          << I.RR_mean    << "\n";
    }
}

// ----------------- ToFormationZones -----------------

std::vector<CFormationZone> CInterpreter::ToFormationZones(const std::vector<Interval>& ivals)
{
    std::vector<CFormationZone> out;
    out.reserve(ivals.size());

    for (const auto& iv : ivals) {
        CFormationZone z(iv.top, iv.base);
        z.SetProp("VSH",   iv.vsh_mean);
        z.SetProp("RDEEP", iv.rdeep_mean);
        z.SetProp("dR",    iv.dR_mean);
        z.SetProp("RR",    iv.RR_mean);
        out.push_back(z);
    }

    return out;
}
