#include "CPlotter.h"
#include <vector>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <fstream>
#include <map>
#include <cstdlib>

using namespace std;

void CPlotter::PlotCurve(const CWellLog& rWell, const std::string& strCurveName) {
    PlotSingleTrack(rWell, strCurveName);
}

void CPlotter::PlotMultiple(const CWellLog& rWell, const std::vector<std::string>& vecCurveNames) {
    for (const auto& strCurve : vecCurveNames) {
        PlotSingleTrack(rWell, strCurve);
          std::cin.get();
    }
}

void CPlotter::PlotCompositeTracks(const CWellLog& rWell, const std::vector<std::string>& vecDummy) {
    PlotSelectedTracks(rWell, {}); // Chama SelectedTracks sem curvas => plota todas
}

void CPlotter::PlotSelectedTracks(const CWellLog& rWell, const std::vector<std::string>& vecCurvesSelecionadas) {
    const auto& vecDepth = rWell.GetDepths();
    const auto& mapCurves = rWell.GetCurves();

    if (vecDepth.empty()) {
        std::cerr << "Sem dados de profundidade para multiplot." << std::endl;
        return;
    }

    std::vector<std::string> curvesToPlot = vecCurvesSelecionadas;
    if (curvesToPlot.empty()) {
        for (const auto& [curveName, _] : mapCurves)
            curvesToPlot.push_back(curveName);
    }

    double fMinDepth = *std::min_element(vecDepth.begin(), vecDepth.end());
    double fMaxDepth = *std::max_element(vecDepth.begin(), vecDepth.end());

    int totalTracks = static_cast<int>(curvesToPlot.size());
    double trackWidth = 1.0 / (totalTracks + 1); // Espaçamento entre as tracks
    double currentOrigin = 0.0;

    std::ofstream gp("multiplot_selected.gp");
    gp << "set terminal windows size 1920,900\n";
    gp << "set multiplot\n";
    gp << "set tmargin at screen 0.95\n";
    gp << "set bmargin at screen 0.05\n";

    // ----- Painel de profundidade -----
    {
        std::ofstream depthFile("track_DEPTH.dat");
        for (const auto& d : vecDepth) {
            depthFile << 0 << " " << d << "\n";   // lado esquerdo
            depthFile << 1 << " " << d << "\n";   // lado direito
            depthFile << "\n"; // separa com linha em branco para cada linha de profundidade
        }
        depthFile.close();
    
        gp << "set size " << trackWidth << ",1\n";
        gp << "set origin " << currentOrigin << ",0\n";
        gp << "unset key\n";
        gp << "unset xtics\n";
        gp << "set ytics nomirror\n";
        gp << "set ylabel 'Profundidade (m)'\n";
        gp << "set xrange [0:1]\n";
        gp << "set yrange [" << fMaxDepth << ":" << fMinDepth << "]\n";
        gp << "set style fill solid 1.0\n"; // preenchimento sólido
        gp << "plot 'track_DEPTH.dat' using 1:2 with filledcurves x1 lc rgb 'gray' notitle\n";
    
        currentOrigin += trackWidth;
    }

    // ----- Agora gera os gráficos de curvas -----
    for (const auto& curve : curvesToPlot) {
        auto it = mapCurves.find(curve);
        if (it == mapCurves.end()) {
            std::cerr << "Curva '" << curve << "' nao encontrada." << std::endl;
            continue;
        }

        const auto& vecData = it->second.GetData();
        if (vecDepth.size() != vecData.size()) {
            std::cerr << "Tamanhos incompativeis na curva '" << curve << "'" << std::endl;
            continue;
        }

        std::string datName = "track_" + curve + ".dat";
        std::ofstream dat(datName);
        for (size_t i = 0; i < vecDepth.size(); i++) {
            dat << vecData[i] << " " << vecDepth[i] << "\n";
        }
        dat.close();

        std::vector<double> xFiltered;
        for (auto v : vecData) {
            if (v > -999 && v != 0)
                xFiltered.push_back(static_cast<double>(v));
        }
        if (xFiltered.empty()) {
            for (auto v : vecData)
                xFiltered.push_back(static_cast<double>(v));
        }

        double fMinX = *std::min_element(xFiltered.begin(), xFiltered.end());
        double fMaxX = *std::max_element(xFiltered.begin(), xFiltered.end());
        if (fMinX == fMaxX) {
            fMinX -= 1;
            fMaxX += 1;
        }

        gp << "set size " << trackWidth << ",1\n";
        gp << "set origin " << currentOrigin << ",0\n";
        gp << "set xlabel '" << curve << "'\n";
        gp << "set ylabel ''\n"; // Só o primeiro tem o ylabel
        gp << "set format x '%.2f'\n";
        gp << "set xrange [" << fMinX << ":" << fMaxX << "]\n";
        gp << "set xtics (" << fMinX << "," << fMaxX << ")\n";
        gp << "set yrange [" << fMaxDepth << ":" << fMinDepth << "]\n";
        gp << "plot '" << datName << "' using 1:2 with lines notitle\n";

        currentOrigin += trackWidth; // Avança para o próximo
    }

    gp << "unset multiplot\n";
    gp.close();

    std::cout<<"antes"<<std::endl;
    system("gnuplot -persist multiplot_selected.gp");
    std::cout<<"depois"<<std::endl;
} 

void CPlotter::PlotSingleTrack(const CWellLog& rWell, const std::string& strCurveName) {
    const auto& vecDepth = rWell.GetDepths();
    const auto& mapCurves = rWell.GetCurves();

    auto it = mapCurves.find(strCurveName);
    if (it == mapCurves.end()) {
        std::cerr << "Curva nao encontrada: " << strCurveName << std::endl;
        return;
    }

    const auto& vecData = it->second.GetData();
    if (vecDepth.size() != vecData.size()) {
        std::cerr << "Tamanhos incompativeis entre profundidade e curva: " << strCurveName << std::endl;
        return;
    }

    std::vector<double> x(vecData.begin(), vecData.end());
    std::vector<double> y(vecDepth.begin(), vecDepth.end());

    double fMinDepth = *std::min_element(y.begin(), y.end());
    double fMaxDepth = *std::max_element(y.begin(), y.end());
    double fMinX = *std::min_element(x.begin(), x.end());
    double fMaxX = *std::max_element(x.begin(), x.end());

    if (fMinX == fMaxX) {
        fMinX -= 1;
        fMaxX += 1;
    }

    std::cout << "Plotando curva '" << strCurveName << "' com " << x.size() << " pontos." << std::endl;

    Gnuplot plot;
    plot.Cmd("set terminal windows size 250,900");
    plot.Cmd("unset output");
    plot.set_style("lines");
    plot.set_xlabel(strCurveName);
    plot.set_ylabel("Profundidade (m)");
    plot.set_xrange(fMinX, fMaxX);
    plot.set_yrange(fMaxDepth, fMinDepth);
    plot.set_grid();
    plot.plot_xy(x, y, strCurveName);

    std::cin.get();
}
