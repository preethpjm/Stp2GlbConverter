#pragma once
#include "data_model.h"
#include <string>
#include <XCAFDoc_ShapeTool.hxx>
#include <TDF_Label.hxx>
#include <gp_Trsf.hxx>

class StepParser {
public:
    bool Load(const std::string& stepFile, ModelData& outModel);

private:
    AssemblyNode BuildAssemblyTree(
        const Handle(XCAFDoc_ShapeTool)& shapeTool,
        const TDF_Label& label,
        const gp_Trsf& parentTransform,
        ModelData& model
    );
    int CountNodes(const AssemblyNode& node) const;
};