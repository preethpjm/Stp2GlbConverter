#pragma once
#include "data_model.h"
#include <string>
#include <vector>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_ColorTool.hxx>
#include <XCAFDoc_MaterialTool.hxx>
#include <XCAFDoc_DimTolTool.hxx>
#include <XCAFDoc_LayerTool.hxx>
#include <STEPCAFControl_Reader.hxx>
#include <TDF_Label.hxx>
#include <TopoDS_Shape.hxx>
#include <gp_Trsf.hxx>

class StepParser {
public:
    bool Load(const std::string& stepFile, ModelData& outModel,
              HierarchyMode mode = HierarchyMode::Auto);

private:
    AssemblyNode BuildAssemblyTree(
        STEPCAFControl_Reader& reader,
        const Handle(XCAFDoc_ShapeTool)& shapeTool,
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const Handle(XCAFDoc_MaterialTool)& materialTool,
        const Handle(XCAFDoc_LayerTool)& layerTool,
        const TDF_Label& label,
        const gp_Trsf& parentTransform,
        ModelData& model
    );

    AssemblyNode SplitLeafByBreadcrumb(
        STEPCAFControl_Reader& reader,
        const TopoDS_Shape& shape,
        const std::string& baseId,
        const std::string& baseName,
        ModelData& model
    );

    AssemblyNode BuildFromGeometrySplit(
        const TopoDS_Shape& shape,
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const Handle(XCAFDoc_LayerTool)& layerTool,   // NEW
        ModelData& model
    );

    bool TreeHasRealStructure(const Handle(XCAFDoc_ShapeTool)& shapeTool,
                               const TDF_Label& label) const;

    void ExtractColor(const Handle(XCAFDoc_ColorTool)& colorTool,
                       const TDF_Label& label, ColorInfo& outColor) const;

    void ExtractMaterial(const Handle(XCAFDoc_MaterialTool)& materialTool,
                          const TDF_Label& label, MaterialInfo& outMaterial) const;

    void ExtractValidationProps(const TDF_Label& label, ValidationProps& outProps) const;

    void ExtractLayers(const Handle(XCAFDoc_LayerTool)& layerTool,
                        const TDF_Label& label, std::vector<std::string>& outLayers) const;

    void ExtractPmi(const Handle(XCAFDoc_DimTolTool)& dimTolTool, ModelData& model) const;

    int CountNodes(const AssemblyNode& node) const;


    struct FaceGroup {
        TopoDS_Shape shape;   // compound of faces sharing this color/layer signature
        ColorInfo color;
        std::string layerTag;
    };

    // Groups a single solid's faces by their individually-assigned color/layer.
    // Returns one group if the solid has no internal styling distinction
    // (i.e. current behavior — treat the whole solid as one part). Returns
    // multiple groups when the solid was Boolean-fused from originally
    // distinct, differently-styled parts.
    std::vector<FaceGroup> SplitByFaceStyling(
        const TopoDS_Shape& solid,
        const Handle(XCAFDoc_ColorTool)& colorTool,
        const Handle(XCAFDoc_LayerTool)& layerTool
    ) const;
};

