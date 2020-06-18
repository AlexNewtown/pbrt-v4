// pbrt is Copyright(c) 1998-2020 Matt Pharr, Wenzel Jakob, and Greg Humphreys.
// It is licensed under the BSD license; see the file LICENSE.txt
// SPDX: BSD-3-Clause

#include <gtest/gtest.h>

#include <pbrt/util/hash.h>

#include <set>

using namespace pbrt;

TEST(Hash, VarArgs) {
    int buf[] = {1, -12511, 31415821, 37};
    for (int i = 0; i < 4; ++i)
        EXPECT_EQ(HashBuffer(buf + i, sizeof(int)), Hash(buf[i]));
}

TEST(Hash, Collisions) {
    std::set<uint32_t> low, high;
    std::set<uint64_t> full;

    int lowCollisions = 0, highCollisions = 0;
    int fullCollisions = 0;
    int same = 0;
    for (int i = 0; i < 10000000; ++i) {
        uint64_t h = Hash(h);

        if (h == i)
            ++same;

        if (low.find(h) != low.end())
            ++lowCollisions;
        if (high.find(h >> 32) != high.end())
            ++highCollisions;
        if (full.find(h >> 32) != full.end())
            ++fullCollisions;
    }

    // It's actually potentially legit if any of these hit; it should
    // shouldn't happen a lot.
    EXPECT_EQ(0, same);
    EXPECT_EQ(0, lowCollisions);
    EXPECT_EQ(0, highCollisions);
    EXPECT_EQ(0, fullCollisions);
}
