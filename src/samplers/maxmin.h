
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

#ifndef PBRT_SAMPLERS_MAXMIN_H
#define PBRT_SAMPLERS_MAXMIN_H

// samplers/maxmin.h*
#include "sampler.h"
#include "error.h"
#include "lowdiscrepancy.h"
#include "ext/google/array_slice.h"
#include <glog/logging.h>

#include <memory>

namespace pbrt {

// MaxMinDistSampler Declarations
class MaxMinDistSampler : public PixelSampler {
  public:
    // MaxMinDistSampler Public Methods
    void GeneratePixelSamples(RNG &rng);
    std::unique_ptr<Sampler> Clone();
    int RoundCount(int count) const { return RoundUpPow2(count); }
    MaxMinDistSampler(int samplesPerPixel, int nSampledDimensions)
        : PixelSampler(
              [](int spp) {
                  int Cindex = Log2Int(spp);
                  if (Cindex >= sizeof(CMaxMinDist) / sizeof(CMaxMinDist[0])) {
                      Warning(
                          "No more than %d samples per pixel are supported "
                          "with "
                          "MaxMinDistSampler. Rounding down.",
                          (1 << int(sizeof(CMaxMinDist) /
                                    sizeof(CMaxMinDist[0]))) -
                              1);
                      spp = (1 << (sizeof(CMaxMinDist) /
                                   sizeof(CMaxMinDist[0]))) -
                            1;
                  }
                  if (!IsPowerOf2(spp)) {
                      spp = RoundUpPow2(spp);
                      Warning(
                          "Non power-of-two sample count rounded up to %d "
                          "for MaxMinDistSampler.",
                          spp);
                  }
                  return spp;
              }(samplesPerPixel),
              nSampledDimensions) {
        int Cindex = Log2Int(samplesPerPixel);
        CHECK(Cindex >= 0 &&
              Cindex < (sizeof(CMaxMinDist) / sizeof(CMaxMinDist[0])));
        CPixel = CMaxMinDist[Cindex];
    }

  private:
    // MaxMinDistSampler Private Data
    gtl::ArraySlice<uint32_t> CPixel;
};

std::unique_ptr<MaxMinDistSampler> CreateMaxMinDistSampler(
    const ParamSet &params);

}  // namespace pbrt

#endif  // PBRT_SAMPLERS_MAXMIN_H
