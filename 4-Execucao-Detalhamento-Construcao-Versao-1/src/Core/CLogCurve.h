#ifndef CLOG_CURVE_H
#define CLOG_CURVE_H

#include <string>
#include <vector>

class CLogCurve {
private:
    std::string Name;
    std::vector<float> vData;
    std::string m_unit;

public:
    CLogCurve(const std::string& Name, const std::string& Unit);

    void SetUnit(const std::string& u) { m_unit = u; }
    const std::string& Unit() const { return m_unit; }
    void AddValue(float Val);
    const std::vector<float>& GetData() const;
    const std::string& GetName() const;
    const std::string& GetUnit() const;
};

#endif