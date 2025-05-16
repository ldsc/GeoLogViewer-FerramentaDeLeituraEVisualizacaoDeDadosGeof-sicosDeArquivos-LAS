#include "Core/CLASReader.h"
#include "Core/CWellLog.h"
#include <iostream>

int main() {
    CWellLog oWell;
    CLASReader oReader;

    if (!oReader.LoadFromFile("../Data/1-KPGL-1D-SPS_GR-RES.las", oWell)) {
        std::cerr << "Erro ao ler o arquivo .LAS" << std::endl;
        return 1;
    }

    std::cout << "Arquivo lido com sucesso!" << std::endl;


    return 0;
}