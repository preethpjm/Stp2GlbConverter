#pragma once
#include "data_model.h"
#include <string>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_MaterialTool.hxx>
#include <XCAFDoc_DimTolTool.hxx>
#include <TDF_Label.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>

class StepParser {
public:
    bool Load(const std::string& stepFile, ModelData& outModel,
              HierarchyMode mode = HierarchyMode::Auto);

private:
    AssemblyNode BuildAssemblyTree(
        const Handle(XCAFDoc_ShapeTool)& shapeTool,
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const Handle(XCAFDoc_MaterialTool)& materialTool,
        const TDF_Label& label,
        const gp_Trsf& parentTransform,
        ModelData& model
    );

    // Fallback for files with no usable product/assembly structure. Splits
    // by disjoint solid body and tags each with whatever color is directly
    // assigned to it — color is a proxy for material grouping, not material
    // itself.
    AssemblyNode BuildFromGeometrySplit(
        const TopoDS_Shape& shape,
        const Handle(XCAFDoc_ColorTool)& colorTool,
        ModelData& model
    );

    // Hardened: also requires at least one real component, since
    // IsAssembly() can be true on zero-component labels in malformed files.
    bool TreeHasRealStructure(const Handle(XCAFDoc_ShapeTool)& shapeTool,
                               const TDF_Label& label) const;

    void ExtractColor(const Handle(XCAFDoc_ColorTool)& colorTool,
                       const TDF_Label& label, ColorInfo& outColor) const;

    void ExtractMaterial(const Handle(XCAFDoc_MaterialTool)& materialTool,
                          const TDF_Label& label, MaterialInfo& outMaterial) const;

    void ExtractValidationProps(const TDF_Label& label, ValidationProps& outProps) const;

    // File-scoped, best-effort. OCCT DimTol API is the least version-stable
    // part of this file — verify against your OCCT version if it doesn't compile.
    void ExtractPmi(const Handle(XCAFDoc_DimTolTool)& dimTolTool, ModelData& model) const;

    int CountNodes(const AssemblyNode& node) const;
};