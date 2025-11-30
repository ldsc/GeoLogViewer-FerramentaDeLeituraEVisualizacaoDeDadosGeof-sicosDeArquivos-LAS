// CLASReader.cpp
#include "CLASReader.h"
#include "CWellLog.h"
#include "CLogCurve.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>   // std::strtod
#include <fstream>
#include <iostream>
#include <limits>    // std::numeric_limits
#include <sstream>
#include <string>
#include <vector>
#include <cmath>     // std::isnan

// Util: trim simples
static inline std::string Trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace((unsigned char)s[b])) ++b;
    while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
    return s.substr(b, e - b);
}

// Util: upper sem espaços
static inline std::string UpperNoSpace(std::string s) {
    s.erase(std::remove_if(s.begin(), s.end(), ::isspace), s.end());
    std::transform(s.begin(), s.end(), s.begin(), ::toupper);
    return s;
}

bool CLASReader::LoadFromFile(const std::string& path, CWellLog& WellLog)
{
    std::ifstream in(path);
    if (!in.is_open()) {
        std::cerr << "[CLASReader] Nao foi possivel abrir: " << path << "\n";
        return false;
    }

    std::string line;
    std::string currentSection;
    std::vector<std::string> curveNames; // ordem das colunas em ~ASCII

    // zera info do ~WELL (mantendo NaN por padrão)
    SWellInfo info = WellLog.Info();

    while (std::getline(in, line)) {
        // Normaliza quebras de linha e ignora linhas vazias
        if (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
            line.pop_back();
        std::string raw = line;
        line = Trim(line);
        if (line.empty()) continue;

        // Troca de seção (~WELL, ~CURVE, ~ASCII, etc.)
        if (line.size() > 0 && line[0] == '~') {
            currentSection = UpperNoSpace(line); // garante ~WELL, ~CURVE, etc. em upper
            continue;
        }

        // Ignora comentários iniciando por '#'
        if (line[0] == '#') continue;

        // =========================
        // Seção ~WELL: NULL/STRT/STOP/STEP
        // =========================
        if (currentSection == "~WELL") {
            // Formatos comuns:
            //  STRT.M        343.96 : START DEPTH
            //  STOP.M        915.00 : STOP DEPTH
            //  STEP.M        0.1524 : STEP
            //  NULL.       -999.25  : NULL VALUE
            auto posDot   = line.find('.');
            auto posColon = line.find(':');

            if (posDot != std::string::npos) {
                std::string key = UpperNoSpace(line.substr(0, posDot));

                // captura o primeiro número da linha
                double val = std::numeric_limits<double>::quiet_NaN();
                {
                    std::istringstream iss(line);
                    std::string tok;
                    while (iss >> tok) {
                        char* endp = nullptr;
                        double tmp = std::strtod(tok.c_str(), &endp);
                        if (endp && *endp == '\0') { val = tmp; break; }
                    }
                }

                if (!std::isnan(val)) {
                    if      (key == "NULL") info.null = val;
                    else if (key == "STRT") info.strt = val;
                    else if (key == "STOP") info.stop = val;
                    else if (key == "STEP") info.step = val;
                }
            }
            WellLog.SetInfo(info);
            continue;
        }

        // =========================
        // Seção ~CURVE: mnemonic e unidade
        // =========================
        if (currentSection == "~CURVE") {
            // Exemplos de linha:
            //  DEPT.M        : DEPTH
            //  GR.API        : GAMMA RAY
            //  RPD2.OHM-M    : RES 2MHz (Deep)
            //  ROP.M/HR
            // Regra: texto antes do '.' e o mnemonic; apos o '.' ate espaco/':' e a unidade
            std::string mnemonic, unit;

            auto posDot = line.find('.');
            if (posDot != std::string::npos) {
                mnemonic = line.substr(0, posDot);
                auto rest = line.substr(posDot + 1);
                size_t endu = rest.find_first_of(" \t:");
                unit = (endu == std::string::npos) ? rest : rest.substr(0, endu);
            } else {
                // sem ponto -> sem unidade explicita
                size_t endm = line.find_first_of(" \t:");
                mnemonic = (endm == std::string::npos) ? line : line.substr(0, endm);
                unit.clear();
            }

            mnemonic = UpperNoSpace(mnemonic);

            // DEPT/DEPTH e tratado como indice de profundidade
            if (mnemonic == "DEPT" || mnemonic == "DEPTH") {
                curveNames.push_back(mnemonic);
            } else {
                curveNames.push_back(mnemonic);
                WellLog.RegisterCurve(mnemonic, unit); // <--- agora com unidade
            }
            continue;
        }

        // =========================
        // Seção ~ASCII: dados (DEPT + curvas na ordem de curveNames)
        // =========================
        if (currentSection == "~ASCII") {
            // separa tokens por espaço/abas
            std::vector<std::string> toks;
            {
                std::istringstream iss(line);
                std::string t;
                while (iss >> t) toks.push_back(t);
            }
            if (toks.empty()) continue;

            if (toks.size() != curveNames.size()) {
                // linha malformada — loga e continua
                std::cerr << "[CLASReader] Linha ~ASCII com colunas " << toks.size()
                          << " mas esperado " << curveNames.size() << " : '" << raw << "'\n";
                continue;
            }

            // primeiro token → profundidade
            {
                char* endp = nullptr;
                double d = std::strtod(toks[0].c_str(), &endp);
                if (!(endp && *endp == '\0')) {
                    std::cerr << "[CLASReader] Profundidade invalida: " << toks[0] << "\n";
                    continue;
                }
                WellLog.AddDepth(d);
            }

            // demais tokens → valores de curvas
            for (size_t i = 1; i < toks.size(); ++i) {
                const std::string& name = UpperNoSpace(curveNames[i]);
                char* endp = nullptr;
                double v = std::strtod(toks[i].c_str(), &endp);
                if (!(endp && *endp == '\0')) {
                    // valor inválido; registra NaN? aqui optamos por pular
                    continue;
                }
                WellLog.AddCurveValue(name, static_cast<float>(v));
            }
            continue;
        }

        // Outras seções: ignoradas por enquanto (mantemos o que já funciona)
    }

    return true;
}
