#include "CPetroPhysicsCalculator.h"
#include <cmath>
#include <algorithm>

static inline bool fin(double v){ return std::isfinite(v); }
static inline double clamp01(double x){ return x<0?0:(x>1?1:x); }

// ----------------- Vsh -----------------

std::vector<double> CPetroPhysicsCalculator::VshLinear(const std::vector<double>& iGR){
    std::vector<double> out(iGR.size(), NAN);
    for (size_t i=0;i<iGR.size();++i)
        if (fin(iGR[i])) out[i] = clamp01(iGR[i]);
    return out;
}

std::vector<double> CPetroPhysicsCalculator::VshLarionovTertiary(const std::vector<double>& iGR){
    std::vector<double> out(iGR.size(), NAN);
    for (size_t i=0;i<iGR.size();++i)
        if (fin(iGR[i])) out[i] = clamp01(0.083*(std::pow(2.0,3.7*iGR[i])-1.0));
    return out;
}

std::vector<double> CPetroPhysicsCalculator::VshLarionovOlder(const std::vector<double>& iGR){
    std::vector<double> out(iGR.size(), NAN);
    for (size_t i=0;i<iGR.size();++i)
        if (fin(iGR[i])) out[i] = clamp01(0.33*(std::pow(2.0,2.0*iGR[i])-1.0));
    return out;
}

// ----------------- Porosidade -----------------

std::vector<double> CPetroPhysicsCalculator::PorosityDensity(const std::vector<double>& rhob,
                                                             double rhobMatrix,
                                                             double rhobFluid)
{
    const size_t N = rhob.size();
    std::vector<double> out(N, NAN);
    const double denom = (rhobMatrix - rhobFluid);
    if (!fin(denom) || denom == 0.0)
        return out;

    for (size_t i=0;i<N;++i){
        if (fin(rhob[i])) {
            double phi = (rhobMatrix - rhob[i]) / denom;
            out[i] = phi;
        }
    }
    return out;
}

std::vector<double> CPetroPhysicsCalculator::PorosityND(const std::vector<double>& nphi,
                                                        const std::vector<double>& rhob,
                                                        double rhobMatrix,
                                                        double rhobFluid)
{
    const size_t N = std::min(nphi.size(), rhob.size());
    std::vector<double> out(N, NAN);

    // phiD pela densidade
    auto phiD = PorosityDensity(rhob, rhobMatrix, rhobFluid);

    for (size_t i=0;i<N;++i){
        if (fin(nphi[i]) && fin(phiD[i])) {
            // combinacao simples: media aritmetica
            out[i] = 0.5*(nphi[i] + phiD[i]);
        }
    }
    return out;
}

// ----------------- Archie -----------------

std::vector<double> CPetroPhysicsCalculator::WaterSaturationArchie(const std::vector<double>& phi,
                                                                   const std::vector<double>& Rt,
                                                                   double a, double m, double n, double Rw)
{
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
