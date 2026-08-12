#include "TirePropertyImportInternal.hpp"

#include <string>
#include <utility>

namespace heritage::vehicles::tires::authoring_detail {

void resetImportedCoefficientDefaults(MagicFormula62Parameters& p)
{
    // MagicFormula62Parameters also serves as Heritage's compatibility seed,
    // so several coefficients intentionally have useful non-zero defaults. A
    // property-file import must never inherit those synthetic values when a
    // .tir omits an optional coefficient. Zero is the neutral imported default
    // for force/moment coefficients; scaling coefficients retain their neutral
    // value of one. Core shape/stiffness terms must therefore be supplied by
    // the imported dataset if meaningful forces are expected.
    p.pCx1 = p.pDx1 = p.pDx2 = p.pEx1 = p.pKx1 = p.pKx2 = 0.0;
    p.rBx1 = p.rBx2 = p.rCx1 = 0.0;

    p.pCy1 = p.pDy1 = p.pDy2 = p.pDy3 = p.pEy1 = 0.0;
    p.pKy1 = p.pKy2 = p.pKy4 = p.pKy6 = 0.0;
    p.rBy1 = p.rBy2 = p.rCy1 = p.rVy5 = p.rVy6 = 0.0;

    p.qSx5 = p.qSx6 = p.qSx9 = p.qSx11 = 0.0;
    p.qSy1 = p.qSy3 = p.qSy7 = p.qSy8 = 0.0;
    p.qBz1 = p.qBz9 = p.qCz1 = p.qDz1 = 0.0;
}

bool hasCoreForceCoefficients(const RawPropertyFile& raw, std::string& missing)
{
    const std::pair<const char*, const char*> required[] = {
        { "LONGITUDINAL_COEFFICIENT", "PCX1" },
        { "LONGITUDINAL_COEFFICIENT", "PDX1" },
        { "LONGITUDINAL_COEFFICIENT", "PKX1" },
        { "LATERAL_COEFFICIENT", "PCY1" },
        { "LATERAL_COEFFICIENT", "PDY1" },
        { "LATERAL_COEFFICIENT", "PKY1" },
        { "LATERAL_COEFFICIENT", "PKY2" },
        { "LATERAL_COEFFICIENT", "PKY4" }
    };
    for (const auto& [section, key] : required)
    {
        if (!rawValue(raw, section, key))
        {
            if (!missing.empty()) missing += ", ";
            missing += "[" + std::string(section) + "] " + key;
        }
    }
    return missing.empty();
}

void mapMagicFormulaCoefficients(const RawPropertyFile& raw, TirePropertyFileData& d)
{
    MagicFormula62Parameters& p = d.magicFormula;
#define MAP(SECTION, KEY, MEMBER) mapScalar(raw, d, SECTION, KEY, p.MEMBER)

    // Scaling coefficients.
    MAP("SCALING_COEFFICIENTS", "LFZO", lFz0);
    MAP("SCALING_COEFFICIENTS", "LCX", lCx);
    MAP("SCALING_COEFFICIENTS", "LMUX", lMux);
    MAP("SCALING_COEFFICIENTS", "LEX", lEx);
    MAP("SCALING_COEFFICIENTS", "LKX", lKxk);
    MAP("SCALING_COEFFICIENTS", "LHX", lHx);
    MAP("SCALING_COEFFICIENTS", "LVX", lVx);
    MAP("SCALING_COEFFICIENTS", "LXAL", lXalpha);
    MAP("SCALING_COEFFICIENTS", "LCY", lCy);
    MAP("SCALING_COEFFICIENTS", "LMUY", lMuy);
    MAP("SCALING_COEFFICIENTS", "LEY", lEy);
    MAP("SCALING_COEFFICIENTS", "LKY", lKya);
    MAP("SCALING_COEFFICIENTS", "LKYC", lKygamma);
    MAP("SCALING_COEFFICIENTS", "LHY", lHy);
    MAP("SCALING_COEFFICIENTS", "LVY", lVy);
    MAP("SCALING_COEFFICIENTS", "LYKA", lYkappa);
    MAP("SCALING_COEFFICIENTS", "LVYKA", lVykappa);
    MAP("SCALING_COEFFICIENTS", "LMX", lMx);
    MAP("SCALING_COEFFICIENTS", "LVMX", lVMx);
    MAP("SCALING_COEFFICIENTS", "LMY", lMy);
    MAP("SCALING_COEFFICIENTS", "LMP", lMp);
    MAP("SCALING_COEFFICIENTS", "LTR", lT);
    MAP("SCALING_COEFFICIENTS", "LRES", lMzr);
    MAP("SCALING_COEFFICIENTS", "LKZC", lKzgamma);
    MAP("SCALING_COEFFICIENTS", "LS", lS);
    mapScalar(raw, d, "SCALING_COEFFICIENTS", "LSGKP", d.lSgKappa);
    mapScalar(raw, d, "SCALING_COEFFICIENTS", "LSGAL", d.lSgAlpha);

    // Longitudinal force.
    const char* x = "LONGITUDINAL_COEFFICIENT";
    MAP(x, "PCX1", pCx1); MAP(x, "PDX1", pDx1); MAP(x, "PDX2", pDx2); MAP(x, "PDX3", pDx3);
    MAP(x, "PEX1", pEx1); MAP(x, "PEX2", pEx2); MAP(x, "PEX3", pEx3); MAP(x, "PEX4", pEx4);
    MAP(x, "PKX1", pKx1); MAP(x, "PKX2", pKx2); MAP(x, "PKX3", pKx3);
    MAP(x, "PHX1", pHx1); MAP(x, "PHX2", pHx2); MAP(x, "PVX1", pVx1); MAP(x, "PVX2", pVx2);
    MAP(x, "PPX1", ppX1); MAP(x, "PPX2", ppX2); MAP(x, "PPX3", ppX3); MAP(x, "PPX4", ppX4);
    MAP(x, "RBX1", rBx1); MAP(x, "RBX2", rBx2); MAP(x, "RBX3", rBx3); MAP(x, "RCX1", rCx1);
    MAP(x, "REX1", rEx1); MAP(x, "REX2", rEx2); MAP(x, "RHX1", rHx1);
    mapScalar(raw, d, x, "PTX1", d.pTx1); mapScalar(raw, d, x, "PTX2", d.pTx2); mapScalar(raw, d, x, "PTX3", d.pTx3);

    // Overturning moment.
    const char* mx = "OVERTURNING_COEFFICIENTS";
    MAP(mx, "QSX1", qSx1); MAP(mx, "QSX2", qSx2); MAP(mx, "QSX3", qSx3); MAP(mx, "QSX4", qSx4);
    MAP(mx, "QSX5", qSx5); MAP(mx, "QSX6", qSx6); MAP(mx, "QSX7", qSx7); MAP(mx, "QSX8", qSx8);
    MAP(mx, "QSX9", qSx9); MAP(mx, "QSX10", qSx10); MAP(mx, "QSX11", qSx11); MAP(mx, "QSX12", qSx12);
    MAP(mx, "QSX13", qSx13); MAP(mx, "QSX14", qSx14); MAP(mx, "PPMX1", ppMx1);

    // Lateral force.
    const char* y = "LATERAL_COEFFICIENT";
    MAP(y, "PCY1", pCy1); MAP(y, "PDY1", pDy1); MAP(y, "PDY2", pDy2); MAP(y, "PDY3", pDy3);
    MAP(y, "PEY1", pEy1); MAP(y, "PEY2", pEy2); MAP(y, "PEY3", pEy3); MAP(y, "PEY4", pEy4); MAP(y, "PEY5", pEy5);
    MAP(y, "PKY1", pKy1); MAP(y, "PKY2", pKy2); MAP(y, "PKY3", pKy3); MAP(y, "PKY4", pKy4);
    MAP(y, "PKY5", pKy5); MAP(y, "PKY6", pKy6); MAP(y, "PKY7", pKy7);
    MAP(y, "PHY1", pHy1); MAP(y, "PHY2", pHy2);
    MAP(y, "PVY1", pVy1); MAP(y, "PVY2", pVy2); MAP(y, "PVY3", pVy3); MAP(y, "PVY4", pVy4);
    MAP(y, "PPY1", ppY1); MAP(y, "PPY2", ppY2); MAP(y, "PPY3", ppY3); MAP(y, "PPY4", ppY4); MAP(y, "PPY5", ppY5);
    MAP(y, "RBY1", rBy1); MAP(y, "RBY2", rBy2); MAP(y, "RBY3", rBy3); MAP(y, "RBY4", rBy4);
    MAP(y, "RCY1", rCy1); MAP(y, "REY1", rEy1); MAP(y, "REY2", rEy2); MAP(y, "RHY1", rHy1); MAP(y, "RHY2", rHy2);
    MAP(y, "RVY1", rVy1); MAP(y, "RVY2", rVy2); MAP(y, "RVY3", rVy3); MAP(y, "RVY4", rVy4); MAP(y, "RVY5", rVy5); MAP(y, "RVY6", rVy6);
    mapScalar(raw, d, y, "PTY1", d.pTy1); mapScalar(raw, d, y, "PTY2", d.pTy2);

    // Rolling resistance.
    const char* my = "ROLLING_COEFFICIENTS";
    MAP(my, "QSY1", qSy1); MAP(my, "QSY2", qSy2); MAP(my, "QSY3", qSy3); MAP(my, "QSY4", qSy4);
    MAP(my, "QSY5", qSy5); MAP(my, "QSY6", qSy6); MAP(my, "QSY7", qSy7); MAP(my, "QSY8", qSy8);

    // Aligning moment.
    const char* z = "ALIGNING_COEFFICIENTS";
    MAP(z, "QBZ1", qBz1); MAP(z, "QBZ2", qBz2); MAP(z, "QBZ3", qBz3); MAP(z, "QBZ4", qBz4); MAP(z, "QBZ5", qBz5);
    MAP(z, "QBZ9", qBz9); MAP(z, "QBZ10", qBz10); MAP(z, "QCZ1", qCz1);
    MAP(z, "QDZ1", qDz1); MAP(z, "QDZ2", qDz2); MAP(z, "QDZ3", qDz3); MAP(z, "QDZ4", qDz4);
    MAP(z, "QDZ6", qDz6); MAP(z, "QDZ7", qDz7); MAP(z, "QDZ8", qDz8); MAP(z, "QDZ9", qDz9); MAP(z, "QDZ10", qDz10); MAP(z, "QDZ11", qDz11);
    MAP(z, "QEZ1", qEz1); MAP(z, "QEZ2", qEz2); MAP(z, "QEZ3", qEz3); MAP(z, "QEZ4", qEz4); MAP(z, "QEZ5", qEz5);
    MAP(z, "QHZ1", qHz1); MAP(z, "QHZ2", qHz2); MAP(z, "QHZ3", qHz3); MAP(z, "QHZ4", qHz4);
    MAP(z, "SSZ1", sSz1); MAP(z, "SSZ2", sSz2); MAP(z, "SSZ3", sSz3); MAP(z, "SSZ4", sSz4);
    MAP(z, "PPZ1", ppZ1); MAP(z, "PPZ2", ppZ2);

    // MF6.2 turn-slip/spin coefficients. These are optional; a zero-valued
    // set leaves the TIRE01/TIRE02 steady-state behavior unchanged.
    const char* ts = "TURNSLIP_COEFFICIENTS";
    MAP(ts, "PDXP1", pDxP1); MAP(ts, "PDXP2", pDxP2); MAP(ts, "PDXP3", pDxP3);
    MAP(ts, "PKYP1", pKyP1);
    MAP(ts, "PDYP1", pDyP1); MAP(ts, "PDYP2", pDyP2); MAP(ts, "PDYP3", pDyP3); MAP(ts, "PDYP4", pDyP4);
    MAP(ts, "PHYP1", pHyP1); MAP(ts, "PHYP2", pHyP2); MAP(ts, "PHYP3", pHyP3); MAP(ts, "PHYP4", pHyP4);
    MAP(ts, "PECP1", pEcP1); MAP(ts, "PECP2", pEcP2);
    MAP(ts, "QDTP1", qDtP1); MAP(ts, "QCRP1", qCrP1); MAP(ts, "QCRP2", qCrP2);
    MAP(ts, "QBRP1", qBrP1); MAP(ts, "QDRP1", qDrP1);

#undef MAP
}

} // namespace heritage::vehicles::tires::authoring_detail
