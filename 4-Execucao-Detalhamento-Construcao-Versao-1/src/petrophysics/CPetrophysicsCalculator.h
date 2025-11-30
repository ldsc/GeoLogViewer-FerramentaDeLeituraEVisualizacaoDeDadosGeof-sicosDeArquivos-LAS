#ifndef CPETROPHYSICSCALCULATOR_H
#define CPETROPHYSICSCALCULATOR_H

#include <vector>
#include <string>

class CPetroPhysicsCalculator {
public:
    // Vsh a partir de GR já normalizado (0..1) ou via IGamma opcional
    static std::vector<double> VshLinear(const std::vector<double>& iGR01);
    static std::vector<double> VshLarionovTertiary(const std::vector<double>& iGR01);
    static std::vector<double> VshLarionovOlder(const std::vector<double>& iGR01);

    // Porosidade efetiva
    // Densidade: phi = (RHOBma - RHOB) / (RHOBma - RHOBfl)
    static std::vector<double> PorosityDensity(const std::vector<double>& rhob,
                                               double rhobMatrix, double rhobFluid);

    // Neutron-Density simples: media/combinação (ex.: arenitos)
    static std::vector<double> PorosityND(const std::vector<double>& nphi,
                                          const std::vector<double>& rhob,
                                          double rhobMatrix, double rhobFluid);

    // Archie (água): Sw^n = (a*Rw)/(phi^m * Rt)
    static std::vector<double> WaterSaturationArchie(const std::vector<double>& phi,
                                                     const std::vector<double>& Rt,
                                                     double a, double m, double n, double Rw);
};

#endif
