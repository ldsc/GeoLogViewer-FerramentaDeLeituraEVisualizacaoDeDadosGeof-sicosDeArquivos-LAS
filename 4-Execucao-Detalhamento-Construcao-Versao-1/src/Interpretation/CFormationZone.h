#ifndef CFORMATIONZONE_H
#define CFORMATIONZONE_H

#include <string>
#include <map>

class CFormationZone {
public:
    CFormationZone() : mTop(0.0), mBase(0.0) {}
    CFormationZone(double top, double base) : mTop(top), mBase(base) {}

    double Top()  const { return mTop;  }
    double Base() const { return mBase; }
    double Espessura() const { return mBase - mTop; }

    void SetTop(double z){ mTop = z; }
    void SetBase(double z){ mBase = z; }

    // propriedades medias por nome (ex.: "VSH", "PHIE", "Sw", "Rdeep")
    void SetProp(const std::string& name, double value){ mProps[name]=value; }
    bool GetProp(const std::string& name, double& out) const {
        auto it=mProps.find(name); if (it==mProps.end()) return false; out=it->second; return true;
    }
    const std::map<std::string,double>& Props() const { return mProps; }

private:
    double mTop, mBase;
    std::map<std::string,double> mProps;
};

#endif
