#include "core/data_model.h"
#include "core/step_parser.h"
#include "export/glb_exporter.h"
#include "export/json_exporter.h"
#include <iostream>
#include <string>

static HierarchyMode ParseMode(const std::string& s) {
    if (s == "hierarchy") return HierarchyMode::ForceHierarchy;
    if (s == "flat")      return HierarchyMode::ForceFlat;
    return HierarchyMode::Auto;
}

static double ParseQuality(const std::string& s) {
    if (s == "fast")    return 0.003;   // coarser mesh, faster
    if (s == "precise") return 0.0003;  // finer mesh, slower
    return 0.001; // "balanced" / default
}

static int ParseThreshold(const std::string& s) {
    try { return std::stoi(s); } catch (...) { return 150; }
}

int main(int argc, char* argv[]) {
    std::string inputFile = (argc > 1) ? argv[1] : "assembly.stp";
    std::string glbOut    = (argc > 2) ? argv[2] : "output.glb";
    std::string jsonOut   = (argc > 3) ? argv[3] : "assembly.json";
    HierarchyMode mode = (argc > 4) ? ParseMode(argv[4]) : HierarchyMode::Auto;
    double quality = (argc > 5) ? ParseQuality(argv[5]) : 0.001;
    int breadcrumbThreshold = (argc > 6) ? ParseThreshold(argv[6]) : 150;

    std::cout << "=== STEP to GLB Converter ===" << std::endl;
    std::cout << "Input:  " << inputFile << std::endl;
    std::cout << "GLB:    " << glbOut << std::endl;
    std::cout << "JSON:   " << jsonOut << std::endl;
    std::cout << "Mode:   " << (argc > 4 ? argv[4] : "auto") << std::endl;
    std::cout << "Quality: " << (argc > 5 ? argv[5] : "balanced") << std::endl;
    std::cout << "Breadcrumb face threshold: " << breadcrumbThreshold << std::endl;

    ModelData model;
    model.meshQualityMultiplier = quality;
    model.breadcrumbSplitFaceThreshold = breadcrumbThreshold;
    StepParser parser;

    std::cout << "\n[1/3] Parsing STEP file..." << std::endl;
    if (!parser.Load(inputFile, model, mode)) {
        std::cerr << "ERROR: Failed to parse STEP file." << std::endl;
        return 1;
    }
    std::cout << "  Parts:        " << model.parts.size() << std::endl;
    std::cout << "  Unique meshes: " << model.uniqueMeshes.size() << std::endl;
    for (const auto& r : model.rootReports) {
        std::cout << "  Root \"" << r.rootName << "\": "
                   << (r.hierarchyAvailable ? "hierarchy" : "geometry-split")
                   << (r.wasForced ? " (forced)" : "") << std::endl;
    }

    std::cout << "\n[2/3] Exporting GLB..." << std::endl;
    if (!GlbExporter::Export(model, glbOut)) {
        std::cerr << "ERROR: GLB export failed." << std::endl;
        return 1;
    }
    std::cout << "  Saved: " << glbOut << std::endl;

    std::cout << "\n[3/3] Exporting JSON BOM..." << std::endl;
    if (!JsonExporter::Export(model, jsonOut)) {
        std::cerr << "ERROR: JSON export failed." << std::endl;
        return 1;
    }
    std::cout << "  Saved: " << jsonOut << std::endl;

    std::cout << "\nDone!" << std::endl;
    return 0;
}