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

    

    oPlotter.PlotSelectedTracks(oWell, {"GR","AP","RPD2","ROP"});

    
    oPlotter.PlotCurve(oWell, "GR");


    oPlotter.PlotMultiple(oWell, {"GR","AP"});


    oPlotter.PlotCompositeTracks(oWell, {});



    return 0;
}