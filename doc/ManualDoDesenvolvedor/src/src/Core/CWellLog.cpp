#include "CWellLog.h"
#include <iostream>

void CWellLog::AddDepth(float fDepth) {
    vDepth.push_back(fDepth);
}

void CWellLog::RegisterCurve(const std::string& CurveName, const std::string& Unit) {
    mapCurves.emplace(CurveName, CLogCurve(CurveName, Unit));
}

void CWellLog::AddCurveValue(const std::string& CurveName, float Val) {
    auto it = mapCurves.find(CurveName);
    if (it != mapCurves.end()) {
        it->second.AddValue(Val);
    } else {
        std::cerr << "Curva não registrada: " << CurveName << std::endl;
    }
}

const std::vector<float>& CWellLog::GetDepths() const {
    return vDepth;
}

const std::map<std::string, CLogCurve>& CWellLog::GetCurves() const {
    return mapCurves;
}
