#ifndef CWELL_LOG_H
#define CWELL_LOG_H

#include "CLogCurve.h"
#include <map>
#include <string>
#include <limits>      // std::numeric-limits
#include <cmath>       // std::isnan

struct SWellInfo {
    double null  = std::numeric_limits<double>::quiet_NaN();
    double strt  = std::numeric_limits<double>::quiet_NaN();
    double stop  = std::numeric_limits<double>::quiet_NaN();
    double step  = std::numeric_limits<double>::quiet_NaN();
};

class CWellLog {
private:
    std::vector<float> vDepth;
    std::map<std::string, CLogCurve> mapCurves;
    
public:
    void AddDepth(float fDepth);
    void AddCurveValue(const std::string& sCurveName, float Val);
    void RegisterCurve(const std::string& sCurveName, const std::string& Unit);

    const std::vector<float>& GetDepths() const;
    const std::map<std::string, CLogCurve>& GetCurves() const;

    const SWellInfo& Info() const { return m_info; }
    void SetInfo(const SWellInfo& info) { m_info = info; }

    private:
    SWellInfo m_info;

};

#endif