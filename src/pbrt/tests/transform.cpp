
#include <gtest/gtest.h>

#include <pbrt/pbrt.h>
#include <pbrt/util/rng.h>
#include <pbrt/util/sampling.h>
#include <pbrt/transform.h>

using namespace pbrt;

static Transform RandomTransform(RNG &rng) {
    Transform t;
    auto r = [&rng]() { return -10. + 20. * rng.Uniform<Float>(); };
    for (int i = 0; i < 10; ++i) {
        switch (rng.Uniform<uint32_t>(3)) {
        case 0:
            t = t * Scale(std::abs(r()), std::abs(r()), std::abs(r()));
            break;
        case 1:
            t = t * Translate(Vector3f(r(), r(), r()));
            break;
        case 2:
            t = t *
                Rotate(r() * 20., SampleUniformSphere(Point2f(
                                      rng.Uniform<Float>(), rng.Uniform<Float>())));
            break;
        }
    }
    return t;
}

TEST(AnimatedTransform, Randoms) {
    RNG rng;
    auto r = [&rng]() { return -10. + 20. * rng.Uniform<Float>(); };

    for (int i = 0; i < 200; ++i) {
        // Generate a pair of random transformation matrices.
        auto t0 = std::make_unique<const Transform>(RandomTransform(rng));
        auto t1 = std::make_unique<const Transform>(RandomTransform(rng));
        AnimatedTransform at(t0.get(), 0., t1.get(), 1.);

        for (int j = 0; j < 5; ++j) {
            // Generate a random bounding box and find the bounds of its motion.
            Bounds3f bounds(Point3f(r(), r(), r()), Point3f(r(), r(), r()));
            Bounds3f motionBounds = at.MotionBounds(bounds);

            for (Float t = 0.; t <= 1.; t += 1e-3 * rng.Uniform<Float>()) {
                // Now, interpolate the transformations at a bunch of times
                // along the time range and then transform the bounding box
                // with the result.
                Transform tr = at.Interpolate(t);
                Bounds3f tb = tr(bounds);

                // Add a little slop to allow for floating-point round-off
                // error in computing the motion extrema times.
                tb.pMin += (Float)1e-4 * tb.Diagonal();
                tb.pMax -= (Float)1e-4 * tb.Diagonal();

                // Now, the transformed bounds should be inside the motion
                // bounds.
                EXPECT_GE(tb.pMin.x, motionBounds.pMin.x);
                EXPECT_LE(tb.pMax.x, motionBounds.pMax.x);
                EXPECT_GE(tb.pMin.y, motionBounds.pMin.y);
                EXPECT_LE(tb.pMax.y, motionBounds.pMax.y);
                EXPECT_GE(tb.pMin.z, motionBounds.pMin.z);
                EXPECT_LE(tb.pMax.z, motionBounds.pMax.z);
            }
        }
    }
}
