#include "glb_exporter.h"
#include <tiny_gltf.h>
#include <Utils/logger.h>
#include <gp_Trsf.hxx>
#include <gp_Mat.hxx>
#include <functional>

// glTF/GLB requires meters. OCCT's STEP reader normalizes all imported
// geometry to millimeters internally regardless of the source file's
// declared unit — so every position and translation must be scaled down
// by this factor before export, or models come out 1000x too large in any
// spec-compliant viewer.
static constexpr double MM_TO_M = 0.001;

bool GlbExporter::Export(const ModelData& model, const std::string& glbPath) {
    Logger::Info("Starting GLB export: " + glbPath);

    tinygltf::Model gltf;
    gltf.asset.version   = "2.0";
    gltf.asset.generator = "CAD_STEP_to_GLB";

    tinygltf::Buffer buffer;

    auto appendToBuffer = [&](const void* data, size_t byteSize) -> size_t {
        size_t offset = buffer.data.size();
        const unsigned char* ptr = reinterpret_cast<const unsigned char*>(data);
        buffer.data.insert(buffer.data.end(), ptr, ptr + byteSize);
        while (buffer.data.size() % 4 != 0)
            buffer.data.push_back(0);
        return offset;
    };

    for (const auto& m : model.uniqueMeshes) {
        tinygltf::Mesh gltfMesh;
        tinygltf::Primitive primitive;

        // --- POSITIONS (scaled mm -> m) ---
        {
            std::vector<float> scaledVerts(m.vertices.size());
            for (size_t i = 0; i < m.vertices.size(); ++i) {
                scaledVerts[i] = (float)(m.vertices[i] * MM_TO_M);
            }

            size_t byteOffset = appendToBuffer(
                scaledVerts.data(),
                scaledVerts.size() * sizeof(float)
            );

            tinygltf::BufferView bv;
            bv.buffer     = 0;
            bv.byteOffset = byteOffset;
            bv.byteLength = scaledVerts.size() * sizeof(float);
            bv.target     = TINYGLTF_TARGET_ARRAY_BUFFER;
            int bvIdx = (int)gltf.bufferViews.size();
            gltf.bufferViews.push_back(bv);

            std::vector<double> minPos = { DBL_MAX,  DBL_MAX,  DBL_MAX};
            std::vector<double> maxPos = {-DBL_MAX, -DBL_MAX, -DBL_MAX};
            for (size_t i = 0; i + 2 < scaledVerts.size(); i += 3) {
                minPos[0] = std::min(minPos[0], (double)scaledVerts[i]);
                minPos[1] = std::min(minPos[1], (double)scaledVerts[i+1]);
                minPos[2] = std::min(minPos[2], (double)scaledVerts[i+2]);
                maxPos[0] = std::max(maxPos[0], (double)scaledVerts[i]);
                maxPos[1] = std::max(maxPos[1], (double)scaledVerts[i+1]);
                maxPos[2] = std::max(maxPos[2], (double)scaledVerts[i+2]);
            }

            tinygltf::Accessor acc;
            acc.bufferView    = bvIdx;
            acc.byteOffset    = 0;
            acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
            acc.count         = scaledVerts.size() / 3;
            acc.type          = TINYGLTF_TYPE_VEC3;
            acc.minValues     = minPos;
            acc.maxValues     = maxPos;
            primitive.attributes["POSITION"] = (int)gltf.accessors.size();
            gltf.accessors.push_back(acc);
        }

        // --- NORMALS (unaffected by scale) ---
        if (!m.normals.empty()) {
            size_t byteOffset = appendToBuffer(
                m.normals.data(),
                m.normals.size() * sizeof(float)
            );

            tinygltf::BufferView bv;
            bv.buffer     = 0;
            bv.byteOffset = byteOffset;
            bv.byteLength = m.normals.size() * sizeof(float);
            bv.target     = TINYGLTF_TARGET_ARRAY_BUFFER;
            int bvIdx = (int)gltf.bufferViews.size();
            gltf.bufferViews.push_back(bv);

            tinygltf::Accessor acc;
            acc.bufferView    = bvIdx;
            acc.byteOffset    = 0;
            acc.componentType = TINYGLTF_COMPONENT_TYPE_FLOAT;
            acc.count         = m.normals.size() / 3;
            acc.type          = TINYGLTF_TYPE_VEC3;
            primitive.attributes["NORMAL"] = (int)gltf.accessors.size();
            gltf.accessors.push_back(acc);
        }

        // --- INDICES ---
        {
            size_t byteOffset = appendToBuffer(
                m.indices.data(),
                m.indices.size() * sizeof(unsigned int)
            );

            tinygltf::BufferView bv;
            bv.buffer     = 0;
            bv.byteOffset = byteOffset;
            bv.byteLength = m.indices.size() * sizeof(unsigned int);
            bv.target     = TINYGLTF_TARGET_ELEMENT_ARRAY_BUFFER;
            int bvIdx = (int)gltf.bufferViews.size();
            gltf.bufferViews.push_back(bv);

            tinygltf::Accessor acc;
            acc.bufferView    = bvIdx;
            acc.byteOffset    = 0;
            acc.componentType = TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
            acc.count         = m.indices.size();
            acc.type          = TINYGLTF_TYPE_SCALAR;
            primitive.indices = (int)gltf.accessors.size();
            gltf.accessors.push_back(acc);
        }

        primitive.mode = TINYGLTF_MODE_TRIANGLES;
        gltfMesh.primitives.push_back(primitive);
        gltf.meshes.push_back(gltfMesh);
    }

    gltf.buffers.push_back(buffer);

    std::vector<tinygltf::Node> nodes;

    std::function<int(const AssemblyNode&)> buildNode =
        [&](const AssemblyNode& node) -> int {

        tinygltf::Node gltfNode;
        gltfNode.name = node.name.empty() ? ("Node_" + node.id) : node.name;
        tinygltf::Value::Object extrasObj;
        extrasObj["partId"] = tinygltf::Value(node.id);
        gltfNode.extras = tinygltf::Value(extrasObj);

        std::array<double, 16> matrix{};
        ConvertTransform(node.transform, matrix);
        gltfNode.matrix = std::vector<double>(matrix.begin(), matrix.end());

        // Only attach a mesh if this part actually has geometry — BOM-only
        // items (consumables, placeholders) stay as named nodes with no mesh.
        if (node.isPart && node.partIndex < model.parts.size() &&
            model.parts[node.partIndex].hasGeometry) {
            gltfNode.mesh = (int)model.parts[node.partIndex].meshIndex;
        }

        for (const auto& child : node.children) {
            int childIdx = buildNode(child);
            gltfNode.children.push_back(childIdx);
        }

        int idx = (int)nodes.size();
        nodes.push_back(gltfNode);
        return idx;
    };

    int rootIdx = buildNode(model.root);
    gltf.nodes = std::move(nodes);

    tinygltf::Scene scene;
    scene.name = "Scene";
    scene.nodes.push_back(rootIdx);
    gltf.scenes.push_back(scene);
    gltf.defaultScene = 0;

    tinygltf::TinyGLTF writer;
    bool success = writer.WriteGltfSceneToFile(
        &gltf, glbPath,
        /*embedImages=*/true,
        /*embedBuffers=*/true,
        /*prettyPrint=*/false,
        /*writeBinary=*/true
    );

    Logger::Info(success ? "GLB export successful!" : "GLB export FAILED!");
    return success;
}

void GlbExporter::ConvertTransform(const gp_Trsf& trsf,
                                    std::array<double, 16>& matrix) {
    const gp_Mat& mat = trsf.VectorialPart();
    gp_XYZ trans      = trsf.TranslationPart();

    // Translation is in mm from OCCT — scale to meters. Rotation/scale part
    // of the matrix is dimensionless, unaffected.
    matrix = {
        mat(1,1), mat(2,1), mat(3,1), 0.0,
        mat(1,2), mat(2,2), mat(3,2), 0.0,
        mat(1,3), mat(2,3), mat(3,3), 0.0,
        trans.X() * MM_TO_M, trans.Y() * MM_TO_M, trans.Z() * MM_TO_M, 1.0
    };
}