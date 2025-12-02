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
#include "CLogCurveUtilities.h"

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

    static void PreparePanel(Gnuplot& gp,
                         double originX, double width,
                         double xmin, double xmax,
                         double ymin, double ymax,
                         const std::string& xlabel,
                         bool wantLogX)
    {
        gp.Cmd("unset lmargin");
        gp.Cmd("unset rmargin");

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
        gp.Cmd("set ylabel ''"); // nada de ylabel nas tracks
        gp.Cmd(("set xrange [" + std::to_string(xmin) + ":" + std::to_string(xmax) + "]").c_str());
        gp.Cmd(("set yrange [" + std::to_string(ymax) + ":" + std::to_string(ymin) + "]").c_str()); // invertido (profundidade)
        gp.Cmd("set grid");

        if (wantLogX)
            gp.Cmd("set logscale x");
    }

    static bool NameLooksLikeResistivity(const std::string& curve)
    {
        if (curve == "RT" || curve == "RPD2" || curve == "RPM2" || curve == "RPS2")
            return true;
        return curve.rfind("RP", 0) == 0; // "RP*"
    }

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
        for (const auto& kv : well.GetCurves())
            curves.push_back(kv.first);
    }
    if (curves.empty()) {
        std::cerr << "[PlotCompareCurves] Nenhuma curva selecionada.\n";
        return;
    }

    struct Serie {
        std::string name;
        std::string unit;
        std::string datPath;
        std::vector<double> x;
        std::vector<double> y;
    };

    std::vector<Serie> S;
    S.reserve(curves.size());

    // monta as séries
    for (const auto& nm : curves) {
        auto it = well.GetCurves().find(nm);
        if (it == well.GetCurves().end()) {
            std::cerr << "[PlotCompareCurves] Curva nao encontrada: " << nm << "\n";
            continue;
        }

        const auto& data = it->second.GetData();
        if (data.size() != depthY.size()) {
            std::cerr << "[PlotCompareCurves] Tamanho incompatível: " << nm << "\n";
            continue;
        }

        std::vector<double> xs; xs.reserve(data.size());
        std::vector<double> ys; ys.reserve(data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            double v = data[i];
            double y = depthY[i];
            if (!std::isfinite(v)) continue;
            if (nullOpt &&
                std::fabs(v - *nullOpt) < 1e-6) continue;
            xs.push_back(v);
            ys.push_back(y);
        }
        if (xs.empty()) {
            std::cerr << "[PlotCompareCurves] Sem pontos válidos em " << nm << " após NULL.\n";
            continue;
        }

        // se for overlay, usamos o próprio depthY; se for side-by-side, mantemos xs/ys “crus”
        std::vector<double> xsf = xs;
        std::vector<double> ysf = ys;
        S.push_back({ nm, it->second.Unit(), /*dat=*/"", std::move(xsf), std::move(ysf) });
    }

    // ------------------ OVERLAY ------------------
    if (overlay) {
        // Normalização opcional (0–1) por série
        if (normalize) {
            for (auto& s : S) {
                std::vector<double> nx;
                CLogCurveUtilities::Normalize01(s.x, nx);
                s.x.swap(nx);
            }
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
        for (auto& s : S)
            s.datPath = WriteDat(s.name, s.x, s.y);

        // Layout em px
        constexpr int kOverlayTermW = 900;
        constexpr int kOverlayTermH = 800;
        constexpr double kOverlayRulerPx = 96.0;
        constexpr double kOverlayGapPx   = 16.0;

        const double rfrac = kOverlayRulerPx / static_cast<double>(kOverlayTermW);
        const double gfrac = kOverlayGapPx   / static_cast<double>(kOverlayTermW);
        const double tfrac = std::max(0.0, 1.0 - rfrac - gfrac);

        Gnuplot gp;
        gp.Cmd("reset");
        gp.Cmd("set terminal wxt");
        gp.Cmd(("set terminal windows size " + std::to_string(kOverlayTermW) + "," + std::to_string(kOverlayTermH)).c_str());
        gp.Cmd("set multiplot");
        gp.Cmd("set tmargin at screen 0.95");
        gp.Cmd("set bmargin at screen 0.05");

        // régua
        {
            const std::string depthDat = WriteDepthRuler(depthY);
            gp.Cmd(("set size " + std::to_string(rfrac) + ",1").c_str());
            gp.Cmd("set origin 0,0");
            gp.Cmd("unset key"); gp.Cmd("unset label"); gp.Cmd("unset arrow"); gp.Cmd("unset object"); gp.Cmd("unset xtics");
            gp.Cmd("set ytics nomirror");
            gp.Cmd("set ylabel 'Profundidade (m)'");
            gp.Cmd("set xrange [0:1]");
            gp.Cmd(("set yrange [" + std::to_string(ymax) + ":" + std::to_string(ymin) + "]").c_str());
            gp.Cmd(("plot '" + depthDat + "' using 1:2 with lines lc rgb '#666666' notitle").c_str());
        }

        // track overlay
        {
            gp.Cmd(("set size " + std::to_string(tfrac) + ",1").c_str());
            gp.Cmd(("set origin " + std::to_string(rfrac + gfrac) + ",0").c_str());

            gp.Cmd("unset key");
            gp.Cmd(("set xrange [" + std::to_string(xmin) + ":" + std::to_string(xmax) + "]").c_str());
            gp.Cmd(("set yrange [" + std::to_string(ymax) + ":" + std::to_string(ymin) + "]").c_str());
            gp.Cmd("set xlabel 'Curvas'");
            gp.Cmd("set ylabel 'Profundidade (m)'");

            std::ostringstream oss;
            oss << "plot ";
            bool first = true;
            int colorIdx = 0;
            for (const auto& s : S) {
                if (!first) oss << ", ";
                const char* col = kColors[colorIdx % (sizeof(kColors)/sizeof(kColors[0]))];
                oss << "'" << s.datPath << "' using 1:2 with lines lc rgb '" << col << "' title '" << s.name << "'";
                first = false;
                ++colorIdx;
            }
            if (first)
                oss << "plot 1/0 notitle";
            gp.Cmd(oss.str());
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

void CPlotter::PlotInterpretationBasic(const CWellLog& well,
                                       const VshParams& vp,
                                       const SepParams& sp,
                                       const PayParams& pp)
{
    // === 0) Derivados e intervalos ===
    auto D     = CInterpreter::ComputeDerived(well, vp, sp);
    auto picks = CInterpreter::PickReservoirs(D, pp);

    if (D.depth.empty()) {
        std::cerr << "[PlotInterpretationBasic] Sem profundidade.\n";
        return;
    }

    // Depth (eixo Y, invertido)
    std::vector<double> depth(D.depth.begin(), D.depth.end());
    const double zMin = *std::min_element(depth.begin(), depth.end());
    const double zMax = *std::max_element(depth.begin(), depth.end());

    // === 1) GR bruto ===
    std::vector<double> grx, gry;
    {
        auto it = well.GetCurves().find(vp.gr_curve);
        if (it != well.GetCurves().end()) {
            const auto& vF = it->second.GetData();
            const size_t n = std::min(vF.size(), depth.size());

            // 2.1) Coleta valores válidos, já filtrando NULL do LAS
            std::vector<double> vValid;
            vValid.reserve(n);

            const bool   hasNull = !std::isnan(well.Info().null);
            const double nullVal = well.Info().null;

            for (size_t i = 0; i < n; ++i) {
                double v = vF[i];
                if (!std::isfinite(v)) continue;
                if (hasNull && std::fabs(v - nullVal) < 1e-6) continue;
                vValid.push_back(v);
            }

            if (!vValid.empty()) {
                // 2.2) Percentil
                auto Percentil = [](std::vector<double> v, double p) -> double {
                    if (v.empty()) return std::numeric_limits<double>::quiet_NaN();
                    if (p <= 0.0)  return *std::min_element(v.begin(), v.end());
                    if (p >= 100.) return *std::max_element(v.begin(), v.end());
                    std::sort(v.begin(), v.end());
                    const double pos  = p / 100.0 * (v.size() - 1);
                    const size_t i0   = static_cast<size_t>(pos);
                    const double frac = pos - i0;
                    if (i0 + 1 < v.size())
                        return v[i0] * (1.0 - frac) + v[i0 + 1] * frac;
                    else
                        return v[i0];
                };

                const double p1  = Percentil(vValid, 1.0);
                const double p99 = Percentil(vValid, 99.0);

                // 2.3) Monta GRX/GRY só dentro de [P1, P99]
                grx.reserve(n);
                gry.reserve(n);
                for (size_t i = 0; i < n; ++i) {
                    double v = vF[i];
                    if (!std::isfinite(v)) continue;
                    if (hasNull && std::fabs(v - nullVal) < 1e-6) continue;
                    if (v < p1 || v > p99) continue; // corta spikes
                    grx.push_back(v);
                    gry.push_back(depth[i]);
                }
            }
        }
    }


    // === 2) Resistividades profundas e rasas (já de D) ===
    std::vector<double> rdx, rdy, rsx, rsy;
    {
        const size_t n = depth.size();
        rdx.reserve(n); rdy.reserve(n);
        rsx.reserve(n); rsy.reserve(n);

        for (size_t i = 0; i < n; ++i) {
            if (i < D.rdeep.size() && std::isfinite(D.rdeep[i]) && D.rdeep[i] > 0.0) {
                rdx.push_back(D.rdeep[i]);
                rdy.push_back(depth[i]);
            }
            if (i < D.rshal.size() && std::isfinite(D.rshal[i]) && D.rshal[i] > 0.0) {
                rsx.push_back(D.rshal[i]);
                rsy.push_back(depth[i]);
            }
        }
    }

    if (grx.empty() && rdx.empty() && rsx.empty()) {
        std::cerr << "[PlotInterpretationBasic] Nao ha dados validos de GR ou resistividade.\n";
        return;
    }

    // === 3) Ranges deduzidos dos dados ===
    auto mm_lin = [](const std::vector<double>& v) {
        double mn = +1e300, mx = -1e300;
        bool ok = false;
        for (double x : v) {
            if (std::isfinite(x)) { ok = true; mn = std::min(mn, x); mx = std::max(mx, x); }
        }
        if (!ok) { mn = 0.0; mx = 1.0; }
        if (mn == mx) {
            mn -= 1.0;
            mx += 1.0;
        } else {
            // dá um "respiro" de 5% nas bordas
            double pad = 0.05 * (mx - mn);
            mn -= pad;
            mx += pad;
        }
        return std::pair<double,double>(mn, mx);
    };

    auto mm_pos = [](const std::vector<double>& v) {
        double mn = +1e300, mx = -1e300;
        bool ok = false;
        for (double x : v) {
            if (std::isfinite(x) && x > 0.0) { ok = true; mn = std::min(mn, x); mx = std::max(mx, x); }
        }
        if (!ok) { mn = 0.1; mx = 10.0; }
        if (mn == mx) {
            mn /= 10.0;
            mx *= 10.0;
        } else {
            double factor = std::pow(10.0, 0.1); // ~1.26
            mn /= factor;
            mx *= factor;
        }
        return std::pair<double,double>(mn, mx);
    };

    auto [grMin, grMax] = mm_lin(grx);
    auto [rdMin, rdMax] = mm_pos(rdx);
    auto [rsMin, rsMax] = mm_pos(rsx);
    double rxMin = std::min(rdMin, rsMin);
    double rxMax = std::max(rdMax, rsMax);

    // === 4) Arquivos .dat ===
    const std::string depthDat = WriteDepthRuler(depth);
    const std::string grDat    = WriteDat("GR",     grx, gry);
    const std::string rdeepDat = WriteDat("RDEEP",  rdx, rdy);
    const std::string rshalDat = WriteDat("RSHAL",  rsx, rsy);

    // === 5) Layout do multiplot ===
    const int termH = 800;
    const double kRulerPx    = 80.0;
    const double kTrackPx    = 260.0;  // largura de cada track
    const double kGapPx      = 16.0;
    const double kRightPadPx = 20.0;

    const int nTracks = 2; // GR + Resistividade
    int termW = static_cast<int>(std::ceil(kRulerPx + nTracks * kTrackPx + (nTracks - 1) * kGapPx + kRightPadPx));

    const double rulerFrac = kRulerPx / termW;
    const double trackFrac = kTrackPx / termW;
    const double gapFrac   = kGapPx   / termW;

    Gnuplot gp;
    gp.Cmd("reset");
    gp.Cmd(("set terminal windows size " + std::to_string(termW) + "," + std::to_string(termH)).c_str());
    gp.Cmd("set multiplot");
    gp.Cmd("set tmargin at screen 0.96");
    gp.Cmd("set bmargin at screen 0.08");

    // === 6) Régua de profundidade ===
    {
        gp.Cmd(("set size " + std::to_string(rulerFrac) + ",1").c_str());
        gp.Cmd("set origin 0,0");

        gp.Cmd("unset key; unset label; unset arrow; unset object");
        gp.Cmd("unset xtics; set ytics nomirror");

        gp.Cmd("set xrange [0:1]");
        gp.Cmd(("set yrange [" + std::to_string(zMax) + ":" + std::to_string(zMin) + "]").c_str());

        // fundo cinza
        gp.Cmd("set object 900 rect from graph 0,0 to graph 1,1 behind fc rgb '#545454' fs solid 1.0 noborder");
        gp.Cmd(("plot '" + depthDat + "' using 1:2 with lines lc rgb '#dddddd' notitle").c_str());

        gp.Cmd("unset object 900");
    }

    // === 7) Track GR (apenas GR + sombras de pay) ===
    {
        const double originX = rulerFrac + gapFrac;
        gp.Cmd(("set size " + std::to_string(trackFrac) + ",1").c_str());
        gp.Cmd(("set origin " + std::to_string(originX) + ",0").c_str());

        gp.Cmd("unset key; unset arrow; unset object");
        gp.Cmd("set grid xtics ytics");
        gp.Cmd(("set yrange [" + std::to_string(zMax) + ":" + std::to_string(zMin) + "]").c_str());
        gp.Cmd(("set xrange [" + std::to_string(grMin) + ":" + std::to_string(grMax) + "]").c_str());
        gp.Cmd("unset logscale x");
        gp.Cmd("set xlabel 'GR (GAPI)'");

        // sombras de pay
        int objId = 1000;
        for (const auto& iv : picks) {
            std::ostringstream oss;
            oss << "set object " << objId++
                << " rect from graph 0, first " << iv.top
                << " to graph 1, first " << iv.base
                << " behind fc rgb '#fff7c2' fs solid 1.0 noborder";
            gp.Cmd(oss.str());
        }

        // curva GR fina (linha padrão)
        std::ostringstream cmd;
        cmd << "plot ";
        if (!grx.empty()) {
            cmd << "'" << grDat << "' using 1:2 with lines lc rgb '#7b68ee' title 'GR'";
        } else {
            cmd << "1/0 notitle";
        }
        gp.Cmd(cmd.str());

        // opcional: limpar objetos se quiser
        // gp.Cmd("unset object");
    }

    // === 8) Track Resistividades (Rdeep/Rshal) ===
    {
        const double originX = rulerFrac + gapFrac + trackFrac + gapFrac;
        gp.Cmd(("set size " + std::to_string(trackFrac) + ",1").c_str());
        gp.Cmd(("set origin " + std::to_string(originX) + ",0").c_str());

        gp.Cmd("unset key; unset arrow; unset object");
        gp.Cmd("set grid xtics ytics");
        gp.Cmd(("set yrange [" + std::to_string(zMax) + ":" + std::to_string(zMin) + "]").c_str());
        gp.Cmd(("set xrange [" + std::to_string(rxMin) + ":" + std::to_string(rxMax) + "]").c_str());
        gp.Cmd("set logscale x");
        gp.Cmd("set xlabel 'Resistividades (ohm.m)'");

        // sombras de pay
        int objId = 2000;
        for (const auto& iv : picks) {
            std::ostringstream oss;
            oss << "set object " << objId++
                << " rect from graph 0, first " << iv.top
                << " to graph 1, first " << iv.base
                << " behind fc rgb '#fff7c2' fs solid 1.0 noborder";
            gp.Cmd(oss.str());
        }

        std::ostringstream cmd;
        cmd << "plot ";
        bool first = true;

        if (!rdx.empty()) {
            cmd << "'" << rdeepDat << "' using 1:2 with lines lc rgb '#1f77b4' title 'Rdeep'";
            first = false;
        }
        if (!rsx.empty()) {
            if (!first) cmd << ", ";
            cmd << "'" << rshalDat << "' using 1:2 with lines lc rgb '#ff7f0e' title 'Rshal'";
            first = false;
        }
        if (first)
            cmd << "1/0 notitle";

        gp.Cmd(cmd.str());
    }

    gp.Cmd("unset multiplot");
    gp.Cmd("pause mouse close");
}


// overload curto: usa cortes de pay “sensatos”
void CPlotter::PlotInterpretationBasic(const CWellLog& well,
                                       const VshParams& vp,
                                       const SepParams& sp)
{
    PayParams pp;
    pp.vsh_max       = 0.35;  // Vsh máximo
    pp.RR_min        = 3.0;   // mínimo de Rdeep/Rshal
    pp.dR_min        = 1.0;   // separação mínima
    pp.min_thickness = 3.0;   // espessura mínima (m)

    PlotInterpretationBasic(well, vp, sp, pp);
}

void CPlotter::PlotWithIntervals(const CWellLog& rWell,
                                 const std::vector<std::string>& curvesToPlot,
                                 const std::vector<Interval>& ivals)
{
    const auto& vecDepth = rWell.GetDepths();
    if (vecDepth.empty()) { std::cerr << "[PlotWithIntervals] Sem profundidades.\n"; return; }

    // 1) Curvas
    std::vector<std::string> curves = curvesToPlot;
    if (curves.empty()) {
        for (const auto& kv : rWell.GetCurves()) curves.push_back(kv.first);
    }
    if (curves.empty()) { std::cerr << "[PlotWithIntervals] Nenhuma curva selecionada.\n"; return; }

    // 2) Depth e ranges
    std::vector<double> ys(vecDepth.begin(), vecDepth.end());
    const double fMinDepth = *std::min_element(ys.begin(), ys.end());
    const double fMaxDepth = *std::max_element(ys.begin(), ys.end());

    // Escreve régua (garante que pelo menos isso plote)
    const std::string depthDat = WriteDepthRuler(ys);

    // 3) Layout compacto px
    const int totalTracks = static_cast<int>(curves.size());
    constexpr int    kTermH       = 900;
    constexpr double kRulerPx     = 96.0;
    constexpr double kTrackPx     = 200.0;
    constexpr double kGapPx       = 14.0;
    constexpr double kRightPadPx  = 12.0;

    int termW = static_cast<int>(std::ceil(kRulerPx + totalTracks*kTrackPx + totalTracks*kGapPx + kRightPadPx));
    const double rfrac = kRulerPx / termW;
    const double tfrac = kTrackPx / termW;
    const double gfrac = kGapPx   / termW;

    Gnuplot gp;
    gp.Cmd("reset");
    gp.Cmd(("set terminal windows size " + std::to_string(termW) + "," + std::to_string(kTermH)).c_str());
    gp.Cmd("set multiplot");
    gp.Cmd("set tmargin at screen 0.95");
    gp.Cmd("set bmargin at screen 0.05");

    double origin = 0.0;

    // 4) Régua (preenchida) — SEMPRE PLOTA
    {
        gp.Cmd(("set size " + std::to_string(rfrac) + ",1").c_str());
        gp.Cmd(("set origin " + std::to_string(origin) + ",0").c_str());
        gp.Cmd("unset key"); gp.Cmd("unset label"); gp.Cmd("unset arrow"); gp.Cmd("unset object"); gp.Cmd("unset xtics");
        gp.Cmd("set ytics nomirror");
        gp.Cmd("set ylabel 'Profundidade (m)'");
        gp.Cmd("set xrange [0:1]");
        gp.Cmd(("set yrange [" + std::to_string(fMaxDepth) + ":" + std::to_string(fMinDepth) + "]").c_str());
        gp.Cmd("set object 1 rectangle from graph 0,0 to graph 1,1 fc rgb 'gray70' fs solid 1.0 behind");
        gp.Cmd(("plot '" + depthDat + "' using 1:2 with lines lc rgb '#666' notitle").c_str());
        gp.Cmd("unset object 1");
        origin += rfrac + gfrac;
    }

    // 5) Tracks
    const auto nullOpt = (std::isnan(rWell.Info().null) ? std::optional<double>{}
                                                          : std::optional<double>{rWell.Info().null});
    int plotted_tracks = 0;
    int object_id_base = 1000;

    for (const auto& curveName : curves) {
        auto it = rWell.GetCurves().find(curveName);
        if (it == rWell.GetCurves().end()) {
            std::cerr << "[PlotWithIntervals] Curva nao encontrada: " << curveName << "\n";
            origin += tfrac + gfrac; continue;
        }

        const auto& vF = it->second.GetData();
        if (vF.size() != ys.size()) {
            std::cerr << "[PlotWithIntervals] Size mismatch " << curveName << " data=" << vF.size() << " depth=" << ys.size() << "\n";
            origin += tfrac + gfrac; continue;
        }

        std::vector<double> xs(vF.begin(), vF.end());
        std::vector<double> xsf, ysf;
        FilterNulls(xs, ys, nullOpt, xsf, ysf);

        if (xsf.empty()) {
            std::cerr << "[PlotWithIntervals] Sem pontos validos em " << curveName << " apos filtro.\n";
            origin += tfrac + gfrac; continue;
        }

        auto [mn, mx] = std::minmax_element(xsf.begin(), xsf.end());
        double xmin = *mn, xmax = *mx;
        if (xmin == xmax) { xmin -= 1.0; xmax += 1.0; }

        bool wantLogX = NameLooksLikeResistivity(curveName) ||
                        (it->second.Unit().find("OHM") != std::string::npos);
        if (wantLogX) for (double v : xsf) { if (!(v > 0.0)) { wantLogX = false; break; } }

        const std::string labelX = it->second.Unit().empty()
            ? curveName : (curveName + " (" + it->second.Unit() + ")");

        PreparePanel(gp, origin, tfrac, xmin, xmax, fMinDepth, fMaxDepth, labelX, wantLogX);

        // sombreia intervalos (toda largura)
        int oid = object_id_base;
        for (const auto& I : ivals) {
            std::ostringstream os;
            os << "set object " << oid++ << " rectangle from graph 0, first "
               << I.top << " to graph 1, first " << I.base
               << " fc rgb '#fff59d' fs solid 0.35 noborder behind";
            gp.Cmd(os.str());
        }

        const std::string dat = WriteDat(curveName, xsf, ysf);
        gp.Cmd(("plot '" + dat + "' using 1:2 with lines lc rgb '#6a1b9a' notitle").c_str());
        plotted_tracks++;

        // limpa objetos do painel
        for (int id = object_id_base; id < oid; ++id)
            gp.Cmd(("unset object " + std::to_string(id)).c_str());
        object_id_base += 1000;

        origin += tfrac + gfrac;
    }

    gp.Cmd("unset multiplot");

    // Segura a janela mesmo se nada foi plotado (além da régua)
    gp.Cmd("pause mouse close");
}

