
/*
    pbrt source code is Copyright(c) 1998-2016
                        Matt Pharr, Greg Humphreys, and Wenzel Jakob.

    This file is part of pbrt.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are
    met:

    - Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    - Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
    IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
    TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 */

#if defined(_MSC_VER)
#define NOMINMAX
#pragma once
#endif

#ifndef PBRT_INTEGRATORS_AOV_H
#define PBRT_INTEGRATORS_AOV_H

#include <pbrt/util/bounds.h>
#include <pbrt/core/camera.h>
#include <pbrt/core/integrator.h>
#include <pbrt/core/pbrt.h>
#include <pbrt/core/sampler.h>

#include <memory>

namespace pbrt {

class AOVIntegrator : public Integrator {
  public:
    // AOVIntegrator Public Methods
    AOVIntegrator(std::shared_ptr<const Camera> camera,
                  const Bounds2i &pixelBounds, int albedoSamples, int aoSamples,
                  Float aoMaxDist, int eSamples)
        : camera(camera),
          pixelBounds(pixelBounds),
          albedoSamples(albedoSamples),
          aoSamples(aoSamples),
          aoMaxDist(aoMaxDist),
          eSamples(eSamples) {}

    void Render(const Scene &scene);

  private:
    std::shared_ptr<const Camera> camera;
    const Bounds2i pixelBounds;
    int albedoSamples, aoSamples;
    const Float aoMaxDist;
    int eSamples;
};

std::unique_ptr<AOVIntegrator> CreateAOVIntegrator(
    const ParamSet &params, std::shared_ptr<const Camera> camera);

}  // namespace pbrt

#endif  // PBRT_INTEGRATORS_AOV_H