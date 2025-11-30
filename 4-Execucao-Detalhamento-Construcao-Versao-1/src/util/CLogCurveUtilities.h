#ifndef CLOGCURVEUTILITIES_H
#define CLOGCURVEUTILITIES_H

#include <vector>
#include <cstddef>

namespace CLogCurveUtilities {

    // estatísticas ignorando NaN
    bool MinMax(const std::vector<double>& v, double& vmin, double& vmax);
    double Mean(const std::vector<double>& v);

    // média móvel simples (janela w amostras; NaN-safe)
    void SmoothMA(std::vector<double>& v, int w);

    // normaliza para [0,1] ignorando NaN
    void Normalize01(const std::vector<double>& in, std::vector<double>& out);

    // alinha dois vetores ao menor tamanho
    template<typename T> void TruncToSameSize(std::vector<T>& a, std::vector<T>& b) {
        size_t n = std::min(a.size(), b.size());
        a.resize(n); b.resize(n);
    }
}

#endif
