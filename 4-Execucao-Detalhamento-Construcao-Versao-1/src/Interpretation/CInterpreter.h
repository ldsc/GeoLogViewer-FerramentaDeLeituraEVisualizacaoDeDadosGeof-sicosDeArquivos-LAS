#ifndef CINTERPRETER_H
#define CINTERPRETER_H

#include <string>
#include <vector>

class CWellLog;

struct VshParams {
    double p_clean = 5.0;    // percentil da “areia”
    double p_shale = 95.0;   // percentil do “xisto”
    enum class Method { Linear, LarionovTertiary, LarionovOlder } method = Method::LarionovTertiary;
    int    smooth_window_samples = 0;      // 0 = sem suavização
    std::string gr_curve = "GR";
};

struct SepParams {
    std::string rdeep = "RPD2";   // deep resistivity
    std::string rmed  = "RPM2";   // medium (opcional)
    std::string rshal = "RPS2";   // shallow
};

struct Derived {
    std::vector<double> depth;     // eixo Y alinhado
    std::vector<double> vsh;       // 0..1
    std::vector<double> rdeep, rmed, rshal;
    std::vector<double> dR;        // Rdeep - Rshal
    std::vector<double> RR;        // Rdeep / Rshal (onde >0)
};

struct PayParams {
    double vsh_max   = 0.40;   // “limpo” se VSH < vsh_max
    double rr_min    = 1.40;   // permeabilidade por RR
    int    dr_pctl   = 90;     // permeabilidade por dR > P{dr_pctl}
    double rt_min    = 5.0;    // resistividade mínima (ohm·m)
    double merge_gap = 0.5;    // (m) gaps internos que podem ser unidos
    double min_thick = 1.0;    // (m) espessura mínima do intervalo final
};

struct Interval {
    double top  = 0.0;   // m
    double base = 0.0;   // m
    double vsh_mean   = 0.0;
    double rdeep_mean = 0.0;
    double dR_mean    = 0.0;
    double RR_mean    = 0.0;

    double h() const { return base - top; }
};

class CInterpreter {
public:
    // Derivados: VSH + Rdeep/med/shal + dR + RR (alinhados ao mesmo depth).
    static Derived ComputeDerived(const CWellLog& wl,
                                  const VshParams& vp = {},
                                  const SepParams& sp = {});

    // Picks automáticos de “reservatório (screening)”.
    static std::vector<Interval> PickReservoirs(const Derived& D, const PayParams& pp);

    // Exporta CSV simples de intervalos.
    static void WriteCSV(const std::string& path, const std::vector<Interval>& ivals);
};

#endif