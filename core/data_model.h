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

struct PartNode {
    std::string id;
    std::string name;
    size_t meshIndex = 0;
    size_t instanceCount = 1;
};

struct AssemblyNode {
    std::string id;
    std::string name;
    gp_Trsf transform;
    std::vector<AssemblyNode> children;
    bool isPart = false;
    size_t partIndex = 0;
};

struct ModelData {
    std::vector<Mesh> uniqueMeshes;
    std::vector<PartNode> parts;
    AssemblyNode root;
    std::unordered_map<std::string, size_t> meshCache;
};