// pbrt is Copyright(c) 1998-2020 Matt Pharr, Wenzel Jakob, and Greg Humphreys.
// It is licensed under the BSD license; see the file LICENSE.txt
// SPDX: BSD-3-Clause

#ifndef PBRT_SCATTERING_H
#define PBRT_SCATTERING_H

#include <pbrt/pbrt.h>

#include <pbrt/util/math.h>
#include <pbrt/util/spectrum.h>
#include <pbrt/util/taggedptr.h>
#include <pbrt/util/vecmath.h>

namespace pbrt {

PBRT_CPU_GPU
inline Vector3f Reflect(const Vector3f &wo, const Vector3f &n) {
    return -wo + 2 * Dot(wo, n) * n;
}

PBRT_CPU_GPU
inline bool Refract(const Vector3f &wi, const Normal3f &n, Float eta, Vector3f *wt) {
    // Compute $\cos \theta_\roman{t}$ using Snell's law
    Float cosTheta_i = Dot(n, wi);
    Float sin2Theta_i = std::max<Float>(0, 1 - cosTheta_i * cosTheta_i);
    Float sin2Theta_t = sin2Theta_i / Sqr(eta);

    // Handle total internal reflection for transmission
    if (sin2Theta_t >= 1)
        return false;
    Float cosTheta_t = SafeSqrt(1 - sin2Theta_t);
    *wt = -wi / eta + (cosTheta_i / eta - cosTheta_t) * Vector3f(n);
    return true;
}

PBRT_CPU_GPU
inline Float FrDielectric(Float cosTheta_i, Float eta) {
    cosTheta_i = Clamp(cosTheta_i, -1, 1);
    // Potentially swap indices of refraction
    bool entering = cosTheta_i > 0.f;
    if (!entering) {
        eta = 1 / eta;
        cosTheta_i = std::abs(cosTheta_i);
    }

    // Compute _cosThetaT_ using Snell's law
    Float sinTheta_i = SafeSqrt(1 - cosTheta_i * cosTheta_i);
    Float sinTheta_t = sinTheta_i / eta;

    // Handle total internal reflection
    if (sinTheta_t >= 1)
        return 1;
    Float cosTheta_t = SafeSqrt(1 - sinTheta_t * sinTheta_t);
    Float Rparl = (eta * cosTheta_i - cosTheta_t) / (eta * cosTheta_i + cosTheta_t);
    Float Rperp = (cosTheta_i - eta * cosTheta_t) / (cosTheta_i + eta * cosTheta_t);
    return (Rparl * Rparl + Rperp * Rperp) / 2;
}

// https://seblagarde.wordpress.com/2013/04/29/memo-on-fresnel-equations/
PBRT_CPU_GPU
inline SampledSpectrum FrConductor(Float cosTheta_i, const SampledSpectrum &eta,
                                   const SampledSpectrum &k) {
    cosTheta_i = Clamp(cosTheta_i, -1, 1);
    SampledSpectrum etak = k;

    Float cos2Theta_i = cosTheta_i * cosTheta_i;
    Float sin2Theta_i = 1 - cos2Theta_i;
    SampledSpectrum eta2 = eta * eta;
    SampledSpectrum etak2 = etak * etak;

    SampledSpectrum t0 = eta2 - etak2 - SampledSpectrum(sin2Theta_i);
    SampledSpectrum a2plusb2 = Sqrt(t0 * t0 + 4 * eta2 * etak2);
    SampledSpectrum t1 = a2plusb2 + SampledSpectrum(cos2Theta_i);
    SampledSpectrum a = Sqrt(0.5f * (a2plusb2 + t0));
    SampledSpectrum t2 = (Float)2 * cosTheta_i * a;
    SampledSpectrum Rs = (t1 - t2) / (t1 + t2);

    SampledSpectrum t3 =
        cos2Theta_i * a2plusb2 + SampledSpectrum(sin2Theta_i * sin2Theta_i);
    SampledSpectrum t4 = t2 * sin2Theta_i;
    SampledSpectrum Rp = Rs * (t3 - t4) / (t3 + t4);

    return 0.5f * (Rp + Rs);
}

PBRT_CPU_GPU
Float FresnelMoment1(Float invEta);
PBRT_CPU_GPU
Float FresnelMoment2(Float invEta);

class alignas(8) FresnelConductor {
  public:
    // FresnelConductor Public Methods
    PBRT_CPU_GPU
    FresnelConductor(const SampledSpectrum &eta, const SampledSpectrum &k)
        : eta(eta), k(k) {}

    PBRT_CPU_GPU
    SampledSpectrum Evaluate(Float cosTheta_i) const {
        return FrConductor(std::abs(cosTheta_i), eta, k);
    }

    std::string ToString() const;

  private:
    SampledSpectrum eta, k;
};

class alignas(8) FresnelDielectric {
  public:
    // FresnelDielectric Public Methods
    PBRT_CPU_GPU
    FresnelDielectric(Float eta, bool opaque = false) : eta(eta), opaque(opaque) {}

    PBRT_CPU_GPU
    SampledSpectrum Evaluate(Float cosTheta_i) const {
        if (opaque)
            cosTheta_i = std::abs(cosTheta_i);
        return SampledSpectrum(FrDielectric(cosTheta_i, eta));
    }

    std::string ToString() const;

  private:
    Float eta;
    bool opaque;
};

class FresnelHandle : public TaggedPointer<FresnelConductor, FresnelDielectric> {
  public:
    using TaggedPointer::TaggedPointer;
    FresnelHandle(TaggedPointer<FresnelConductor, FresnelDielectric> tp)
        : TaggedPointer(tp) {}

    PBRT_CPU_GPU
    SampledSpectrum Evaluate(Float cosTheta_i) const;
};

// MicrofacetDistribution Declarations
class alignas(8) TrowbridgeReitzDistribution {
  public:
    // TrowbridgeReitzDistribution Public Methods
    PBRT_CPU_GPU
    static inline Float RoughnessToAlpha(Float roughness);

    PBRT_CPU_GPU
    TrowbridgeReitzDistribution(Float alpha_x, Float alpha_y)
        : alpha_x(std::max<Float>(1e-4, alpha_x)),
          alpha_y(std::max<Float>(1e-4, alpha_y)) {}

    PBRT_CPU_GPU
    Float D(const Vector3f &wm) const {
        Float tan2Theta = Tan2Theta(wm);
        if (std::isinf(tan2Theta))
            return 0.;
        Float cos4Theta = Cos2Theta(wm) * Cos2Theta(wm);
        Float e =
            (Cos2Phi(wm) / (alpha_x * alpha_x) + Sin2Phi(wm) / (alpha_y * alpha_y)) *
            tan2Theta;
        return 1 / (Pi * alpha_x * alpha_y * cos4Theta * (1 + e) * (1 + e));
    }

    PBRT_CPU_GPU
    Float G(const Vector3f &wo, const Vector3f &wi) const {
        return 1 / (1 + Lambda(wo) + Lambda(wi));
    }

    PBRT_CPU_GPU
    Vector3f Sample_wm(const Point2f &u) const {
        return SampleTrowbridgeReitz(alpha_x, alpha_y, u);
    }

    PBRT_CPU_GPU
    Vector3f Sample_wm(const Vector3f &wo, const Point2f &u) const {
        bool flip = wo.z < 0;
        Vector3f wm =
            SampleTrowbridgeReitzVisibleArea(flip ? -wo : wo, alpha_x, alpha_y, u);
        if (flip)
            wm = -wm;
        return wm;
    }

    std::string ToString() const;

    PBRT_CPU_GPU
    bool EffectivelySpecular() const { return std::min(alpha_x, alpha_y) < 1e-3; }

    PBRT_CPU_GPU
    MicrofacetDistributionHandle Regularize(ScratchBuffer &scratchBuffer) const;

    PBRT_CPU_GPU
    Float Lambda(const Vector3f &w) const {
        Float tan2Theta = Tan2Theta(w);
        if (std::isinf(tan2Theta))
            return 0.;
        // Compute _alpha_ for direction _w_
        Float alpha2 = Cos2Phi(w) * alpha_x * alpha_x + Sin2Phi(w) * alpha_y * alpha_y;
        return (-1 + std::sqrt(1.f + alpha2 * tan2Theta)) / 2;
    }

  private:
    // TrowbridgeReitzDistribution Private Data
    Float alpha_x, alpha_y;
};

class MicrofacetDistributionHandle : public TaggedPointer<TrowbridgeReitzDistribution> {
  public:
    using TaggedPointer::TaggedPointer;
    MicrofacetDistributionHandle(TaggedPointer<TrowbridgeReitzDistribution> tp)
        : TaggedPointer(tp) {}

    // MicrofacetDistributionHandle Public Methods
    PBRT_CPU_GPU
    Float D(const Vector3f &wm) const {
        DCHECK_EQ(Tag(), TypeIndex<TrowbridgeReitzDistribution>());
        return Cast<TrowbridgeReitzDistribution>()->D(wm);
    }

    PBRT_CPU_GPU
    Float D(const Vector3f &w, const Vector3f &wm) const {
        return D(wm) * G1(w) * std::max<Float>(0, Dot(w, wm)) / AbsCosTheta(w);
    }

    PBRT_CPU_GPU
    Float Lambda(const Vector3f &w) const {
        DCHECK_EQ(Tag(), TypeIndex<TrowbridgeReitzDistribution>());
        return Cast<TrowbridgeReitzDistribution>()->Lambda(w);
    }

    PBRT_CPU_GPU
    Float G1(const Vector3f &w) const {
        //    if (Dot(w, wh) * CosTheta(w) < 0.) return 0.;
        return 1 / (1 + Lambda(w));
    }

    PBRT_CPU_GPU
    Float G(const Vector3f &wo, const Vector3f &wi) const {
        DCHECK_EQ(Tag(), TypeIndex<TrowbridgeReitzDistribution>());
        return Cast<TrowbridgeReitzDistribution>()->G(wo, wi);
    }

    PBRT_CPU_GPU
    Vector3f Sample_wm(const Point2f &u) const {
        DCHECK_EQ(Tag(), TypeIndex<TrowbridgeReitzDistribution>());
        return Cast<TrowbridgeReitzDistribution>()->Sample_wm(u);
    }

    PBRT_CPU_GPU
    Vector3f Sample_wm(const Vector3f &wo, const Point2f &u) const {
        DCHECK_EQ(Tag(), TypeIndex<TrowbridgeReitzDistribution>());
        return Cast<TrowbridgeReitzDistribution>()->Sample_wm(wo, u);
    }

    PBRT_CPU_GPU
    Float PDF(const Vector3f &wo, const Vector3f &wm) const {
        return D(wm) * G1(wo) * AbsDot(wo, wm) / AbsCosTheta(wo);
    }

    std::string ToString() const {
        DCHECK_EQ(Tag(), TypeIndex<TrowbridgeReitzDistribution>());
        return Cast<TrowbridgeReitzDistribution>()->ToString();
    }

    PBRT_CPU_GPU
    bool EffectivelySpecular() const {
        DCHECK_EQ(Tag(), TypeIndex<TrowbridgeReitzDistribution>());
        return Cast<TrowbridgeReitzDistribution>()->EffectivelySpecular();
    }

    PBRT_CPU_GPU
    MicrofacetDistributionHandle Regularize(ScratchBuffer &scratchBuffer) const {
        DCHECK_EQ(Tag(), TypeIndex<TrowbridgeReitzDistribution>());
        return Cast<TrowbridgeReitzDistribution>()->Regularize(scratchBuffer);
    }
};

// MicrofacetDistribution Inline Methods
inline Float TrowbridgeReitzDistribution::RoughnessToAlpha(Float roughness) {
    return std::sqrt(roughness);
}

PBRT_CPU_GPU
inline Float EvaluateHenyeyGreenstein(Float cosTheta, Float g) {
    Float denom = 1 + g * g + 2 * g * cosTheta;
    return Inv4Pi * (1 - g * g) / (denom * SafeSqrt(denom));
}

}  // namespace pbrt

#endif  //  PBRT_SCATTERING_H
