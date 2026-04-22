#pragma once
#include "data_model.h"
#include <TopoDS_Shape.hxx>

class MeshConverter {
public:
    static size_t ConvertAndCache(const TopoDS_Shape& shape, ModelData& model);

private:
    static Mesh ExtractMeshData(const TopoDS_Shape& shape);
};