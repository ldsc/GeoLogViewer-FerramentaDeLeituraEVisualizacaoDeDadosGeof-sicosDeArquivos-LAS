#ifndef CLOG_CURVE_H
#define CLOG_CURVE_H

#include <string>
#include <vector>

class CLogCurve {
private:
    std::string Name;
    std::string Unit;
    std::vector<float> vData;

public:
    CLogCurve(const std::string& Name, const std::string& Unit);

    void AddValue(float Val);
    const std::vector<float>& GetData() const;
    const std::string& GetName() const;
    const std::string& GetUnit() const;
};

#endif