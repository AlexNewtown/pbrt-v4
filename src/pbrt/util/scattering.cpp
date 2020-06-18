// pbrt is Copyright(c) 1998-2020 Matt Pharr, Wenzel Jakob, and Greg Humphreys.
// It is licensed under the BSD license; see the file LICENSE.txt
// SPDX: BSD-3-Clause

#include <pbrt/util/scattering.h>

namespace pbrt {

Float FresnelMoment1(Float eta) {
    Float eta2 = eta * eta, eta3 = eta2 * eta, eta4 = eta3 * eta, eta5 = eta4 * eta;
    if (eta < 1)
        return 0.45966f - 1.73965f * eta + 3.37668f * eta2 - 3.904945 * eta3 +
               2.49277f * eta4 - 0.68441f * eta5;
    else
        return -4.61686f + 11.1136f * eta - 10.4646f * eta2 + 5.11455f * eta3 -
               1.27198f * eta4 + 0.12746f * eta5;
}

Float FresnelMoment2(Float eta) {
    Float eta2 = eta * eta, eta3 = eta2 * eta, eta4 = eta3 * eta, eta5 = eta4 * eta;
    if (eta < 1) {
        return 0.27614f - 0.87350f * eta + 1.12077f * eta2 - 0.65095f * eta3 +
               0.07883f * eta4 + 0.04860f * eta5;
    } else {
        Float r_eta = 1 / eta, r_eta2 = r_eta * r_eta, r_eta3 = r_eta2 * r_eta;
        return -547.033f + 45.3087f * r_eta3 - 218.725f * r_eta2 + 458.843f * r_eta +
               404.557f * eta - 189.519f * eta2 + 54.9327f * eta3 - 9.00603f * eta4 +
               0.63942f * eta5;
    }
}

std::string FresnelConductor::ToString() const {
    return StringPrintf("[ FresnelConductor eta: %s k: %s ]", eta, k);
}

std::string FresnelDielectric::ToString() const {
    return StringPrintf("[ FrenselDielectric eta: %f opaque: %s ]", eta,
                        opaque ? "true" : "false");
}

// Microfacet Utility Functions
// MicrofacetDistribution Method Definitions

std::string TrowbridgeReitzDistribution::ToString() const {
    return StringPrintf("[ TrowbridgeReitzDistribution alpha_x: %f alpha_y: %f ]",
                        alpha_x, alpha_y);
}

MicrofacetDistributionHandle TrowbridgeReitzDistribution::Regularize(
    ScratchBuffer &scratchBuffer) const {
    if (alpha_x >= 0.3f && alpha_y >= 0.3f)
        return this;
    return scratchBuffer.Alloc<TrowbridgeReitzDistribution>(
        std::max<Float>(alpha_x, 0.3f), std::max<Float>(alpha_y, 0.3f));
}

}  // namespace pbrt
