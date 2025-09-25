#include "CLogCurve.h"

CLogCurve::CLogCurve(const std::string& Name, const std::string& Unit)
    : Name(Name), Unit(Unit) {}

void CLogCurve::AddValue(float Val) {
    vData.push_back(Val);
}

const std::vector<float>& CLogCurve::GetData() const {
    return vData;  
}

const std::string& CLogCurve::GetName() const {
    return Name; 
}

const std::string& CLogCurve::GetUnit() const {
    return Unit; 
}
