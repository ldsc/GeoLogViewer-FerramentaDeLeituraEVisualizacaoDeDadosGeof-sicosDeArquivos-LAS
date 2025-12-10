#ifndef CINTERPRETER_H
#define CINTERPRETER_H

#include <string>
#include <vector>

class CWellLog;
class CFormationZone;

// Parametros de calculo de VSH 
struct VshParams {
    // Percentis para areia limpa e xisto
    double p_clean = 5.0;     // percentil da areia
    double p_shale = 95.0;    // percentil do xisto

    enum class Method {
        Linear,
        LarionovTertiary,
        LarionovOlder
    } method = Method::LarionovTertiary;

    // Suavizacao opcional na amostra (0 sem suavizacao)
    int smooth_window_samples = 0;

    // Nome da curva de GR
    std::string gr_curve = "GR";
};

// Parametros para resistividades 

struct SepParams {
    // Nomes padrao, ajuste conforme seu LAS
    std::string rdeep_curve = "RT";   // resistividade profunda
    std::string rmed_curve  = "RMD";  // media
    std::string rshal_curve = "RS";   // rasa
};

// Parametros de pay / triagem de reservatorio 

struct PayParams {
    double vsh_max       = 0.5;   // corte maximo de Vsh
    double RR_min        = 2.0;   // minimo de Rdeep/Rshal
    double dR_min        = 0.0;   // minimo de separacao (Rdeep - Rshal)
    double min_thickness = 1.0;   // espessura minima em metros
};

// Derivados calculados por profundidade

struct Derived {
    std::vector<double> depth;   // md (mesma amostragem do poco)

    std::vector<double> vsh;     // volume de argila
    std::vector<double> rdeep;   // resistividade profunda
    std::vector<double> rmed;    // media
    std::vector<double> rshal;   // rasa

    std::vector<double> dR;      // separacao Rdeep - Rshal
    std::vector<double> RR;      // razao Rdeep / Rshal
};

// Intervalo interpretado 

struct Interval {
    double top   = 0.0;   // topo (m)
    double base  = 0.0;   // base (m)

    double vsh_mean   = 0.0;
    double rdeep_mean = 0.0;
    double dR_mean    = 0.0;
    double RR_mean    = 0.0;

    double h() const { return base - top; }
};

//Classe principal de interpretacao 

class CInterpreter {
public:
    // Derivados: VSH Rdeep/med/shal dR RR (alinhados ao mesmo depth)
    static Derived ComputeDerived(const CWellLog& wl,
                                  const VshParams& vp = VshParams{},
                                  const SepParams& sp = SepParams{});

    // Picks automaticos de reservatorio (screening)
    static std::vector<Interval> PickReservoirs(const Derived& D,
                                                const PayParams& pp = PayParams{});

    // Exporta CSV simples de intervalos
    static void WriteCSV(const std::string& path,
                         const std::vector<Interval>& ivals);

    // Converte Interval CFormationZone (para uso posterior)
    static std::vector<CFormationZone> ToFormationZones(const std::vector<Interval>& ivals);
};

#endif
