#ifndef CPLOTTER_H
#define CPLOTTER_H

#include "../Core/CWellLog.h"
#include "../Interpretation/CInterpreter.h"
#include "CGnuplot.h"
#include <vector>
#include <string>

class CPlotter {
public:
    void PlotCurve(const CWellLog& rWell, const std::string& strCurveName);
    void PlotMultiple(const CWellLog& rWell, const std::vector<std::string>& vecCurveNames);
    void PlotSelectedTracks(const CWellLog& rWell, const std::vector<std::string>& vecCurvesSelecionadas);

    void PlotCompareCurves(const CWellLog& well,
                           const std::vector<std::string>& curves,
                           bool overlay = true,
                           bool normalize = true);

    void PlotInterpretationBasic(const CWellLog& wl,
                             const VshParams& vp = {},
                             const SepParams& sp = {});
    
    void PlotWithIntervals(const CWellLog& rWell,
                       const std::vector<std::string>& curvesToPlot,
                       const std::vector<Interval>& ivals);

};

#endif
