#include "Core/CLASReader.h"
#include "Core/CWellLog.h"
#include "Plotting/CPlotter.h"
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

    oPlotter.PlotCurve(oWell, "GR");

    oPlotter.PlotMultiple(oWell, {});

    oPlotter.PlotSelectedTracks(oWell, {});


    //normalized
    oPlotter.PlotCompareCurves(oWell, {"GR","RPD2","RPM2","RPS2"}, /*overlay=*/true, /*normalize=*/true);
    //overlay
    oPlotter.PlotCompareCurves(oWell, {"GR","RPD2","RPM2"}, /*overlay=*/true, /*normalize=*/false);
    //sidebyside
    oPlotter.PlotCompareCurves(oWell, {"GR","RPD2","RPM2","RPS2"}, /*overlay=*/false, /*normalize=*/false);

   

    return 0;
}