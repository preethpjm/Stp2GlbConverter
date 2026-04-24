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

    // Get this label's location
    TopLoc_Location loc = shapeTool->GetLocation(label);
    node.transform = loc.IsIdentity() ? gp_Trsf() : loc.Transformation();

    if (shapeTool->IsAssembly(label)) {
        // Get only DIRECT children (not recursive - we handle recursion ourselves)
        TDF_LabelSequence components;
        shapeTool->GetComponents(label, components, Standard_False);

        Logger::Info("  Assembly: " + node.name + 
                     " -> " + std::to_string(components.Length()) + " components");

        for (Standard_Integer i = 1; i <= components.Length(); ++i) {
            TDF_Label compLabel = components.Value(i);

            // Get instance transform from the component label
            TopLoc_Location compLoc = shapeTool->GetLocation(compLabel);
            gp_Trsf compTrsf = compLoc.IsIdentity() ? gp_Trsf() : compLoc.Transformation();

            // Get the actual shape this component refers to
            TDF_Label referred;
            if (XCAFDoc_ShapeTool::GetReferredShape(compLabel, referred)) {
                // Recurse into the referred shape
                AssemblyNode child = BuildAssemblyTree(
                    shapeTool, referred, compTrsf, model
                );
                child.transform = compTrsf;

                // Get child name from component label if referred has none
                if (child.name.empty()) {
                    Handle(TDataStd_Name) compName;
                    if (compLabel.FindAttribute(TDataStd_Name::GetID(), compName)) {
                        child.name = TCollection_AsciiString(compName->Get()).ToCString();
                    }
                }

                node.children.push_back(child);
            } else {
                // No referred shape — treat component itself as a part
                AssemblyNode child = BuildAssemblyTree(
                    shapeTool, compLabel, compTrsf, model
                );
                child.transform = compTrsf;
                node.children.push_back(child);
            }
        }
    } else if (shapeTool->IsSimpleShape(label)) {
        // Leaf part — tessellate it
        node.isPart = true;
        TopoDS_Shape shape = shapeTool->GetShape(label);

        if (!shape.IsNull()) {
            size_t meshIdx = MeshConverter::ConvertAndCache(shape, model);
            node.partIndex = model.parts.size();
            model.parts.push_back({node.id, node.name, meshIdx, 1});
            Logger::Info("  Part: " + node.name + 
                        " meshIdx=" + std::to_string(meshIdx));
        } else {
            Logger::Warning("  Null shape for: " + node.name);
        }
    } else if (shapeTool->IsReference(label)) {
        // This label is itself a reference — resolve it
        TDF_Label referred;
        if (XCAFDoc_ShapeTool::GetReferredShape(label, referred)) {
            return BuildAssemblyTree(shapeTool, referred, parentTransform, model);
        }
    }

    return node;
}

int StepParser::CountNodes(const AssemblyNode& node) const {
    int count = 1;
    for (const auto& child : node.children)
        count += CountNodes(child);
    return count;
}