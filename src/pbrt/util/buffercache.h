// pbrt is Copyright(c) 1998-2020 Matt Pharr, Wenzel Jakob, and Greg Humphreys.
// It is licensed under the BSD license; see the file LICENSE.txt
// SPDX: BSD-3-Clause

#if defined(_MSC_VER)
#define NOMINMAX
#pragma once
#endif

#ifndef PBRT_UTIL_BUFFERCACHE_H
#define PBRT_UTIL_BUFFERCACHE_H

#include <pbrt/pbrt.h>

#include <pbrt/util/check.h>
#include <pbrt/util/hash.h>
#include <pbrt/util/print.h>
#include <pbrt/util/pstd.h>

#include <cstring>
#include <mutex>
#include <string>
#include <unordered_map>

namespace pbrt {

STAT_MEMORY_COUNTER("Memory/Redundant vertex and index buffers", redundantBufferBytes);
STAT_PERCENT("Geometry/Buffer cache hits", nBufferCacheHits, nBufferCacheLookups);

// BufferId stores a hash of the contents of a buffer as well as its size.
// It serves as a key for the BufferCache hash table.
struct BufferId {
    BufferId() = default;
    BufferId(const char *ptr, size_t size) : hash(HashBuffer(ptr, size)), size(size) {}

    bool operator==(const BufferId &id) const {
        return hash == id.hash && size == id.size;
    }

    std::string ToString() const {
        return StringPrintf("[ BufferId hash: %d size: %d ]", hash, size);
    }

    uint64_t hash = 0;
    size_t size = 0;
};

// Utility class that computes the hash of a BufferId, using the
// already-computed hash of its buffer.
struct BufferHasher {
    size_t operator()(const BufferId &id) const { return id.hash; }
};

// The BufferCache class lets us cases such as where a TriangleMesh is
// storing the same vertex indices, positions, uv texture coordinates,
// etc., as another TriangleMesh that has already been created.  In that
// case, the BUfferCache returns a pointer to the pre-existing buffer that
// stores those values, allowing the redundant one to be freed, thus
// reducing memory use. (This case can come up with highly complex scenes,
// especially with large amounts of procedural geometry.)
template <typename T>
class BufferCache {
  public:
    BufferCache(Allocator alloc) : alloc(alloc) {}

    const T *LookupOrAdd(std::vector<T> buf) {
        // Hash the provided buffer and see if it's already in the cache.
        // Assumes no padding in T for alignment. (TODO: can we verify this
        // at compile time?)
        BufferId id((const char *)buf.data(), buf.size() * sizeof(T));
        ++nBufferCacheLookups;
        std::lock_guard<std::mutex> lock(mutex);
        auto iter = cache.find(id);
        if (iter != cache.end()) {
            // Success; return the pointer to the start of already-existing
            // one.
            DCHECK(std::memcmp(buf.data(), iter->second->data(),
                               buf.size() * sizeof(T)) == 0);
            ++nBufferCacheHits;
            redundantBufferBytes += buf.capacity() * sizeof(T);
            return iter->second->data();
        }
        cache[id] = alloc.new_object<pstd::vector<T>>(buf.begin(), buf.end(), alloc);
        return cache[id]->data();
    }

    size_t BytesUsed() const {
        size_t sum = 0;
        for (const auto &item : cache)
            sum += item.second->capacity() * sizeof(T);
        return sum;
    }

    void Clear() {
        for (const auto &item : cache)
            alloc.delete_object(item.second);
        cache.clear();
    }

    std::string ToString() const {
        return StringPrintf("[ BufferCache cache.size(): %d BytesUsed(): %d ]",
                            cache.size(), BytesUsed());
    }

  private:
    Allocator alloc;
    std::mutex mutex;
    std::unordered_map<BufferId, pstd::vector<T> *, BufferHasher> cache;
};

}  // namespace pbrt

#endif  // PBRT_UTIL_BUFFERCACHE_H
