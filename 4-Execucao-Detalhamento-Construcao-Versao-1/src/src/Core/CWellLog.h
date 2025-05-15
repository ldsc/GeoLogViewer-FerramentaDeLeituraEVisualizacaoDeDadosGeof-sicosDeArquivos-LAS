#ifndef CWELL_LOG_H
#define CWELL_LOG_H

#include "CLogCurve.h"
#include <map>
#include <string>

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
};

#endif