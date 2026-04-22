#pragma once
#include "geometry_processor.h"
#include <Utils/logger.h>

void GeometryProcessor::OptimizeMesh(Mesh& mesh) {
    Logger::Info("Optimizing mesh: " + std::to_string(mesh.vertices.size() / 3) + " vertices");
    
    if (mesh.vertices.empty()) {
        Logger::Warning("Empty mesh detected during optimization");
        return;
    }
    
    mesh.vertexCount = mesh.vertices.size() / 3;
    Logger::Info("Mesh optimization complete. Vertices: " + 
                 std::to_string(mesh.vertexCount) + 
                 " Triangles: " + std::to_string(mesh.indices.size() / 3));
}