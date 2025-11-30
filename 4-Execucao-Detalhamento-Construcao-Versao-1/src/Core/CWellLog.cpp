#include "CWellLog.h"
#include <iostream>

void CWellLog::AddDepth(float fDepth) {
    vDepth.push_back(fDepth);
}

void CWellLog::RegisterCurve(const std::string& sCurveName, const std::string& Unit) {
    mapCurves.emplace(sCurveName, CLogCurve(sCurveName, Unit));
}

void CWellLog::AddCurveValue(const std::string& sCurveName, float Val) {
    auto it = mapCurves.find(sCurveName);
    if (it != mapCurves.end()) {
        it->second.AddValue(Val);
    } else {
        std::cerr << "Curva não registrada: " << sCurveName << std::endl;
    }
}

const std::vector<float>& CWellLog::GetDepths() const {
    return vDepth;
}

const std::map<std::string, CLogCurve>& CWellLog::GetCurves() const {
    return mapCurves;
}
