#pragma once
#include "data_model.h"
#include <TopoDS_Shape.hxx>
#include <BRepBndLib.hxx>
#include <Bnd_Box.hxx>

class MeshConverter {
public:
    static size_t ConvertAndCache(
        const TopoDS_Shape& shape, 
        const std::string& partId,
        ModelData& model
    );

private:
    static Mesh ExtractMeshData(const TopoDS_Shape& shape);
};