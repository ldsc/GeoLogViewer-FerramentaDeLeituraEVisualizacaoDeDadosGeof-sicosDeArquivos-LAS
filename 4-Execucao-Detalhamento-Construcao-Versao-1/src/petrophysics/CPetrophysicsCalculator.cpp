#include "CPetroPhysicsCalculator.h"
#include <cmath>
#include <algorithm>

static inline bool fin(double v){ return std::isfinite(v); }
static inline double clamp01(double x){ return x<0?0:(x>1?1:x); }

std::vector<double> CPetroPhysicsCalculator::VshLinear(const std::vector<double>& iGR){
    std::vector<double> out(iGR.size(), NAN);
    for (size_t i=0;i<iGR.size();++i) if (fin(iGR[i])) out[i] = clamp01(iGR[i]);
    return out;
}
std::vector<double> CPetroPhysicsCalculator::VshLarionovTertiary(const std::vector<double>& iGR){
    std::vector<double> out(iGR.size(), NAN);
    for (size_t i=0;i<iGR.size();++i) if (fin(iGR[i])) out[i] = clamp01(0.083*(std::pow(2.0,3.7*iGR[i])-1.0));
    return out;
}
std::vector<double> CPetroPhysicsCalculator::VshLarionovOlder(const std::vector<double>& iGR){
    std::vector<double> out(iGR.size(), NAN);
    for (size_t i=0;i<iGR.size();++i) if (fin(iGR[i])) out[i] = clamp01(0.33*(std::pow(2.0,2.0*iGR[i])-1.0));
    return out;
}

std::vector<double> CPetroPhysicsCalculator::PorosityDensity(const std::vector<double>& rhob,
                                                             double rhobMatrix, double rhobFluid){
    std::vector<double> out(rhob.size(), NAN);
    const double denom = (rhobMatrix - rhobFluid);
    for (size_t i=0;i<rhob.size();++i) if (fin(rhob[i]) && denom!=0.0) {
        out[i] = clamp01((rhobMatrix - rhob[i]) / denom);
    }
    return out;
}

std::vector<double> CPetroPhysicsCalculator::PorosityND(const std::vector<double>& nphi,
                                                        const std::vector<double>& rhob,
                                                        double rhobMatrix, double rhobFluid){
    const size_t n = std::min(nphi.size(), rhob.size());
    auto phid = PorosityDensity(rhob, rhobMatrix, rhobFluid);
    phid.resize(n);
    std::vector<double> out(n, NAN);
    for (size_t i=0;i<n;++i){
        if (fin(nphi[i]) && fin(phid[i])) out[i] = clamp01(0.5*(nphi[i] + phid[i]));
        else if (fin(nphi[i])) out[i] = clamp01(nphi[i]);
        else if (fin(phid[i])) out[i] = clamp01(phid[i]);
    }
    return out;
}

std::vector<double> CPetroPhysicsCalculator::WaterSaturationArchie(const std::vector<double>& phi,
                                                                   const std::vector<double>& Rt,
                                                                   double a, double m, double n, double Rw){
    const size_t N = std::min(phi.size(), Rt.size());
    std::vector<double> out(N, NAN);
    for (size_t i=0;i<N;++i){
        if (fin(phi[i]) && fin(Rt[i]) && phi[i]>0 && Rt[i]>0 && Rw>0){
            double num = a*Rw;
            double den = std::pow(phi[i], m)*Rt[i];
            if (den>0){
                double swn = num/den;
                if (swn>=0) out[i] = clamp01(std::pow(swn, 1.0/n));
            }
        }
    }
    return out;
}
