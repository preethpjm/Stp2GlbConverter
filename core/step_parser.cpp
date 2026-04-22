#include "step_parser.h"
#include "mesh_converter.h"
#include <STEPCAFControl_Reader.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_LabelSequence.hxx>
#include <TCollection_AsciiString.hxx>
#include <BRep_Builder.hxx>
#include <Utils/logger.h>

bool StepParser::Load(const std::string& stepFile, ModelData& outModel) {
    Logger::Info("Parsing STEP file: " + stepFile);

    Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
    Handle(TDocStd_Document) doc;
    app->NewDocument("MDTV-XCAF", doc);

    STEPCAFControl_Reader reader;
    reader.SetNameMode(Standard_True);
    reader.SetColorMode(Standard_True);
    reader.SetLayerMode(Standard_True);

    IFSelect_ReturnStatus stat = reader.ReadFile(stepFile.c_str());
    if (stat != IFSelect_RetDone) {
        Logger::Error("Failed to read STEP file: " + stepFile);
        return false;
    }

    if (!reader.Transfer(doc)) {
        Logger::Error("Failed to transfer STEP data to document");
        return false;
    }

    Handle(XCAFDoc_ShapeTool) shapeTool =
        XCAFDoc_DocumentTool::ShapeTool(doc->Main());

    TDF_LabelSequence freeShapes;
    shapeTool->GetFreeShapes(freeShapes);

    if (freeShapes.IsEmpty()) {
        Logger::Error("No shapes found in STEP file");
        return false;
    }

    // Build root assembly node
    outModel.root.name = "Root";
    outModel.root.id   = "root";

    for (Standard_Integer i = 1; i <= freeShapes.Length(); ++i) {
        AssemblyNode child = BuildAssemblyTree(
            shapeTool, freeShapes.Value(i), gp_Trsf(), outModel
        );
        outModel.root.children.push_back(child);
    }

    Logger::Info("Parsing complete.");
    Logger::Info("  Parts:         " + std::to_string(outModel.parts.size()));
    Logger::Info("  Unique meshes: " + std::to_string(outModel.uniqueMeshes.size()));
    return true;
}

AssemblyNode StepParser::BuildAssemblyTree(
    const Handle(XCAFDoc_ShapeTool)& shapeTool,
    const TDF_Label& label,
    const gp_Trsf& parentTransform,
    ModelData& model)
{
    AssemblyNode node;

    // Get name
    Handle(TDataStd_Name) nameAttr;
    if (label.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
        node.name = TCollection_AsciiString(nameAttr->Get()).ToCString();
    }
    node.id = std::to_string(label.Tag());

    // Get this label's own location and combine with parent
    TopLoc_Location loc = shapeTool->GetLocation(label);
    gp_Trsf localTrsf   = loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.IsIdentity() ? gp_Trsf() : loc.Transformation();
    node.transform = localTrsf;

    if (shapeTool->IsAssembly(label)) {
        // Recurse into components
        TDF_LabelSequence components;
        shapeTool->GetComponents(label, components, Standard_False);

        for (Standard_Integer i = 1; i <= components.Length(); ++i) {
            TDF_Label compLabel = components.Value(i);

            // Get the instance transform from the component label
            TopLoc_Location compLoc = shapeTool->GetLocation(compLabel);
            gp_Trsf compTrsf = compLoc.IsIdentity() ? gp_Trsf() : compLoc.Transformation();

            // Get the referred (prototype) shape label
            TDF_Label referred;
            if (XCAFDoc_ShapeTool::GetReferredShape(compLabel, referred)) {
                AssemblyNode child = BuildAssemblyTree(
                    shapeTool, referred, compTrsf, model
                );
                // Override transform with the instance placement
                child.transform = compTrsf;
                node.children.push_back(child);
            }
        }
    } else {
        // Leaf part — tessellate
        node.isPart = true;
        TopoDS_Shape shape = shapeTool->GetShape(label);

        size_t meshIdx   = MeshConverter::ConvertAndCache(shape, model);
        node.partIndex   = model.parts.size();
        model.parts.push_back({node.id, node.name, meshIdx, 1});
    }

    return node;
}

int StepParser::CountNodes(const AssemblyNode& node) const {
    int count = 1;
    for (const auto& child : node.children)
        count += CountNodes(child);
    return count;
}