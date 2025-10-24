#ifndef CPLOTTER_H
#define CPLOTTER_H

#include "../Core/CWellLog.h"
#include "CGnuplot.h"
#include <vector>
#include <string>

class CPlotter {
public:
    void PlotCurve(const CWellLog& rWell, const std::string& strCurveName);
    void PlotMultiple(const CWellLog& rWell, const std::vector<std::string>& vecCurveNames);
    void PlotCompositeTracks(const CWellLog& rWell, const std::vector<std::string>& vecCurveNames);
    void PlotSelectedTracks(const CWellLog& rWell, const std::vector<std::string>& vecCurvesSelecionadas);

private:
    void PlotSingleTrack(const CWellLog& rWell, const std::string& strCurveName);
};

#endif
