#include "CInterpreter.h"
#include "CWellLog.h"
#include "CLogCurve.h"

#include <algorithm>
#include <cmath>
#include <optional>
#include <fstream>

// ----------------- helpers -----------------
namespace {

inline double clamp01(double x) { return x < 0 ? 0 : (x > 1 ? 1 : x); }

double percentile(std::vector<double> v, double p) {
    if (v.empty()) return NAN;
    if (p <= 0)   return *std::min_element(v.begin(), v.end());
    if (p >= 100) return *std::max_element(v.begin(), v.end());
    const double idx = (p/100.0) * (v.size()-1);
    const size_t i = static_cast<size_t>(idx);
    std::nth_element(v.begin(), v.begin()+i, v.end());
    double a = v[i];
    if (i+1 < v.size()) {
        std::nth_element(v.begin(), v.begin()+i+1, v.end());
        double b = v[i+1];
        return a + (idx - i) * (b - a);
    }
    return a;
}

void smooth_ma(std::vector<double>& x, int w) {
    if (w <= 1 || (int)x.size() <= w) return;
    const int n = (int)x.size(), half = w/2;
    std::vector<double> y(n, NAN);
    for (int i = 0; i < n; ++i) {
        int i0 = std::max(0, i - half);
        int i1 = std::min(n - 1, i + half);
        double acc = 0.0; int cnt = 0;
        for (int k = i0; k <= i1; ++k) if (std::isfinite(x[k])) { acc += x[k]; ++cnt; }
        if (cnt) y[i] = acc / cnt;
    }
    x.swap(y);
}

// média ignorando NaN
double mean_valid(const std::vector<double>& v, size_t i0, size_t i1) {
    double acc = 0.0; int n = 0;
    for (size_t i=i0; i<=i1; ++i) if (std::isfinite(v[i])) { acc += v[i]; ++n; }
    return n? acc/n : NAN;
}

} // namespace

// ----------------- ComputeDerived (igual ao anterior) -----------------

Derived CInterpreter::ComputeDerived(const CWellLog& wl,
                                     const VshParams& vp,
                                     const SepParams& sp)
{
    Derived D;

    const auto& depths = wl.GetDepths();
    if (depths.empty()) return D;

    const size_t N = depths.size();

    const bool hasNull = !std::isnan(wl.Info().null);
    const double vNull = wl.Info().null;
    auto is_null = [&](double v) -> bool {
        return hasNull && std::isfinite(v) && std::fabs(v - vNull) < 1e-6;
    };

    // -------- GR -> VSH --------
    std::vector<double> grAligned(N, NAN);
    {
        auto it = wl.GetCurves().find(vp.gr_curve);
        if (it != wl.GetCurves().end()) {
            const auto& gr = it->second.GetData();
            for (size_t i = 0; i < N && i < gr.size(); ++i) {
                double v = gr[i];
                if (!is_null(v) && std::isfinite(v)) grAligned[i] = v;
            }
            std::vector<double> grValid;
            grValid.reserve(N);
            for (double v : grAligned) if (std::isfinite(v)) grValid.push_back(v);

            if (!grValid.empty()) {
                double gr_clean = percentile(grValid, vp.p_clean);
                double gr_shale = percentile(grValid, vp.p_shale);
                if (gr_shale == gr_clean) gr_shale = gr_clean + 1e-6;

                D.vsh.resize(N, NAN);
                for (size_t i = 0; i < N; ++i) {
                    double g = grAligned[i];
                    if (!std::isfinite(g)) continue;
                    double igr = clamp01((g - gr_clean) / (gr_shale - gr_clean));
                    double v;
                    switch (vp.method) {
                        case VshParams::Method::Linear:            v = igr; break;
                        case VshParams::Method::LarionovTertiary:  v = 0.083 * (std::pow(2.0, 3.7*igr) - 1.0); break;
                        case VshParams::Method::LarionovOlder:     v = 0.33  * (std::pow(2.0, 2.0*igr)  - 1.0); break;
                    }
                    D.vsh[i] = clamp01(v);
                }
                if (vp.smooth_window_samples > 1) smooth_ma(D.vsh, vp.smooth_window_samples);
            }
        }
    }

    auto fillAligned = [&](const std::string& name, std::vector<double>& out){
        out.resize(N, NAN);
        auto it = wl.GetCurves().find(name);
        if (it == wl.GetCurves().end()) return;
        const auto& rv = it->second.GetData();
        for (size_t i = 0; i < N && i < rv.size(); ++i) {
            double v = rv[i];
            if (std::isfinite(v) && !is_null(v)) out[i] = v;
        }
    };
    fillAligned(sp.rdeep, D.rdeep);
    fillAligned(sp.rmed , D.rmed );
    fillAligned(sp.rshal, D.rshal);

    D.dR.resize(N, NAN);
    D.RR.resize(N, NAN);
    for (size_t i = 0; i < N; ++i) {
        const double rd = D.rdeep[i];
        const double rs = D.rshal[i];
        if (std::isfinite(rd) && std::isfinite(rs)) {
            D.dR[i] = rd - rs;
            if (rd > 0 && rs > 0) D.RR[i] = rd / rs;
        }
    }

    D.depth.assign(depths.begin(), depths.end());
    return D;
}

// ----------------- PickReservoirs -----------------

std::vector<Interval> CInterpreter::PickReservoirs(const Derived& D, const PayParams& pp)
{
    std::vector<Interval> out;
    if (D.depth.empty()) return out;

    const size_t N = D.depth.size();
    // passo vertical médio (m) — para converter gap espessura->amostras
    std::vector<double> dz; dz.reserve(N-1);
    for (size_t i=1;i<N;++i) if (std::isfinite(D.depth[i]) && std::isfinite(D.depth[i-1])) {
        dz.push_back(std::fabs(D.depth[i]-D.depth[i-1]));
    }
    double step = dz.empty()? 0.25 : percentile(dz, 50.0); // mediana ~ step
    int gap_samp = (step>0? (int)std::ceil(pp.merge_gap/step) : 1);
    int min_samp = (step>0? (int)std::ceil(pp.min_thick/step) : 1);

    // limiar dR por percentil global (considerando apenas finitos)
    double dr_thr = NAN;
    {
        std::vector<double> dRvalid;
        dRvalid.reserve(N);
        for (double v : D.dR) if (std::isfinite(v)) dRvalid.push_back(v);
        if (!dRvalid.empty()) dr_thr = percentile(dRvalid, (double)pp.dr_pctl);
    }

    // máscara de “candidato”
    std::vector<char> mask(N, 0);
    for (size_t i=0;i<N;++i) {
        bool vsh_ok = (i<D.vsh.size()   && std::isfinite(D.vsh[i])   && D.vsh[i] < pp.vsh_max);
        bool rt_ok  = (i<D.rdeep.size() && std::isfinite(D.rdeep[i]) && D.rdeep[i] > pp.rt_min);

        bool perm_ok = false;
        if (i<D.RR.size() && std::isfinite(D.RR[i]) && D.RR[i] > pp.rr_min) perm_ok = true;
        else if (std::isfinite(dr_thr) && i<D.dR.size() && std::isfinite(D.dR[i]) && D.dR[i] > dr_thr) perm_ok = true;

        mask[i] = (vsh_ok && rt_ok && perm_ok) ? 1 : 0;
    }

    // varre máscara gerando intervalos (com merge de gaps)
    size_t i = 0;
    while (i < N) {
        // encontra início
        while (i < N && !mask[i]) ++i;
        if (i >= N) break;
        size_t start = i;

        // anda até onde a sequência (com gaps pequenos) quebra
        int gap_run = 0;
        size_t j = i;
        for (; j < N; ++j) {
            if (mask[j]) {
                gap_run = 0; // em zona válida
            } else {
                ++gap_run;
                if (gap_run > gap_samp) break; // gap grande -> fecha
            }
        }
        size_t end = (j==N? N-1 : j-gap_run-1);

        if (end > start) {
            // checa espessura mínima
            double top  = D.depth[start];
            double base = D.depth[end];
            if (base < top) std::swap(base, top);
            if ((base - top) >= pp.min_thick) {
                // médias do intervalo
                double vsh_m   = mean_valid(D.vsh,   start, end);
                double rd_m    = mean_valid(D.rdeep, start, end);
                double dR_m    = mean_valid(D.dR,    start, end);
                double RR_m    = mean_valid(D.RR,    start, end);
                out.push_back({top, base, vsh_m, rd_m, dR_m, RR_m});
            }
        }
        i = (j==N? N : j); // continua
    }
    return out;
}

// ----------------- WriteCSV -----------------

void CInterpreter::WriteCSV(const std::string& path, const std::vector<Interval>& ivals)
{
    std::ofstream f(path);
    if (!f) return;
    f.setf(std::ios::fixed); f.precision(3);
    f << "Top_m,Base_m,Thick_m,VSH_mean,Rdeep_mean,dR_mean,RR_mean\n";
    for (const auto& I : ivals) {
        f << I.top << "," << I.base << "," << I.h() << ","
          << I.vsh_mean << "," << I.rdeep_mean << "," << I.dR_mean << "," << I.RR_mean << "\n";
    }
    f.close();
}
