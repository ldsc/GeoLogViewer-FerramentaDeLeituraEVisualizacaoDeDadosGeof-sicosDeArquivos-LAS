#ifndef CINTERPRETER_H
#define CINTERPRETER_H

#include <string>
#include <vector>

class CWellLog;
class CFormationZone;

//-------------------- Parâmetros de cálculo de VSH --------------------

struct VshParams {
    // Percentis para “areia limpa” e “xisto”
    double p_clean = 5.0;     // percentil da areia
    double p_shale = 95.0;    // percentil do xisto

    enum class Method {
        Linear,
        LarionovTertiary,
        LarionovOlder
    } method = Method::LarionovTertiary;

    // Suavização opcional na amostra (0 = sem suavização)
    int smooth_window_samples = 0;

    // Nome da curva de GR
    std::string gr_curve = "GR";
};

//-------------------- Parâmetros para resistividades --------------------

struct SepParams {
    // Nomes padrão, ajuste conforme seu LAS
    std::string rdeep_curve = "RT";   // resistividade profunda
    std::string rmed_curve  = "RMD";  // média
    std::string rshal_curve = "RS";   // rasa
};

//-------------------- Parâmetros de “pay” / triagem de reservatório --------------------

struct PayParams {
    double vsh_max       = 0.5;   // corte máximo de Vsh
    double RR_min        = 2.0;   // mínimo de Rdeep/Rshal
    double dR_min        = 0.0;   // mínimo de separação (Rdeep - Rshal)
    double min_thickness = 1.0;   // espessura mínima em metros
};

//-------------------- Derivados calculados por profundidade --------------------

struct Derived {
    std::vector<double> depth;   // md (mesma amostragem do poço)

    std::vector<double> vsh;     // volume de argila
    std::vector<double> rdeep;   // resistividade profunda
    std::vector<double> rmed;    // média
    std::vector<double> rshal;   // rasa

    std::vector<double> dR;      // separação Rdeep - Rshal
    std::vector<double> RR;      // razão Rdeep / Rshal
};

//-------------------- Intervalo interpretado --------------------

struct Interval {
    double top   = 0.0;   // topo (m)
    double base  = 0.0;   // base (m)

    double vsh_mean   = 0.0;
    double rdeep_mean = 0.0;
    double dR_mean    = 0.0;
    double RR_mean    = 0.0;

    double h() const { return base - top; }
};

//-------------------- Classe principal de interpretação --------------------

class CInterpreter {
public:
    // Derivados: VSH + Rdeep/med/shal + dR + RR (alinhados ao mesmo depth)
    static Derived ComputeDerived(const CWellLog& wl,
                                  const VshParams& vp = VshParams{},
                                  const SepParams& sp = SepParams{});

    // Picks automáticos de “reservatório” (screening)
    static std::vector<Interval> PickReservoirs(const Derived& D,
                                                const PayParams& pp = PayParams{});

    // Exporta CSV simples de intervalos
    static void WriteCSV(const std::string& path,
                         const std::vector<Interval>& ivals);

    // Converte Interval -> CFormationZone (para uso posterior)
    static std::vector<CFormationZone> ToFormationZones(const std::vector<Interval>& ivals);
};

#endif
