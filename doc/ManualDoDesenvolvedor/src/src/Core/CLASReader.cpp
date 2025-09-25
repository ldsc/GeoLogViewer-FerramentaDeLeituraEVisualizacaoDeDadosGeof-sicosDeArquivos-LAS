#include "CLASReader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

bool CLASReader::LoadFromFile(const std::string& FilePath, CWellLog& WellLog) {
    std::ifstream file(FilePath);
    if (!file.is_open()) {
        std::cerr << "Erro ao abrir o arquivo: " << FilePath << std::endl;
        return false;
    }

    std::string line;
    std::string currentSection;
    std::vector<std::string> curveNames;

    while (std::getline(file, line)) {
        // Remove espaços em branco no início e fim
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.empty() || line[0] == '#') {
            continue; // Ignora linhas vazias e comentários
        }

        if (line[0] == '~') {
            currentSection = line;
            std::transform(currentSection.begin(), currentSection.end(), currentSection.begin(), ::toupper);
            continue;
        }

        if (currentSection == "~CURVE") {
            // Exemplo de linha: "GR.API : GAMMA RAY"
            std::istringstream iss(line);
            std::string mnemonic;
            if (std::getline(iss, mnemonic, '.')) {
                // Limpa nome da curva
                mnemonic.erase(std::remove_if(mnemonic.begin(), mnemonic.end(), ::isspace), mnemonic.end());
                std::transform(mnemonic.begin(), mnemonic.end(), mnemonic.begin(), ::toupper);

                if (mnemonic == "DEPT" || mnemonic == "DEPTH") {
                    curveNames.push_back(mnemonic); // só pra saber onde está
                } else {
                    curveNames.push_back(mnemonic);
                    WellLog.RegisterCurve(mnemonic, "");
                }
            }
        } else if (currentSection == "~ASCII") {
            std::istringstream iss(line);
            float value;
            std::vector<float> values;

            while (iss >> value) {
                values.push_back(value);
            }

            if (values.size() != curveNames.size()) {
                std::cerr << "Número de valores não corresponde ao número de curvas." << std::endl;
                continue;
            }

            for (size_t i = 0; i < values.size(); ++i) {
                std::string name = curveNames[i];
                std::string clean = name;
                clean.erase(std::remove_if(clean.begin(), clean.end(), ::isspace), clean.end());
                std::transform(clean.begin(), clean.end(), clean.begin(), ::toupper);

                if (clean == "DEPT" || clean == "DEPTH") {
                    WellLog.AddDepth(values[i]);
                } else {
                    WellLog.AddCurveValue(name, values[i]);
                }
            }
        }
    }

    file.close();
    return true;
}
