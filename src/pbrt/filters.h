// pbrt is Copyright(c) 1998-2020 Matt Pharr, Wenzel Jakob, and Greg Humphreys.
// It is licensed under the BSD license; see the file LICENSE.txt
// SPDX: BSD-3-Clause

#if defined(_MSC_VER)
#define NOMINMAX
#pragma once
#endif

#ifndef PBRT_FILTERS_BOX_H
#define PBRT_FILTERS_BOX_H

// filters.h*
#include <pbrt/pbrt.h>

#include <pbrt/base/filter.h>
#include <pbrt/util/math.h>
#include <pbrt/util/sampling.h>

#include <cmath>
#include <memory>
#include <string>

namespace pbrt {

struct FilterSample {
    Point2f p;
    Float weight;
};

class FilterSampler {
  public:
    FilterSampler(FilterHandle filter, int freq = 64, Allocator alloc = {});

    PBRT_CPU_GPU
    FilterSample Sample(const Point2f &u) const {
        Point2f p = distrib.Sample(u);
        Point2f p01 = Point2f(domain.Offset(p));
        Point2i pi(Clamp(p01.x * values.xSize() + 0.5f, 0, values.xSize() - 1),
                   Clamp(p01.y * values.ySize() + 0.5f, 0, values.ySize() - 1));
        return {p, values[pi] < 0 ? -1.f : 1.f};
    }

    std::string ToString() const;

  private:
    Bounds2f domain;
    Array2D<Float> values;
    PiecewiseConstant2D distrib;
};

// Box Filter Declarations
class FilterBase {
  public:
    PBRT_CPU_GPU
    Vector2f Radius() const { return radius; }

  protected:
    FilterBase(Vector2f radius) : radius(radius) {}
    Vector2f radius;
};

class BoxFilter : public FilterBase {
  public:
    BoxFilter(const Vector2f &radius = Vector2f(0.5, 0.5)) : FilterBase(radius) {}

    static BoxFilter *Create(const ParameterDictionary &parameters, const FileLoc *loc,
                             Allocator alloc);

    PBRT_CPU_GPU
    Float Evaluate(const Point2f &p) const {
        return (std::abs(p.x) <= radius.x && std::abs(p.y) <= radius.y) ? 1 : 0;
    }

    PBRT_CPU_GPU
    FilterSample Sample(const Point2f &u) const {
        return {Point2f(Lerp(u[0], -radius.x, radius.x), Lerp(u[1], -radius.y, radius.y)),
                1.f};
    }

    PBRT_CPU_GPU
    Float Integral() const { return 2 * radius.x * 2 * radius.y; }

    std::string ToString() const;
};

// Gaussian Filter Declarations
class GaussianFilter : public FilterBase {
  public:
    // GaussianFilter Public Methods
    GaussianFilter(const Vector2f &radius, Float sigma = 0.5f, Allocator alloc = {})
        : FilterBase(radius),
          sigma(sigma),
          expX(Gaussian(radius.x, 0, sigma)),
          expY(Gaussian(radius.y, 0, sigma)),
          sampler(this, 64, alloc) {}

    static GaussianFilter *Create(const ParameterDictionary &parameters,
                                  const FileLoc *loc, Allocator alloc);

    PBRT_CPU_GPU
    Float Evaluate(const Point2f &p) const {
        return (std::max<Float>(0, Gaussian(p.x, 0, sigma) - expX) *
                std::max<Float>(0, Gaussian(p.y, 0, sigma) - expY));
    }

    PBRT_CPU_GPU
    FilterSample Sample(const Point2f &u) const { return sampler.Sample(u); }

    PBRT_CPU_GPU
    Float Integral() const {
        return ((GaussianIntegral(-radius.x, radius.x, 0, sigma) - 2 * radius.x * expX) *
                (GaussianIntegral(-radius.y, radius.y, 0, sigma) - 2 * radius.y * expY));
    }

    std::string ToString() const;

  private:
    // GaussianFilter Private Data
    Float sigma;
    Float expX, expY;
    FilterSampler sampler;
};

// Mitchell Filter Declarations
class MitchellFilter : public FilterBase {
  public:
    // MitchellFilter Public Methods
    MitchellFilter(const Vector2f &radius, Float B = 1.f / 3.f, Float C = 1.f / 3.f,
                   Allocator alloc = {})
        : FilterBase(radius), B(B), C(C), sampler(this, 64, alloc) {}
    static MitchellFilter *Create(const ParameterDictionary &parameters,
                                  const FileLoc *loc, Allocator alloc);

    PBRT_CPU_GPU
    Float Evaluate(const Point2f &p) const {
        return Mitchell1D(p.x / radius.x) * Mitchell1D(p.y / radius.y);
    }

    PBRT_CPU_GPU
    FilterSample Sample(const Point2f &u) const { return sampler.Sample(u); }

    PBRT_CPU_GPU
    Float Integral() const {
        // integrate filters.nb
        return radius.x * radius.y / 4;
    }

    std::string ToString() const;

  private:
    Float B, C;
    FilterSampler sampler;

    PBRT_CPU_GPU
    Float Mitchell1D(Float x) const {
        x = std::abs(2 * x);
        if (x <= 1)
            return ((12 - 9 * B - 6 * C) * x * x * x + (-18 + 12 * B + 6 * C) * x * x +
                    (6 - 2 * B)) *
                   (1.f / 6.f);
        else if (x <= 2)
            return ((-B - 6 * C) * x * x * x + (6 * B + 30 * C) * x * x +
                    (-12 * B - 48 * C) * x + (8 * B + 24 * C)) *
                   (1.f / 6.f);
        else
            return 0;
    }
};

// Sinc Filter Declarations
class LanczosSincFilter : public FilterBase {
  public:
    // LanczosSincFilter Public Methods
    LanczosSincFilter(const Vector2f &radius, Float tau = 3.f, Allocator alloc = {})
        : FilterBase(radius), tau(tau), sampler(this, 64, alloc) {}

    static LanczosSincFilter *Create(const ParameterDictionary &parameters,
                                     const FileLoc *loc, Allocator alloc);

    PBRT_CPU_GPU
    Float Evaluate(const Point2f &p) const {
        return WindowedSinc(p.x, radius.x, tau) * WindowedSinc(p.y, radius.y, tau);
    }

    PBRT_CPU_GPU
    FilterSample Sample(const Point2f &u) const { return sampler.Sample(u); }

    PBRT_CPU_GPU
    Float Integral() const;

    std::string ToString() const;

  private:
    Float tau;
    FilterSampler sampler;
};

// Triangle Filter Declarations
class TriangleFilter : public FilterBase {
  public:
    TriangleFilter(const Vector2f &radius) : FilterBase(radius) {}

    static TriangleFilter *Create(const ParameterDictionary &parameters,
                                  const FileLoc *loc, Allocator alloc);

    PBRT_CPU_GPU
    Float Evaluate(const Point2f &p) const {
        return std::max<Float>(0, radius.x - std::abs(p.x)) *
               std::max<Float>(0, radius.y - std::abs(p.y));
    }

    PBRT_CPU_GPU
    FilterSample Sample(const Point2f &u) const {
        return {Point2f(SampleTent(u[0], radius.x), SampleTent(u[1], radius.y)), 1.f};
    }

    PBRT_CPU_GPU
    Float Integral() const { return radius.x * radius.x * radius.y * radius.y; }

    std::string ToString() const;
};

inline Float FilterHandle::Evaluate(const Point2f &p) const {
    auto eval = [&](auto ptr) { return ptr->Evaluate(p); };
    return Apply<Float>(eval);
}

inline FilterSample FilterHandle::Sample(const Point2f &u) const {
    auto sample = [&](auto ptr) { return ptr->Sample(u); };
    return Apply<FilterSample>(sample);
}

inline Vector2f FilterHandle::Radius() const {
    auto radius = [&](auto ptr) { return ptr->Radius(); };
    return Apply<Vector2f>(radius);
}

inline Float FilterHandle::Integral() const {
    auto integral = [&](auto ptr) { return ptr->Integral(); };
    return Apply<Float>(integral);
}

}  // namespace pbrt

#endif  // PBRT_FILTERS_H
