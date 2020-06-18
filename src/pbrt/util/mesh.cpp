// pbrt is Copyright(c) 1998-2020 Matt Pharr, Wenzel Jakob, and Greg Humphreys.
// It is licensed under the BSD license; see the file LICENSE.txt
// SPDX: BSD-3-Clause

#include <pbrt/util/mesh.h>

#include <pbrt/util/buffercache.h>
#include <pbrt/util/check.h>
#include <pbrt/util/error.h>
#include <pbrt/util/log.h>
#include <pbrt/util/print.h>
#include <pbrt/util/stats.h>
#include <pbrt/util/transform.h>

#include <rply/rply.h>

namespace pbrt {

STAT_MEMORY_COUNTER("Memory/Mesh indices", meshIndexBytes);
STAT_MEMORY_COUNTER("Memory/Mesh vertex positions", meshPositionBytes);
STAT_MEMORY_COUNTER("Memory/Mesh normals", meshNormalBytes);
STAT_MEMORY_COUNTER("Memory/Mesh uvs", meshUVBytes);
STAT_MEMORY_COUNTER("Memory/Mesh tangents", meshTangentBytes);
STAT_MEMORY_COUNTER("Memory/Mesh face indices", meshFaceIndexBytes);

STAT_RATIO("Geometry/Triangles per mesh", nTris, nTriMeshes);
STAT_MEMORY_COUNTER("Memory/Triangles", triangleBytes);

static BufferCache<int> *indexBufferCache;
static BufferCache<Point3f> *pBufferCache;
static BufferCache<Normal3f> *nBufferCache;
static BufferCache<Point2f> *uvBufferCache;
static BufferCache<Vector3f> *sBufferCache;
static BufferCache<int> *faceIndexBufferCache;

void InitBufferCaches(Allocator alloc) {
    CHECK(indexBufferCache == nullptr);
    indexBufferCache = alloc.new_object<BufferCache<int>>(alloc);
    pBufferCache = alloc.new_object<BufferCache<Point3f>>(alloc);
    nBufferCache = alloc.new_object<BufferCache<Normal3f>>(alloc);
    uvBufferCache = alloc.new_object<BufferCache<Point2f>>(alloc);
    sBufferCache = alloc.new_object<BufferCache<Vector3f>>(alloc);
    faceIndexBufferCache = alloc.new_object<BufferCache<int>>(alloc);
}

void FreeBufferCaches() {
    LOG_VERBOSE("index buffer bytes: %d", indexBufferCache->BytesUsed());
    meshIndexBytes += indexBufferCache->BytesUsed();
    indexBufferCache->Clear();

    LOG_VERBOSE("p bytes: %d", pBufferCache->BytesUsed());
    meshPositionBytes += pBufferCache->BytesUsed();
    pBufferCache->Clear();

    LOG_VERBOSE("n bytes: %d", nBufferCache->BytesUsed());
    meshNormalBytes += nBufferCache->BytesUsed();
    nBufferCache->Clear();
    LOG_VERBOSE("uv bytes: %d", uvBufferCache->BytesUsed());
    meshUVBytes += uvBufferCache->BytesUsed();
    uvBufferCache->Clear();

    LOG_VERBOSE("s bytes: %d", sBufferCache->BytesUsed());
    meshTangentBytes += sBufferCache->BytesUsed();
    sBufferCache->Clear();

    LOG_VERBOSE("face index bytes: %d", faceIndexBufferCache->BytesUsed());
    meshFaceIndexBytes += faceIndexBufferCache->BytesUsed();
    faceIndexBufferCache->Clear();
}

std::string TriangleMesh::ToString() const {
    std::string np = "(nullptr)";
    return StringPrintf(
        "[ TriangleMesh reverseOrientation: %s transformSwapsHandedness: %s "
        "nTriangles: %d nVertices: %d vertexIndices: %s p: %s n: %s "
        "s: %s uv: %s faceIndices: %s ]",
        reverseOrientation, transformSwapsHandedness, nTriangles, nVertices,
        vertexIndices ? StringPrintf("%s", pstd::MakeSpan(vertexIndices, 3 * nTriangles))
                      : np,
        p ? StringPrintf("%s", pstd::MakeSpan(p, nVertices)) : nullptr,
        n ? StringPrintf("%s", pstd::MakeSpan(n, nVertices)) : nullptr,
        s ? StringPrintf("%s", pstd::MakeSpan(s, nVertices)) : nullptr,
        uv ? StringPrintf("%s", pstd::MakeSpan(uv, nVertices)) : nullptr,
        faceIndices ? StringPrintf("%s", pstd::MakeSpan(faceIndices, nTriangles))
                    : nullptr);
}

TriangleMesh::TriangleMesh(const Transform &worldFromObject, bool reverseOrientation,
                           std::vector<int> indices, std::vector<Point3f> P,
                           std::vector<Vector3f> S, std::vector<Normal3f> N,
                           std::vector<Point2f> UV, std::vector<int> fIndices)
    : reverseOrientation(reverseOrientation),
      transformSwapsHandedness(worldFromObject.SwapsHandedness()),
      nTriangles(indices.size() / 3),
      nVertices(P.size()) {
    CHECK_EQ((indices.size() % 3), 0);
    ++nTriMeshes;
    nTris += nTriangles;

    // Make sure that we don't have too much stuff to be using integers to
    // index into things.
    CHECK_LE(P.size(), std::numeric_limits<int>::max());
    // We could be clever and check indices.size() / 3 if we were careful
    // to promote to a 64-bit int before multiplying by 3 when we look up
    // in the indices array...
    CHECK_LE(indices.size(), std::numeric_limits<int>::max());

    vertexIndices = indexBufferCache->LookupOrAdd(std::move(indices));

    triangleBytes += sizeof(*this);

    // Transform mesh vertices to world space
    for (Point3f &p : P)
        p = worldFromObject(p);
    p = pBufferCache->LookupOrAdd(std::move(P));

    // Copy _UV_, _N_, and _S_ vertex data, if present
    if (!UV.empty()) {
        CHECK_EQ(nVertices, UV.size());
        uv = uvBufferCache->LookupOrAdd(std::move(UV));
    }
    if (!N.empty()) {
        CHECK_EQ(nVertices, N.size());
        for (Normal3f &n : N) {
            n = worldFromObject(n);
            if (reverseOrientation)
                n = -n;
        }
        n = nBufferCache->LookupOrAdd(std::move(N));
    }
    if (!S.empty()) {
        CHECK_EQ(nVertices, S.size());
        for (Vector3f &s : S)
            s = worldFromObject(s);
        s = sBufferCache->LookupOrAdd(std::move(S));
    }

    if (!fIndices.empty()) {
        CHECK_EQ(nTriangles, fIndices.size());
        faceIndices = faceIndexBufferCache->LookupOrAdd(std::move(fIndices));
    }
}

static void PlyErrorCallback(p_ply, const char *message) {
    Error("PLY writing error: %s", message);
}

bool TriangleMesh::WritePLY(const std::string &filename) const {
    p_ply plyFile =
        ply_create(filename.c_str(), PLY_DEFAULT, PlyErrorCallback, 0, nullptr);
    if (plyFile == nullptr)
        return false;

    ply_add_element(plyFile, "vertex", nVertices);
    ply_add_scalar_property(plyFile, "x", PLY_FLOAT);
    ply_add_scalar_property(plyFile, "y", PLY_FLOAT);
    ply_add_scalar_property(plyFile, "z", PLY_FLOAT);
    if (n != nullptr) {
        ply_add_scalar_property(plyFile, "nx", PLY_FLOAT);
        ply_add_scalar_property(plyFile, "ny", PLY_FLOAT);
        ply_add_scalar_property(plyFile, "nz", PLY_FLOAT);
    }
    if (uv != nullptr) {
        ply_add_scalar_property(plyFile, "u", PLY_FLOAT);
        ply_add_scalar_property(plyFile, "v", PLY_FLOAT);
    }
    if (s != nullptr)
        Warning(R"(%s: PLY mesh will be missing tangent vectors "S".)", filename);

    ply_add_element(plyFile, "face", nTriangles);
    ply_add_list_property(plyFile, "vertex_indices", PLY_UINT8, PLY_INT);
    if (faceIndices != nullptr)
        ply_add_scalar_property(plyFile, "face_indices", PLY_INT);

    ply_write_header(plyFile);

    for (int i = 0; i < nVertices; ++i) {
        ply_write(plyFile, p[i].x);
        ply_write(plyFile, p[i].y);
        ply_write(plyFile, p[i].z);
        if (n != nullptr) {
            ply_write(plyFile, n[i].x);
            ply_write(plyFile, n[i].y);
            ply_write(plyFile, n[i].z);
        }
        if (uv != nullptr) {
            ply_write(plyFile, uv[i].x);
            ply_write(plyFile, uv[i].y);
        }
    }

    for (int i = 0; i < nTriangles; ++i) {
        ply_write(plyFile, 3);
        ply_write(plyFile, vertexIndices[3 * i]);
        ply_write(plyFile, vertexIndices[3 * i + 1]);
        ply_write(plyFile, vertexIndices[3 * i + 2]);
        if (faceIndices != nullptr)
            ply_write(plyFile, faceIndices[i]);
    }

    ply_close(plyFile);
    return true;
}

STAT_RATIO("Geometry/Bilinear patches per mesh", nBlps, nBilinearMeshes);
STAT_MEMORY_COUNTER("Memory/Bilinear patches", blpBytes);

BilinearPatchMesh::BilinearPatchMesh(const Transform &worldFromObject,
                                     bool reverseOrientation, std::vector<int> indices,
                                     std::vector<Point3f> P, std::vector<Normal3f> N,
                                     std::vector<Point2f> UV, std::vector<int> fIndices,
                                     PiecewiseConstant2D *imageDist)
    : reverseOrientation(reverseOrientation),
      transformSwapsHandedness(worldFromObject.SwapsHandedness()),
      nPatches(indices.size() / 4),
      nVertices(P.size()),
      imageDistribution(std::move(imageDist)) {
    CHECK_EQ((indices.size() % 4), 0);
    ++nBilinearMeshes;
    nBlps += nPatches;

    // Make sure that we don't have too much stuff to be using integers to
    // index into things.
    CHECK_LE(P.size(), std::numeric_limits<int>::max());
    CHECK_LE(indices.size(), std::numeric_limits<int>::max());

    vertexIndices = indexBufferCache->LookupOrAdd(std::move(indices));

    blpBytes += sizeof(*this);

    // Transform mesh vertices to world space
    for (Point3f &p : P)
        p = worldFromObject(p);
    p = pBufferCache->LookupOrAdd(std::move(P));

    // Copy _UV_ and _N_ vertex data, if present
    if (!UV.empty()) {
        CHECK_EQ(nVertices, UV.size());
        uv = uvBufferCache->LookupOrAdd(std::move(UV));
    }
    if (!N.empty()) {
        CHECK_EQ(nVertices, N.size());
        for (Normal3f &n : N) {
            n = worldFromObject(n);
            if (reverseOrientation)
                n = -n;
        }
        n = nBufferCache->LookupOrAdd(std::move(N));
    }

    if (!fIndices.empty()) {
        CHECK_EQ(nPatches, fIndices.size());
        faceIndices = faceIndexBufferCache->LookupOrAdd(std::move(fIndices));
    }
}

std::string BilinearPatchMesh::ToString() const {
    std::string np = "(nullptr)";
    return StringPrintf(
        "[ BilinearMatchMesh reverseOrientation: %s transformSwapsHandedness: "
        "%s "
        "nPatches: %d nVertices: %d vertexIndices: %s p: %s n: %s "
        "uv: %s faceIndices: %s ]",
        reverseOrientation, transformSwapsHandedness, nPatches, nVertices,
        vertexIndices ? StringPrintf("%s", pstd::MakeSpan(vertexIndices, 4 * nPatches))
                      : np,
        p ? StringPrintf("%s", pstd::MakeSpan(p, nVertices)) : nullptr,
        n ? StringPrintf("%s", pstd::MakeSpan(n, nVertices)) : nullptr,
        uv ? StringPrintf("%s", pstd::MakeSpan(uv, nVertices)) : nullptr,
        faceIndices ? StringPrintf("%s", pstd::MakeSpan(faceIndices, nPatches))
                    : nullptr);
}

struct FaceCallbackContext {
    int face[4];
    std::vector<int> triIndices, quadIndices;
};

void rply_message_callback(p_ply ply, const char *message) {
    Warning("rply: %s", message);
}

/* Callback to handle vertex data from RPly */
int rply_vertex_callback(p_ply_argument argument) {
    Float *buffer;
    long index, flags;

    ply_get_argument_user_data(argument, (void **)&buffer, &flags);
    ply_get_argument_element(argument, nullptr, &index);

    int stride = (flags & 0x0F0) >> 4;
    int offset = flags & 0x00F;

    buffer[index * stride + offset] = (float)ply_get_argument_value(argument);

    return 1;
}

/* Callback to handle face data from RPly */
int rply_face_callback(p_ply_argument argument) {
    FaceCallbackContext *context;
    long flags;
    ply_get_argument_user_data(argument, (void **)&context, &flags);

    long length, value_index;
    ply_get_argument_property(argument, nullptr, &length, &value_index);

    if (length != 3 && length != 4) {
        Warning("plymesh: Ignoring face with %i vertices (only triangles and quads "
                "are supported!)",
                (int)length);
        return 1;
    } else if (value_index < 0) {
        return 1;
    }

    if (value_index >= 0)
        context->face[value_index] = (int)ply_get_argument_value(argument);

    if (value_index == length - 1) {
        if (length == 3)
            for (int i = 0; i < 3; ++i)
                context->triIndices.push_back(context->face[i]);
        else {
            CHECK_EQ(length, 4);

            // Note: modify order since we're specifying it as a blp...
            context->quadIndices.push_back(context->face[0]);
            context->quadIndices.push_back(context->face[1]);
            context->quadIndices.push_back(context->face[3]);
            context->quadIndices.push_back(context->face[2]);
        }
    }

    return 1;
}

int rply_faceindex_callback(p_ply_argument argument) {
    std::vector<int> *faceIndices;
    long flags;
    ply_get_argument_user_data(argument, (void **)&faceIndices, &flags);

    faceIndices->push_back((int)ply_get_argument_value(argument));

    return 1;
}

pstd::optional<TriQuadMesh> TriQuadMesh::ReadPLY(const std::string &filename) {
    TriQuadMesh mesh;

    p_ply ply = ply_open(filename.c_str(), rply_message_callback, 0, nullptr);
    if (ply == nullptr) {
        Error("Couldn't open PLY file \"%s\"", filename);
        return {};
    }

    if (ply_read_header(ply) == 0) {
        Error("Unable to read the header of PLY file \"%s\"", filename);
        return {};
    }

    p_ply_element element = nullptr;
    size_t vertexCount = 0, faceCount = 0;

    /* Inspect the structure of the PLY file */
    while ((element = ply_get_next_element(ply, element)) != nullptr) {
        const char *name;
        long nInstances;

        ply_get_element_info(element, &name, &nInstances);
        if (strcmp(name, "vertex") == 0)
            vertexCount = nInstances;
        else if (strcmp(name, "face") == 0)
            faceCount = nInstances;
    }

    if (vertexCount == 0 || faceCount == 0) {
        Error("%s: PLY file is invalid! No face/vertex elements found!", filename);
        return {};
    }

    mesh.p.resize(vertexCount);
    if (ply_set_read_cb(ply, "vertex", "x", rply_vertex_callback, mesh.p.data(), 0x30) ==
            0 ||
        ply_set_read_cb(ply, "vertex", "y", rply_vertex_callback, mesh.p.data(), 0x31) ==
            0 ||
        ply_set_read_cb(ply, "vertex", "z", rply_vertex_callback, mesh.p.data(), 0x32) ==
            0) {
        Error("%s: Vertex coordinate property not found!", filename);
        return {};
    }

    mesh.n.resize(vertexCount);
    if (ply_set_read_cb(ply, "vertex", "nx", rply_vertex_callback, mesh.n.data(), 0x30) ==
            0 ||
        ply_set_read_cb(ply, "vertex", "ny", rply_vertex_callback, mesh.n.data(), 0x31) ==
            0 ||
        ply_set_read_cb(ply, "vertex", "nz", rply_vertex_callback, mesh.n.data(), 0x32) ==
            0)
        mesh.n.resize(0);

    /* There seem to be lots of different conventions regarding UV coordinate
     * names */
    mesh.uv.resize(vertexCount);
    if (((ply_set_read_cb(ply, "vertex", "u", rply_vertex_callback, mesh.uv.data(),
                          0x20) != 0) &&
         (ply_set_read_cb(ply, "vertex", "v", rply_vertex_callback, mesh.uv.data(),
                          0x21) != 0)) ||
        ((ply_set_read_cb(ply, "vertex", "s", rply_vertex_callback, mesh.uv.data(),
                          0x20) != 0) &&
         (ply_set_read_cb(ply, "vertex", "t", rply_vertex_callback, mesh.uv.data(),
                          0x21) != 0)) ||
        ((ply_set_read_cb(ply, "vertex", "texture_u", rply_vertex_callback,
                          mesh.uv.data(), 0x20) != 0) &&
         (ply_set_read_cb(ply, "vertex", "texture_v", rply_vertex_callback,
                          mesh.uv.data(), 0x21) != 0)) ||
        ((ply_set_read_cb(ply, "vertex", "texture_s", rply_vertex_callback,
                          mesh.uv.data(), 0x20) != 0) &&
         (ply_set_read_cb(ply, "vertex", "texture_t", rply_vertex_callback,
                          mesh.uv.data(), 0x21) != 0)))
        ;
    else
        mesh.uv.resize(0);

    FaceCallbackContext context;
    context.triIndices.reserve(faceCount * 3);
    context.quadIndices.reserve(faceCount * 4);
    if (ply_set_read_cb(ply, "face", "vertex_indices", rply_face_callback, &context, 0) ==
        0)
        ErrorExit("%s: vertex indices not found in PLY file", filename);

    if (ply_set_read_cb(ply, "face", "face_indices", rply_faceindex_callback,
                        &mesh.faceIndices, 0) != 0)
        mesh.faceIndices.reserve(faceCount);

    if (ply_read(ply) == 0)
        ErrorExit("%s: unable to read the contents of PLY file", filename);

    mesh.triIndices = std::move(context.triIndices);
    mesh.quadIndices = std::move(context.quadIndices);

    ply_close(ply);

    for (int idx : mesh.triIndices)
        if (idx < 0 || idx >= mesh.p.size())
            ErrorExit("plymesh: Vertex index %i is out of bounds! "
                      "Valid range is [0..%i)",
                      idx, int(mesh.p.size()));
    for (int idx : mesh.quadIndices)
        if (idx < 0 || idx >= mesh.p.size())
            ErrorExit("plymesh: Vertex index %i is out of bounds! "
                      "Valid range is [0..%i)",
                      idx, int(mesh.p.size()));

    return mesh;
}

}  // namespace pbrt
