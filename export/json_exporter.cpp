#include "json_exporter.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <Utils/logger.h>

using json = nlohmann::json;

static json ColorToJson(const ColorInfo& c) {
    if (!c.hasColor) return nullptr;
    return json{{"r", c.r}, {"g", c.g}, {"b", c.b}};
}

static json MaterialToJson(const MaterialInfo& m) {
    if (!m.hasMaterial) return nullptr;
    json j;
    j["name"] = m.name;
    if (!m.description.empty()) j["description"] = m.description;
    if (m.density > 0.0) {
        j["density"] = m.density;
        if (!m.densityUnitName.empty()) j["densityUnit"] = m.densityUnitName;
    }
    return j;
}

static json ValidationToJson(const ValidationProps& v) {
    if (!v.hasArea && !v.hasVolume && !v.hasCentroid) return nullptr;
    json j;
    if (v.hasArea)     j["area"] = v.area;
    if (v.hasVolume)   j["volume"] = v.volume;
    if (v.hasCentroid) j["centroid"] = {v.centroid[0], v.centroid[1], v.centroid[2]};
    return j;
}

static json MetadataToJson(const DocumentMetadata& m) {
    json j;
    if (!m.originalFileName.empty())    j["originalFileName"] = m.originalFileName;
    if (!m.timestamp.empty())           j["timestamp"] = m.timestamp;
    if (!m.author.empty())              j["author"] = m.author;
    if (!m.organization.empty())        j["organization"] = m.organization;
    if (!m.originatingSystem.empty())   j["originatingSystem"] = m.originatingSystem;
    if (!m.preprocessorVersion.empty()) j["preprocessorVersion"] = m.preprocessorVersion;
    if (!m.schema.empty())              j["schema"] = m.schema;
    if (!m.descriptionNotes.empty())    j["notes"] = m.descriptionNotes;
    return j;
}

bool JsonExporter::Export(const ModelData& model, const std::string& jsonPath) {
    Logger::Info("Exporting JSON assembly tree and BOM: " + jsonPath);

    json j;
    j["name"] = "CAD Assembly";
    j["document"] = MetadataToJson(model.documentMetadata); 
    j["total_parts"] = model.parts.size();
    j["unique_meshes"] = model.uniqueMeshes.size();
    j["hierarchy_available"] = model.hierarchyAvailable;
    if (!model.hierarchyAvailable) {
        j["note"] = "Source STEP file had no usable assembly structure for "
                     "at least one root shape. See root_shapes for per-root "
                     "detail — affected parts were derived by splitting "
                     "disjoint solid bodies and grouping by outer-surface "
                     "color, not by the original product hierarchy.";
    }

    json rootReports = json::array();
    for (const auto& r : model.rootReports) {
        json rr;
        rr["name"] = r.rootName;
        rr["hierarchyAvailable"] = r.hierarchyAvailable;
        rr["wasForced"] = r.wasForced;
        rootReports.push_back(rr);
    }
    j["root_shapes"] = rootReports;

    json tree;
    std::function<void(const AssemblyNode&, json&)> buildTree =
        [&](const AssemblyNode& node, json& jnode) {
            jnode["id"] = node.id;
            jnode["name"] = node.name;
            jnode["isPart"] = node.isPart;

            if (node.isPart && node.partIndex < model.parts.size()) {
                const auto& part = model.parts[node.partIndex];
                jnode["hasGeometry"] = part.hasGeometry;
                if (part.hasGeometry) {
                    jnode["meshIndex"] = part.meshIndex;
                }
                jnode["instanceCount"] = part.instanceCount;

                json color = ColorToJson(part.color);
                if (!color.is_null()) jnode["color"] = color;

                json material = MaterialToJson(part.material);
                if (!material.is_null()) jnode["material"] = material;

                json validation = ValidationToJson(part.validation);
                if (!validation.is_null()) jnode["validationProperties"] = validation;

                if (!part.partNumber.empty())      jnode["partNumber"] = part.partNumber;
                if (!part.descriptionText.empty())  jnode["description"] = part.descriptionText;
                if (!part.layers.empty())           jnode["layers"] = part.layers;
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

    json bom = json::array();
    for (const auto& part : model.parts) {
        json item;
        item["id"] = part.id;
        item["name"] = part.name;
        item["instances"] = part.instanceCount;
        item["hasGeometry"] = part.hasGeometry;
        if (part.hasGeometry) {
            item["meshIndex"] = part.meshIndex;
        }

        json color = ColorToJson(part.color);
        if (!color.is_null()) item["color"] = color;

        json material = MaterialToJson(part.material);
        if (!material.is_null()) item["material"] = material;

        json validation = ValidationToJson(part.validation);
        
        if (!validation.is_null()) item["validationProperties"] = validation;
        if (!part.partNumber.empty())      item["partNumber"] = part.partNumber;
        if (!part.descriptionText.empty())  item["description"] = part.descriptionText;
        if (!part.layers.empty())           item["layers"] = part.layers;

        bom.push_back(item);
    }
    j["bom"] = bom;

    if (!model.pmi.empty()) {
        json pmiArray = json::array();
        for (const auto& p : model.pmi) {
            json pj;
            pj["kind"] = p.kind;
            if (!p.text.empty()) pj["text"] = p.text;
            if (p.hasValue) pj["value"] = p.value;
            if (!p.refPartIds.empty()) pj["refParts"] = p.refPartIds;
            pmiArray.push_back(pj);
        }
        j["pmi"] = pmiArray;
    }

    std::ofstream file(jsonPath);
    if (file.is_open()) {
        file << j.dump(4);
        file.close();
        Logger::Info("JSON export successful");
        return true;
    }

    Logger::Error("Failed to write JSON file");
    return false;
}