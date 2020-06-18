// pbrt is Copyright(c) 1998-2020 Matt Pharr, Wenzel Jakob, and Greg Humphreys.
// It is licensed under the BSD license; see the file LICENSE.txt
// SPDX: BSD-3-Clause

#ifndef PBRT_BASE_BXDF_H
#define PBRT_BASE_BXDF_H

#if defined(_MSC_VER)
#define NOMINMAX
#pragma once
#endif

#include <pbrt/pbrt.h>

#include <pbrt/util/pstd.h>
#include <pbrt/util/spectrum.h>
#include <pbrt/util/taggedptr.h>
#include <pbrt/util/vecmath.h>

#include <string>

namespace pbrt {

// BSDF Inline Functions
// BSDF Declarations
enum class BxDFReflTransFlags {
    Unset = 0,
    Reflection = 1 << 0,
    Transmission = 1 << 1,
    All = Reflection | Transmission
};

PBRT_CPU_GPU
inline BxDFReflTransFlags operator|(BxDFReflTransFlags a, BxDFReflTransFlags b) {
    return BxDFReflTransFlags((int)a | (int)b);
}

PBRT_CPU_GPU
inline int operator&(BxDFReflTransFlags a, BxDFReflTransFlags b) {
    return ((int)a & (int)b);
}

PBRT_CPU_GPU
inline BxDFReflTransFlags &operator|=(BxDFReflTransFlags &a, BxDFReflTransFlags b) {
    (int &)a |= int(b);
    return a;
}

std::string ToString(BxDFReflTransFlags flags);

enum class BxDFFlags {
    Unset = 0,
    Reflection = 1 << 0,
    Transmission = 1 << 1,
    Diffuse = 1 << 2,
    Glossy = 1 << 3,
    Specular = 1 << 4,
    DiffuseReflection = Diffuse | Reflection,
    DiffuseTransmission = Diffuse | Transmission,
    GlossyReflection = Glossy | Reflection,
    GlossyTransmission = Glossy | Transmission,
    SpecularReflection = Specular | Reflection,
    SpecularTransmission = Specular | Transmission,
    All = Diffuse | Glossy | Specular | Reflection | Transmission
};

PBRT_CPU_GPU
inline BxDFFlags operator|(BxDFFlags a, BxDFFlags b) {
    return BxDFFlags((int)a | (int)b);
}

PBRT_CPU_GPU
inline int operator&(BxDFFlags a, BxDFFlags b) {
    return ((int)a & (int)b);
}

PBRT_CPU_GPU
inline int operator&(BxDFFlags a, BxDFReflTransFlags b) {
    return ((int)a & (int)b);
}

PBRT_CPU_GPU
inline BxDFFlags &operator|=(BxDFFlags &a, BxDFFlags b) {
    (int &)a |= int(b);
    return a;
}

PBRT_CPU_GPU
inline bool IsReflective(BxDFFlags flags) {
    return (flags & BxDFFlags::Reflection) != 0;
}
PBRT_CPU_GPU
inline bool IsTransmissive(BxDFFlags flags) {
    return (flags & BxDFFlags::Transmission) != 0;
}
PBRT_CPU_GPU
inline bool IsDiffuse(BxDFFlags flags) {
    return (flags & BxDFFlags::Diffuse) != 0;
}
PBRT_CPU_GPU
inline bool IsGlossy(BxDFFlags flags) {
    return (flags & BxDFFlags::Glossy) != 0;
}
PBRT_CPU_GPU
inline bool IsSpecular(BxDFFlags flags) {
    return (flags & BxDFFlags::Specular) != 0;
}

std::string ToString(BxDFFlags flags);

// TransportMode Declarations
enum class TransportMode { Radiance, Importance };

PBRT_CPU_GPU
inline TransportMode operator~(TransportMode mode) {
    return (mode == TransportMode::Radiance) ? TransportMode::Importance
                                             : TransportMode::Radiance;
}

std::string ToString(TransportMode mode);

struct BSDFSample {
    BSDFSample() = default;
    PBRT_CPU_GPU
    BSDFSample(const SampledSpectrum &f, const Vector3f &wi, Float pdf, BxDFFlags flags)
        : f(f), wi(wi), pdf(pdf), flags(flags) {}

    SampledSpectrum f;
    Vector3f wi;
    Float pdf = 0;
    BxDFFlags flags;

    std::string ToString() const;

    PBRT_CPU_GPU
    bool IsReflection() const { return pbrt::IsReflective(flags); }
    PBRT_CPU_GPU
    bool IsTransmission() const { return pbrt::IsTransmissive(flags); }
    PBRT_CPU_GPU
    bool IsDiffuse() const { return pbrt::IsDiffuse(flags); }
    PBRT_CPU_GPU
    bool IsGlossy() const { return pbrt::IsGlossy(flags); }
    PBRT_CPU_GPU
    bool IsSpecular() const { return pbrt::IsSpecular(flags); }
};

// BxDFHandle Declarations
class DiffuseBxDF;
class DielectricInterfaceBxDF;
class ThinDielectricBxDF;
class SpecularReflectionBxDF;
class HairBxDF;
class MeasuredBxDF;
class MicrofacetReflectionBxDF;
class MicrofacetTransmissionBxDF;
class BSSRDFAdapter;
class CoatedDiffuseBxDF;
template <typename TopBxDF, typename BottomBxDF>
class LayeredBxDF;
using GeneralLayeredBxDF = LayeredBxDF<BxDFHandle, BxDFHandle>;

class BxDFHandle
    : public TaggedPointer<
          DiffuseBxDF, CoatedDiffuseBxDF, GeneralLayeredBxDF, DielectricInterfaceBxDF,
          ThinDielectricBxDF, SpecularReflectionBxDF, HairBxDF, MeasuredBxDF,
          MicrofacetReflectionBxDF, MicrofacetTransmissionBxDF, BSSRDFAdapter> {
  public:
    using TaggedPointer::TaggedPointer;

    // BxDF Interface
    PBRT_CPU_GPU inline SampledSpectrum f(const Vector3f &wo, const Vector3f &wi,
                                          TransportMode mode) const;

    PBRT_CPU_GPU inline pstd::optional<BSDFSample> Sample_f(
        const Vector3f &wo, Float uc, const Point2f &u, TransportMode mode,
        BxDFReflTransFlags sampleFlags = BxDFReflTransFlags::All) const;
    PBRT_CPU_GPU
    SampledSpectrum rho(const Vector3f &wo, pstd::span<const Float> uc,
                        pstd::span<const Point2f> u2) const;
    PBRT_CPU_GPU
    SampledSpectrum rho(pstd::span<const Float> uc1, pstd::span<const Point2f> u1,
                        pstd::span<const Float> uc2, pstd::span<const Point2f> u2) const;

    PBRT_CPU_GPU inline Float PDF(
        const Vector3f &wo, const Vector3f &wi, TransportMode mode,
        BxDFReflTransFlags sampleFlags = BxDFReflTransFlags::All) const;

    std::string ToString() const;

    PBRT_CPU_GPU inline BxDFHandle Regularize(ScratchBuffer &scratchBuffer);

    PBRT_CPU_GPU inline bool SampledPDFIsProportional() const;

    PBRT_CPU_GPU inline BxDFFlags Flags() const;
};

}  // namespace pbrt

#endif  // PBRT_BASE_BXDF_H
