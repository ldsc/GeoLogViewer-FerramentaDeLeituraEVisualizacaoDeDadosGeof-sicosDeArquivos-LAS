// CPlotter.cpp
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <cctype>
#include <cmath>      // std::isfinite, std::isnan, std::fabs

#include "CPlotter.h"
#include "CWellLog.h"
#include "CLogCurve.h"
#include "CGnuplot.h"

// =====================
// Helpers internos
// =====================
    // Pasta padrão para saída dos arquivos .dat
    static std::string s_OutDir = "../out";

    // ===== Config do OVERLAY =====
    constexpr int    kOverlayTermW   = 420;   // largura da janela do overlay (px)  [ajuste aqui]
    constexpr int    kOverlayTermH   = 900;   // altura da janela do overlay (px)
    constexpr double kOverlayRulerPx = 84.0;  // largura da régua no overlay (px)   [mais grossa]
    constexpr double kOverlayGapPx   = 10.0;  // gap entre régua e track (px)

    // ------------------ SIDE-BY-SIDE (terminal do tamanho exato) ------------------
    // Knobs (px) — ajuste ao gosto
    constexpr int    kSideTermH       = 900;   // altura da janela
    constexpr double kRulerPx         = 96.0;  // largura da régua
    constexpr double kTrackPxTarget   = 180.0; // largura desejada por track
    constexpr double kGapPx           = 12.0;  // gap entre painéis (régua→1ª track e entre tracks)
    constexpr double kRightPadPx      = 16.0;  // respiro à direita (opcional)

    // Tolerância para comparar com o valor NULL do LAS
    constexpr double kNullTol = 1e-4;

    // Paleta simples de cores para overlay (hex RGB)
    static const char* kColors[] = {
        "#1f77b4", "#ff7f0e", "#2ca02c", "#d62728",
        "#9467bd", "#8c564b", "#e377c2", "#7f7f7f",
        "#bcbd22", "#17becf"
    };

    // Filtra pontos inválidos (NULL) e devolve xs/ys filtrados
    static void FilterNulls(const std::vector<double>& xin,
                            const std::vector<double>& yin,
                            std::optional<double> nullVal,
                            std::vector<double>& xout,
                            std::vector<double>& yout)
    {
        xout.clear(); yout.clear();
        const size_t n = std::min(xin.size(), yin.size());
        if (!nullVal) {
            xout.assign(xin.begin(), xin.begin() + n);
            yout.assign(yin.begin(), yin.begin() + n);
            return;
        }
        const double nv = *nullVal;
        for (size_t i = 0; i < n; ++i) {
            double vx = xin[i];
            if (std::isfinite(nv) && std::isfinite(vx) && std::fabs(vx - nv) < kNullTol) continue;
            xout.push_back(vx);
            yout.push_back(yin[i]);
        }
    }

    // Normaliza vetor para [0,1]; se (max==min), devolve zeros
    static void Normalize01(const std::vector<double>& in, std::vector<double>& out)
    {
        out.resize(in.size());
        if (in.empty()) return;
        auto [mnIt, mxIt] = std::minmax_element(in.begin(), in.end());
        double mn = *mnIt, mx = *mxIt;
        if (mx == mn) { std::fill(out.begin(), out.end(), 0.0); return; }
        double inv = 1.0 / (mx - mn);
        for (size_t i = 0; i < in.size(); ++i) out[i] = (in[i] - mn) * inv;
    }

    // Gera um .dat (x y) para uma série e retorna o caminho do arquivo
    static std::string WriteDat(const std::string& name,
                                const std::vector<double>& x,
                                const std::vector<double>& y)
    {
        std::error_code ec;
        std::filesystem::create_directories(s_OutDir, ec);

        // sanitiza o nome do arquivo
        std::string safe = name;
        for (char& c : safe) {
            if (!(std::isalnum((unsigned char)c) || c == '_' || c == '-')) c = '_';
        }

        std::string path = s_OutDir + "/track_" + safe + ".dat";
        std::ofstream f(path);
        f.setf(std::ios::fixed);
        f.precision(8);

        const size_t n = std::min(x.size(), y.size());
        for (size_t i = 0; i < n; ++i)
            f << x[i] << " " << y[i] << "\n";

        return path;
    }

    // Escreve a régua de profundidade (coluna cinza) e retorna o caminho do .dat
    static std::string WriteDepthRuler(const std::vector<double>& depths)
    {
        std::error_code ec;
        std::filesystem::create_directories(s_OutDir, ec);

        std::string path = s_OutDir + "/track_DEPTH.dat";
        std::ofstream depthFile(path);
        depthFile.setf(std::ios::fixed);
        depthFile.precision(6);

        for (double d : depths) {
            depthFile << 0.0 << " " << d << "\n";
            depthFile << 1.0 << " " << d << "\n\n";
        }
        return path;
    }

    // Prepara um painel do multiplot (limpa estado que poderia vazar entre painéis)
    static void PreparePanel(Gnuplot& gp,
                             double originX, double width,
                             double xmin, double xmax,
                             double ymin, double ymax,
                             const std::string& xlabel,
                             bool wantLogX)
    {
        gp.Cmd(("set size "   + std::to_string(width)   + ",1").c_str());
        gp.Cmd(("set origin " + std::to_string(originX) + ",0").c_str());

        // Limpa qualquer estado persistente
        gp.Cmd("unset key");
        gp.Cmd("unset label");
        gp.Cmd("unset arrow");
        gp.Cmd("unset object");
        gp.Cmd("unset logscale x");

        // Eixos e grades
        gp.Cmd(("set xlabel '" + xlabel + "'").c_str());
        gp.Cmd("set ylabel ''");
        gp.Cmd(("set xrange [" + std::to_string(xmin) + ":" + std::to_string(xmax) + "]").c_str());
        gp.Cmd(("set yrange [" + std::to_string(ymax) + ":" + std::to_string(ymin) + "]").c_str()); // invertido (profundidade)
        gp.Cmd("set grid");

        if (wantLogX)
            gp.Cmd("set logscale x");
    }

    // Heurística para resistividade
    static bool NameLooksLikeResistivity(const std::string& curve)
    {
        if (curve == "RT" || curve == "RPD2" || curve == "RPM2" || curve == "RPS2")
            return true;
        return curve.rfind("RP", 0) == 0; // "RP*"
    }

// =====================
// API pública
// =====================

void CPlotter::PlotCurve(const CWellLog& rWell, const std::string& curveName)
{
    const auto& depth = rWell.GetDepths();
    auto it = rWell.GetCurves().find(curveName);
    if (it == rWell.GetCurves().end() || depth.empty()) {
        std::cerr << "[PlotCurve] Curva nao encontrada ou profundidade vazia: " << curveName << "\n";
        return;
    }

    const auto& vF = it->second.GetData();
    if (vF.size() != depth.size()) {
        std::cerr << "[PlotCurve] Tamanho incompatível em " << curveName << "\n";
        return;
    }

    std::vector<double> xs(vF.begin(), vF.end());
    std::vector<double> ys(depth.begin(), depth.end());

    // Filtro de NULL
    const auto nullOpt = (std::isnan(rWell.Info().null) ? std::optional<double>{}
                                                        : std::optional<double>{rWell.Info().null});
    std::vector<double> xsf, ysf;
    FilterNulls(xs, ys, nullOpt, xsf, ysf);
    if (xsf.empty()) {
        std::cerr << "[PlotCurve] Sem pontos válidos após filtrar NULL em " << curveName << "\n";
        return;
    }

    auto [mn, mx] = std::minmax_element(xsf.begin(), xsf.end());
    double xmin = *mn, xmax = *mx;
    if (xmin == xmax) { xmin -= 1.0; xmax += 1.0; }

    double ymin = *std::min_element(ysf.begin(), ysf.end());
    double ymax = *std::max_element(ysf.begin(), ysf.end());

    // Decide log X: resistividade OU unidade contendo "OHM", mas só se > 0
    bool wantLogX = NameLooksLikeResistivity(curveName) ||
                    (it->second.Unit().find("OHM") != std::string::npos);
    if (wantLogX) {
        for (double v : xsf) { if (!(v > 0.0)) { wantLogX = false; break; } }
    }

    const std::string labelX =
        it->second.Unit().empty() ? curveName : (curveName + " (" + it->second.Unit() + ")");

    std::string dat = WriteDat(curveName, xsf, ysf);

    Gnuplot gp;
    gp.Cmd("reset");
    gp.Cmd("set terminal windows size 700,900");
    PreparePanel(gp, /*originX*/0.0, /*width*/1.0, xmin, xmax, ymin, ymax, labelX, wantLogX);

    gp.Cmd(("plot '" + dat + "' using 1:2 with lines title '" + curveName + "'").c_str());

    gp.Cmd("pause mouse close");
}

void CPlotter::PlotMultiple(const CWellLog& rWell, const std::vector<std::string>& curves)
{
    for (const auto& c : curves)
        PlotCurve(rWell, c);
}

void CPlotter::PlotSelectedTracks(const CWellLog& rWell,
                                  const std::vector<std::string>& curvesToPlot)
{
    // 0) Checagem
    const auto& vecDepth = rWell.GetDepths();
    if (vecDepth.empty()) {
        std::cerr << "[PlotSelectedTracks] Sem profundidades para plotar.\n";
        return;
    }

    // 1) Lista de curvas (se vazio, usa todas as curvas do poço)
    std::vector<std::string> curves = curvesToPlot;
    if (curves.empty()) {
        for (const auto& kv : rWell.GetCurves())
            curves.push_back(kv.first);
    }
    if (curves.empty()) {
        std::cerr << "[PlotSelectedTracks] Nenhuma curva selecionada.\n";
        return;
    }

    // 2) Profundidades (ys) e estatísticas
    std::vector<double> ys(vecDepth.begin(), vecDepth.end());
    const double fMinDepth = *std::min_element(ys.begin(), ys.end());
    const double fMaxDepth = *std::max_element(ys.begin(), ys.end());

    // 3) Arquivo da régua de profundidade
    const std::string depthDat = WriteDepthRuler(ys);

    // 4) Layout do multiplot
    const int totalTracks = static_cast<int>(curves.size());
    const double trackWidth = 1.0 / static_cast<double>(totalTracks + 1);
    double currentOrigin = 0.0;

    Gnuplot gp;
    gp.Cmd("reset");
    gp.Cmd("set terminal windows size 1920,900");
    gp.Cmd("set multiplot");
    gp.Cmd("set tmargin at screen 0.95");
    gp.Cmd("set bmargin at screen 0.05");

    // 5) Coluna 0: régua de profundidade
    {
        gp.Cmd(("set size " + std::to_string(trackWidth) + ",1").c_str());
        gp.Cmd(("set origin " + std::to_string(currentOrigin) + ",0").c_str());
        gp.Cmd("unset key");
        gp.Cmd("unset label");
        gp.Cmd("unset arrow");
        gp.Cmd("unset object");
        gp.Cmd("unset xtics");
        gp.Cmd("set ytics nomirror");
        gp.Cmd("set ylabel 'Profundidade (m)'");
        gp.Cmd("set xrange [0:1]");
        gp.Cmd(("set yrange [" + std::to_string(fMaxDepth) + ":" + std::to_string(fMinDepth) + "]").c_str());
        gp.Cmd("set style fill solid 1.0");
        gp.Cmd(("plot '" + depthDat + "' using 1:2 with filledcurves x1 lc rgb 'gray' notitle").c_str());
        currentOrigin += trackWidth;
    }

    // 6) Uma curva por painel
    for (const auto& curveName : curves) {
        auto it = rWell.GetCurves().find(curveName);
        if (it == rWell.GetCurves().end()) {
            std::cerr << "[PlotSelectedTracks] Curva nao encontrada: " << curveName << "\n";
            continue;
        }

        const auto& vF = it->second.GetData();
        if (vF.size() != ys.size()) {
            std::cerr << "[PlotSelectedTracks] Tamanho incompatível: " << curveName
                      << " (" << vF.size() << " vs " << ys.size() << ")\n";
            continue;
        }

        std::vector<double> xs(vF.begin(), vF.end());

        // Filtro de NULL
        const auto nullOpt = (std::isnan(rWell.Info().null) ? std::optional<double>{}
                                                            : std::optional<double>{rWell.Info().null});
        std::vector<double> xsf, ysf;
        FilterNulls(xs, ys, nullOpt, xsf, ysf);
        if (xsf.empty()) {
            std::cerr << "[PlotSelectedTracks] Sem pontos válidos após filtrar NULL em " << curveName << "\n";
            currentOrigin += trackWidth;
            continue;
        }

        // X-range com dados filtrados
        auto [mn, mx] = std::minmax_element(xsf.begin(), xsf.end());
        double xmin = *mn, xmax = *mx;
        if (xmin == xmax) { xmin -= 1.0; xmax += 1.0; }

        // Decide se queremos log X (nome ou unidade) e se PODE logar (>0)
        bool wantLogX = NameLooksLikeResistivity(curveName) ||
                        (it->second.Unit().find("OHM") != std::string::npos);
        if (wantLogX) {
            for (double v : xsf) { if (!(v > 0.0)) { wantLogX = false; break; } }
        }

        // label com unidade
        const std::string labelX =
            it->second.Unit().empty() ? curveName : (curveName + " (" + it->second.Unit() + ")");

        // Painel
        PreparePanel(gp, currentOrigin, trackWidth, xmin, xmax, fMinDepth, fMaxDepth,
                     labelX, wantLogX);

        // .dat explícito e plot único
        const std::string dat = WriteDat(curveName, xsf, ysf);
        gp.Cmd(("plot '" + dat + "' using 1:2 with lines notitle").c_str());

        // avança para o próximo painel
        currentOrigin += trackWidth;
    }

    // 7) Fecha multiplot
    gp.Cmd("unset multiplot");

    gp.Cmd("pause mouse close");
}

// =====================
// NOVO: comparar curvas
// =====================

void CPlotter::PlotCompareCurves(const CWellLog& well,
                                 const std::vector<std::string>& curvesIn,
                                 bool overlay,
                                 bool normalize)
{
    // ------------------ dados base ------------------
    const auto& d = well.GetDepths();
    if (d.empty()) {
        std::cerr << "[PlotCompareCurves] Sem profundidades.\n";
        return;
    }
    std::vector<double> depthY(d.begin(), d.end());
    const auto nullOpt = (std::isnan(well.Info().null) ? std::optional<double>{}
                                                       : std::optional<double>{well.Info().null});

    // Seleção das curvas (mantém ordem pedida; se vazio, usa todas)
    std::vector<std::string> curves = curvesIn;
    if (curves.empty()) {
        for (const auto& kv : well.GetCurves()) curves.push_back(kv.first);
    }

    // Coleta/filtra cada curva
    struct Series { std::string name, unit, datPath; std::vector<double> x, y; };
    std::vector<Series> S;
    S.reserve(curves.size());

    for (const auto& nm : curves) {
        auto it = well.GetCurves().find(nm);
        if (it == well.GetCurves().end()) {
            std::cerr << "[PlotCompareCurves] Curva nao encontrada: " << nm << "\n";
            continue;
        }
        const auto& vf = it->second.GetData();
        if (vf.size() != depthY.size()) {
            std::cerr << "[PlotCompareCurves] Tamanho incompatível: " << nm << "\n";
            continue;
        }
        std::vector<double> xs(vf.begin(), vf.end()), ys(depthY.begin(), depthY.end());
        std::vector<double> xsf, ysf;
        FilterNulls(xs, ys, nullOpt, xsf, ysf);
        if (xsf.empty()) {
            std::cerr << "[PlotCompareCurves] Sem pontos válidos em " << nm << " após NULL.\n";
            continue;
        }
        S.push_back({ nm, it->second.Unit(), /*dat=*/"", std::move(xsf), std::move(ysf) });
    }

    // ------------------ OVERLAY ------------------
    if (overlay) {
        // Normalização opcional (0–1) por série
        if (normalize) {
            for (auto& s : S) { std::vector<double> nx; Normalize01(s.x, nx); s.x.swap(nx); }
        }

        // Ranges globais
        double xmin = +1e300, xmax = -1e300;
        for (const auto& s : S) {
            auto [mn, mx] = std::minmax_element(s.x.begin(), s.x.end());
            xmin = std::min(xmin, *mn);
            xmax = std::max(xmax, *mx);
        }
        if (xmin == xmax) { xmin -= 1.0; xmax += 1.0; }
        double ymin = *std::min_element(depthY.begin(), depthY.end());
        double ymax = *std::max_element(depthY.begin(), depthY.end());

        // .dat das séries
        for (auto& s : S) s.datPath = WriteDat(s.name, s.x, s.y);

        // Larguras em PX -> fração da janela do overlay
        const double rfrac = kOverlayRulerPx / static_cast<double>(kOverlayTermW);
        const double gfrac = kOverlayGapPx   / static_cast<double>(kOverlayTermW);
        const double tfrac = std::max(0.0, 1.0 - rfrac - gfrac); // track ocupa o restante

        Gnuplot gp;
        gp.Cmd("reset");
        gp.Cmd(("set terminal windows size " + std::to_string(kOverlayTermW) + "," + std::to_string(kOverlayTermH)).c_str());
        gp.Cmd("set multiplot");
        gp.Cmd("set tmargin at screen 0.95");
        gp.Cmd("set bmargin at screen 0.05");

        // Régua
        {
            const std::string depthDat = WriteDepthRuler(depthY);
            gp.Cmd(("set size " + std::to_string(rfrac) + ",1").c_str());
            gp.Cmd("set origin 0,0");
            gp.Cmd("unset key"); gp.Cmd("unset label"); gp.Cmd("unset arrow"); gp.Cmd("unset object"); gp.Cmd("unset xtics");
            gp.Cmd("set ytics nomirror");
            gp.Cmd("set ylabel 'Profundidade (m)'");
            gp.Cmd("set xrange [0:1]");
            gp.Cmd(("set yrange [" + std::to_string(ymax) + ":" + std::to_string(ymin) + "]").c_str());
            gp.Cmd("set style fill solid 1.0");
            gp.Cmd(("plot '" + depthDat + "' using 1:2 with filledcurves x1 lc rgb 'gray' notitle").c_str());
        }

        // Painel overlay (uma “track” colada na régua)
        {
            const double origin = rfrac + gfrac;
            gp.Cmd(("set size " + std::to_string(tfrac) + ",1").c_str());
            gp.Cmd(("set origin " + std::to_string(origin) + ",0").c_str());
            gp.Cmd("unset key"); gp.Cmd("unset label"); gp.Cmd("unset arrow"); gp.Cmd("unset object"); gp.Cmd("unset logscale x");

            std::string xlabel = "Comparacao";
            if (normalize) xlabel += " (curvas normalizadas 0-1)";
            gp.Cmd(("set xlabel '" + xlabel + "'").c_str());
            gp.Cmd("set ylabel ''");
            gp.Cmd(("set xrange [" + std::to_string(xmin) + ":" + std::to_string(xmax) + "]").c_str());
            gp.Cmd(("set yrange [" + std::to_string(ymax) + ":" + std::to_string(ymin) + "]").c_str());
            gp.Cmd("set grid");

            if (S.empty()) {
                gp.Cmd("plot 1/0 notitle");
            } else {
                std::ostringstream oss; oss << "plot ";
                for (size_t i = 0; i < S.size(); ++i) {
                    const auto& s = S[i];
                    const char* col = kColors[i % (sizeof(kColors)/sizeof(kColors[0]))];
                    std::string title = s.name;
                    if (normalize) title += " (norm.)";
                    else if (!s.unit.empty()) title += " (" + s.unit + ")";
                    oss << "'" << s.datPath << "' using 1:2 with lines lc rgb '" << col
                        << "' title '" << title << "'";
                    if (i + 1 < S.size()) oss << ", ";
                }
                gp.Cmd(oss.str());
            }
        }

        gp.Cmd("unset multiplot");
        gp.Cmd("pause mouse close");
        return;
    }

    // ------------------ SIDE-BY-SIDE ------------------
    // Ranges de profundidade
    double fMinDepth = *std::min_element(depthY.begin(), depthY.end());
    double fMaxDepth = *std::max_element(depthY.begin(), depthY.end());

    const std::string depthDat = WriteDepthRuler(depthY);
    const int n = static_cast<int>(S.size());

    // 1) Define o tamanho do terminal a partir de n
    //    régua + n*track + n*gap + padding
    int termW = static_cast<int>(
        std::ceil(kRulerPx + n * kTrackPxTarget + n * kGapPx + kRightPadPx)
    );
    // (se quiser um teto, habilite a próxima linha)
    // termW = std::min(termW, 1920);

    // 2) Converte px -> frações do terminal (para set size / set origin)
    const double rfrac = kRulerPx       / static_cast<double>(termW);
    const double tfrac = kTrackPxTarget / static_cast<double>(termW);
    const double gfrac = kGapPx         / static_cast<double>(termW);
    // (o pequeno padding fica “para fora”, não precisa fração)

    // 3) Desenho
    double origin = 0.0;

    Gnuplot gp;
    gp.Cmd("reset");
    gp.Cmd(("set terminal windows size " + std::to_string(termW) + "," + std::to_string(kSideTermH)).c_str());
    gp.Cmd("set multiplot");
    gp.Cmd("set tmargin at screen 0.95");
    gp.Cmd("set bmargin at screen 0.05");

    // --- régua com preenchimento ---
    {
        gp.Cmd(("set size " + std::to_string(rfrac) + ",1").c_str());
        gp.Cmd(("set origin " + std::to_string(origin) + ",0").c_str());

        gp.Cmd("unset key"); gp.Cmd("unset label"); gp.Cmd("unset arrow"); gp.Cmd("unset object"); gp.Cmd("unset xtics");
        gp.Cmd("set ytics nomirror");
        gp.Cmd("set ylabel 'Profundidade (m)'");
        gp.Cmd("set xrange [0:1]");
        gp.Cmd(("set yrange [" + std::to_string(fMaxDepth) + ":" + std::to_string(fMinDepth) + "]").c_str());

        // pinta o painel todo antes de plotar (independente do estilo de linhas)
        gp.Cmd("set object 1 rectangle from graph 0,0 to graph 1,1 fc rgb 'gray70' fs solid 1.0 behind");

        // um traço qualquer para materializar o eixo e a moldura
        // (pode usar o mesmo arquivo da régua, mas agora só com 'lines')
        gp.Cmd(("plot '" + depthDat + "' using 1:2 with lines lc rgb '#666666' notitle").c_str());

        // limpa o objeto para não vazar pro próximo painel
        gp.Cmd("unset object 1");

        origin += rfrac + gfrac;
    }

    // --- sem séries válidas? mostra mensagem e sai mantendo janela aberta
    if (S.empty()) {
        gp.Cmd(("set size " + std::to_string(tfrac) + ",1").c_str());
        gp.Cmd(("set origin " + std::to_string(origin) + ",0").c_str());
        gp.Cmd("unset key"); gp.Cmd("unset label"); gp.Cmd("unset arrow"); gp.Cmd("unset object");
        gp.Cmd("set xlabel 'Sem curvas válidas para comparar'");
        gp.Cmd("set ylabel ''");
        gp.Cmd("set xrange [0:1]");
        gp.Cmd(("set yrange [" + std::to_string(fMaxDepth) + ":" + std::to_string(fMinDepth) + "]").c_str());
        gp.Cmd("plot 1/0 notitle");
        gp.Cmd("unset multiplot");
        gp.Cmd("pause mouse close");
        return;
    }

    // --- uma track por painel (todas do mesmo tamanho) ---
    for (auto& s : S) {
        auto [mn, mx] = std::minmax_element(s.x.begin(), s.x.end());
        double xmin = *mn, xmax = *mx; if (xmin == xmax) { xmin -= 1.0; xmax += 1.0; }

        bool wantLogX = NameLooksLikeResistivity(s.name) || (s.unit.find("OHM") != std::string::npos);
        if (wantLogX) for (double v : s.x) { if (!(v > 0.0)) { wantLogX = false; break; } }

        const std::string labelX = s.unit.empty() ? s.name : (s.name + " (" + s.unit + ")");

        s.datPath = WriteDat(s.name, s.x, s.y);

        // painel da track
        PreparePanel(gp, origin, tfrac, xmin, xmax, fMinDepth, fMaxDepth, labelX, wantLogX);
        gp.Cmd(("plot '" + s.datPath + "' using 1:2 with lines notitle").c_str());

        origin += tfrac + gfrac;
    }

    gp.Cmd("unset multiplot");
    gp.Cmd("pause mouse close");
    return;
}
