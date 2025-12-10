#ifndef CGEOLOGVIEWERAPP_H
#define CGEOLOGVIEWERAPP_H

#include <string>
#include <vector>

class CLASReader;
class CWellLog;
class CPlotter;

class CGeoLogViewerApp {
public:
    CGeoLogViewerApp();
    ~CGeoLogViewerApp();

    void Executar();   // loop principal

private:
    void Menu();
    void AcaoCarregarLAS();
    void AcaoListarCurvas() const;
    void AcaoPlotarTracks();
    void AcaoCompararCurvas();
    void AcaoInterpretacaoBasica();
    void AcaoExportarCSVIntervalos();

private:
    CLASReader*  mpReader;
    CPlotter*    mpPlotter;

    CWellLog*    mpWell;     // ponteiro para o membro mWell
    CWellLog*    mWellStorage;
    CWellLog&    Well();     // helper para obter referencia valida ao well

    std::string  mUltimoLAS;
};

#endif
