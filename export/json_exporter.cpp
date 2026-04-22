#include "json_exporter.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <Utils/logger.h>

using json = nlohmann::json;

bool JsonExporter::Export(const ModelData& model, const std::string& jsonPath) {
    Logger::Info("Exporting JSON assembly tree and BOM: " + jsonPath);

    json j;
    j["name"] = "CAD Assembly";
    j["total_parts"] = model.parts.size();
    j["unique_meshes"] = model.uniqueMeshes.size();

    // Assembly Tree
    json tree;
    std::function<void(const AssemblyNode&, json&)> buildTree = 
        [&](const AssemblyNode& node, json& jnode) {
            jnode["id"] = node.id;
            jnode["name"] = node.name;
            jnode["isPart"] = node.isPart;
            
            if (node.isPart && node.partIndex < model.parts.size()) {
                const auto& part = model.parts[node.partIndex];
                jnode["meshIndex"] = part.meshIndex;
                jnode["instanceCount"] = part.instanceCount;
            }

            if (!node.children.empty()) {
                jnode["children"] = json::array();
                for (const auto& child : node.children) {
                    json childJson;
                    buildTree(child, childJson);
                    jnode["children"].push_back(childJson);
                }
            }
        };

    buildTree(model.root, tree);
    j["assembly_tree"] = tree;

    // Simple BOM (Bill of Materials)
    json bom = json::array();
    for (const auto& part : model.parts) {
        json item;
        item["id"] = part.id;
        item["name"] = part.name;
        item["instances"] = part.instanceCount;
        item["meshIndex"] = part.meshIndex;
        bom.push_back(item);
    }
    j["bom"] = bom;

    std::ofstream file(jsonPath);
    if (file.is_open()) {
        file << j.dump(4);   // pretty print with 4 spaces
        file.close();
        Logger::Info("JSON export successful");
        return true;
    }

    Logger::Error("Failed to write JSON file");
    return false;
}