#include "app/CGeoLogViewerApp.h"

#include "core/CLASReader.h"
#include "core/CWellLog.h"
#include "plotting/CPlotter.h"
#include "interpretation/CInterpreter.h"

#include <iostream>
#include <algorithm>
#include <limits>

CGeoLogViewerApp::CGeoLogViewerApp()
    : mpReader(new CLASReader()),
      mpPlotter(new CPlotter()),
      mpWell(nullptr),
      mWellStorage(nullptr) {}

CGeoLogViewerApp::~CGeoLogViewerApp() {
    delete mWellStorage; 
    delete mpPlotter;
    delete mpReader;
}

static void PrintPrompt() {
    std::cout << "\n==== GeoLogViewer ====\n"
              << "1) Carregar LAS\n"
              << "2) Listar curvas\n"
              << "3) Plotar tracks selecionadas (multiplot)\n"
              << "4) Comparar curvas (overlay/side-by-side)\n"
              << "5) Interpretacao basica (GR | VSH | dR/RR) + shading\n"
              << "6) Exportar intervalos (CSV)\n"
              << "0) Sair\n> ";
}

void CGeoLogViewerApp::Menu() { PrintPrompt(); }

CWellLog& CGeoLogViewerApp::Well() {
    
    if (!mpWell) {
        if (!mWellStorage) mWellStorage = new CWellLog();
        mpWell = mWellStorage;
    }
    return *mpWell;
}

void CGeoLogViewerApp::Executar() {
    for (;;) {
        Menu();
        int op = -1;
        if (!(std::cin >> op)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        switch (op) {
            case 1: AcaoCarregarLAS(); break;
            case 2: AcaoListarCurvas(); break;
            case 3: AcaoPlotarTracks(); break;
            case 4: AcaoCompararCurvas(); break;
            case 5: AcaoInterpretacaoBasica(); break;
            case 6: AcaoExportarCSVIntervalos(); break;
            case 0: return;
            default: std::cout << "Opcao invalida.\n"; break;
        }
    }
}

void CGeoLogViewerApp::AcaoCarregarLAS() {
    std::cout << "Caminho do arquivo LAS: ";
    std::cin >> mUltimoLAS;

    CWellLog& wellRef = Well(); // referência ao storage

    const bool ok = mpReader->LoadFromFile(mUltimoLAS, wellRef);
    if (!ok) {
        std::cout << "Falha ao ler LAS.\n";
        return;
    }

    if (wellRef.GetDepths().empty() || wellRef.GetCurves().empty()) {
        std::cout << "LAS lido, mas sem profundidades/curvas.\n";
        return;
    }

    std::cout << "Arquivo lido com sucesso!\n";
}

void CGeoLogViewerApp::AcaoListarCurvas() const {
    if (!mpWell) { std::cout << "Carregue um LAS primeiro.\n"; return; }
    std::cout << "Curvas:\n";
    for (const auto& kv : mpWell->GetCurves())
        std::cout << " - " << kv.first << "\n";
}

void CGeoLogViewerApp::AcaoPlotarTracks() {
    if (!mpWell) { std::cout << "Carregue um LAS primeiro.\n"; return; }
    std::cout << "Curvas separadas por virgula (vazio = todas): ";
    std::string line; 
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::getline(std::cin, line);

    std::vector<std::string> curves;
    if (!line.empty()) {
        size_t pos=0;
        while (pos < line.size()) {
            size_t comma = line.find(',', pos);
            std::string c = line.substr(pos, (comma==std::string::npos)? std::string::npos : comma-pos);
            c.erase(std::remove_if(c.begin(), c.end(), ::isspace), c.end());
            if (!c.empty()) curves.push_back(c);
            if (comma==std::string::npos) break; else pos = comma+1;
        }
    }
    mpPlotter->PlotSelectedTracks(*mpWell, curves);
}

void CGeoLogViewerApp::AcaoCompararCurvas() {
    if (!mpWell) { std::cout << "Carregue um LAS primeiro.\n"; return; }
    std::cout << "Curvas para comparar (virgulas): ";
    std::string line; 
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::getline(std::cin, line);

    std::vector<std::string> curves;
    size_t pos=0;
    while (pos < line.size()) {
        size_t comma = line.find(',', pos);
        std::string c = line.substr(pos, (comma==std::string::npos)? std::string::npos : comma-pos);
        c.erase(std::remove_if(c.begin(), c.end(), ::isspace), c.end());
        if (!c.empty()) curves.push_back(c);
        if (comma==std::string::npos) break; else pos = comma+1;
    }
    bool overlay=true, normalize=false;
    std::cout << "Overlay? (1=sim 0=nao): "; std::cin >> overlay;
    if (overlay) { std::cout << "Normalizar 0-1? (1/0): "; std::cin >> normalize; }

    mpPlotter->PlotCompareCurves(*mpWell, curves, overlay, normalize);
}

void CGeoLogViewerApp::AcaoInterpretacaoBasica() {
    if (!mpWell) { std::cout << "Carregue um LAS primeiro.\n"; return; }
    VshParams vp; SepParams sp;
    mpPlotter->PlotInterpretationBasic(*mpWell, vp, sp);
}

void CGeoLogViewerApp::AcaoExportarCSVIntervalos() {
    if (!mpWell) { std::cout << "Carregue um LAS primeiro.\n"; return; }
    VshParams vp; SepParams sp; PayParams pp;
    auto D     = CInterpreter::ComputeDerived(*mpWell, vp, sp);
    auto picks = CInterpreter::PickReservoirs(D, pp);
    CInterpreter::WriteCSV("./out/intervalos.csv", picks);
    std::cout << "intervalos.csv gerado em ./out\n";
}
