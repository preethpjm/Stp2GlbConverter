#include "core/data_model.h"
#include "core/step_parser.h"
#include "export/glb_exporter.h"
#include "export/json_exporter.h"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    std::string inputFile = (argc > 1) ? argv[1] : "assembly.stp";
    std::string glbOut    = (argc > 2) ? argv[2] : "output.glb";
    std::string jsonOut   = (argc > 3) ? argv[3] : "assembly.json";

    std::cout << "=== STEP to GLB Converter ===" << std::endl;
    std::cout << "Input:  " << inputFile << std::endl;
    std::cout << "GLB:    " << glbOut << std::endl;
    std::cout << "JSON:   " << jsonOut << std::endl;

    ModelData model;
    StepParser parser;

    std::cout << "\n[1/3] Parsing STEP file..." << std::endl;
    if (!parser.Load(inputFile, model)) {
        std::cerr << "ERROR: Failed to parse STEP file." << std::endl;
        return 1;
    }
    std::cout << "  Parts:        " << model.parts.size() << std::endl;
    std::cout << "  Unique meshes: " << model.uniqueMeshes.size() << std::endl;

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