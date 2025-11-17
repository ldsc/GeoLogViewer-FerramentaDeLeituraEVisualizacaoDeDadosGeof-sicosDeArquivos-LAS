#include "Core/CLASReader.h"
#include "Core/CWellLog.h"
#include "Plotting/CPlotter.h"
#include "Interpretation/CInterpreter.h"
#include <iostream>

int main() {
    CWellLog oWell;
    CLASReader oReader;

    if (!oReader.LoadFromFile("../Data/1-KPGL-1D-SPS_GR-RES.las", oWell)) {
        std::cerr << "Erro ao ler o arquivo .LAS" << std::endl;
        return 1;
    }

    std::cout << "Arquivo lido com sucesso!" << std::endl;

    CPlotter oPlotter;

    VshParams vp;              // (padrões ok: Larionov Tertiary, P5/P95)
    SepParams sp;              // usa RPD2/RPM2/RPS2
    PayParams pp;              // VSH<0.40; RR>1.4 ou dR>P90; Rt>5; merge 0.5m; min 1.0m

    auto D     = CInterpreter::ComputeDerived(oWell, vp, sp);
    auto picks = CInterpreter::PickReservoirs(D, pp);

    // (opcional) CSV
    CInterpreter::WriteCSV("../out/intervalos.csv", picks);

    // 2) Lista de curvas que você já usa (as “14 tracks” ou qualquer subconjunto)
    std::vector<std::string> curves = {
        "GR","RPD2","RPM2","RPS2"
    };

    oPlotter.PlotWithIntervals(oWell, curves, picks);

    oPlotter.PlotSelectedTracks(oWell, {});
    //normalized
    oPlotter.PlotCompareCurves(oWell, curves, /*overlay=*/true, /*normalize=*/true);
    //overlay
    oPlotter.PlotCompareCurves(oWell, curves, /*overlay=*/true, /*normalize=*/false);
    //sidebyside
    oPlotter.PlotCompareCurves(oWell, curves, /*overlay=*/false, /*normalize=*/false);

    //oPlotter.PlotCurve(oWell, "GR");

    return 0;
}