#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <gp_Trsf.hxx>

struct Mesh {
    std::vector<float> vertices;
    std::vector<float> normals;
    std::vector<unsigned int> indices;
    size_t vertexCount = 0;
};

struct ColorInfo {
    bool hasColor = false;
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

// Populated from MATERIAL_DESIGNATION / PROPERTY_DEFINITION when the source
// STEP file carries structured material data. Many exports (e.g. the F6137
// valve file) don't populate this — material info there only exists as free
// text inside the PRODUCT description field, which is a text-parsing
// concern handled downstream, not here.
struct MaterialInfo {
    bool hasMaterial = false;
    std::string name;
    std::string description;
    double density = 0.0;
    std::string densityUnitName;
};

// From CAx-IF "3D Tessellated Data Validation Properties" recommended
// practice. Only present if the exporting CAD tool chose to include it.
struct ValidationProps {
    bool hasArea = false;
    bool hasVolume = false;
    bool hasCentroid = false;
    double area = 0.0;
    double volume = 0.0;
    double centroid[3] = {0.0, 0.0, 0.0};
};

// Best-effort PMI/GD&T capture. Semantic PMI support varies significantly
// across CAD systems and OCCT versions — expect this to often be empty.
struct PmiEntry {
    std::string kind;       // "Dimension" | "GeometricTolerance" | "Datum"
    std::string text;       // human-readable name/label if present
    bool hasValue = false;
    double value = 0.0;
    std::vector<std::string> refPartIds;
};

struct PartNode {
    std::string id;
    std::string name;
    size_t meshIndex = 0;
    size_t instanceCount = 1;
    bool hasGeometry = true;   // false for BOM-only items (consumables,
                                // reference placeholders) with no 3D shape
    ColorInfo color;
    MaterialInfo material;
    ValidationProps validation;
};

struct AssemblyNode {
    std::string id;
    std::string name;
    gp_Trsf transform;
    std::vector<AssemblyNode> children;
    bool isPart = false;
    size_t partIndex = 0;
};

// Lets you override auto-detection per run — useful when a file is an edge
// case the heuristic gets wrong.
enum class HierarchyMode {
    Auto,            // default: decide per free shape via TreeHasRealStructure
    ForceHierarchy,  // always use BuildAssemblyTree
    ForceFlat        // always use BuildFromGeometrySplit
};

// Per top-level free shape record of which path was actually taken and why.
// A single STEP file can contain multiple free shapes with different
// outcomes (e.g. one real assembly + one flattened leftover blob).
struct RootShapeInfo {
    std::string rootId;
    std::string rootName;
    bool hierarchyAvailable = false;
    bool wasForced = false;
};

struct ModelData {
    std::vector<Mesh> uniqueMeshes;
    std::vector<PartNode> parts;
    AssemblyNode root;
    std::unordered_map<std::string, size_t> meshCache;       // cacheKey -> mesh index
    std::unordered_map<std::string, size_t> partIndexCache;  // cacheKey -> PartNode index
    std::vector<PmiEntry> pmi;        // file-scoped, not tree-scoped
    bool hierarchyAvailable = true;   // true only if ALL roots had real structure
    std::vector<RootShapeInfo> rootReports;
};