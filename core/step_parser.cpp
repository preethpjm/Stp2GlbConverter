#include "step_parser.h"
#include "mesh_converter.h"
#include <STEPCAFControl_Reader.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_LabelSequence.hxx>
#include <TCollection_AsciiString.hxx>
#include <TDF_Tool.hxx>
#include <Utils/logger.h>
#include <sstream>

// Helper to get a unique string ID from a TDF_Label
static std::string LabelToString(const TDF_Label& label) {
    TCollection_AsciiString entry;
    TDF_Tool::Entry(label, entry);
    return entry.ToCString();
}

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
        Logger::Error("Failed to transfer STEP data");
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

    Logger::Info("Free shapes found: " + std::to_string(freeShapes.Length()));

    outModel.root.name = "Root";
    outModel.root.id   = "root";

    for (Standard_Integer i = 1; i <= freeShapes.Length(); ++i) {
        AssemblyNode child = BuildAssemblyTree(
            shapeTool,
            freeShapes.Value(i),
            gp_Trsf(),
            outModel
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
    node.id = LabelToString(label);

    // Get name
    Handle(TDataStd_Name) nameAttr;
    if (label.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
        node.name = TCollection_AsciiString(nameAttr->Get()).ToCString();
    }
    if (node.name.empty()) node.name = "Part_" + node.id;

    // Get transform
    TopLoc_Location loc = shapeTool->GetLocation(label);
    node.transform = loc.IsIdentity() ? gp_Trsf() : loc.Transformation();

    if (shapeTool->IsAssembly(label)) {
        // --- ASSEMBLY NODE ---
        TDF_LabelSequence components;
        shapeTool->GetComponents(label, components, Standard_False);

        for (Standard_Integer i = 1; i <= components.Length(); ++i) {
            TDF_Label compLabel = components.Value(i);

            // Get instance placement transform
            TopLoc_Location compLoc = shapeTool->GetLocation(compLabel);
            gp_Trsf compTrsf = compLoc.IsIdentity() ? gp_Trsf() : compLoc.Transformation();

            // Resolve reference to actual shape
            TDF_Label referred;
            TDF_Label targetLabel = compLabel;

            if (XCAFDoc_ShapeTool::GetReferredShape(compLabel, referred)) {
                targetLabel = referred;
            }

            // Recurse
            AssemblyNode child = BuildAssemblyTree(
                shapeTool, targetLabel, compTrsf, model
            );

            // Always use the INSTANCE transform, not the prototype's
            child.transform = compTrsf;

            // Use component label name if child has no name
            if (child.name.empty() || child.name.substr(0, 5) == "Part_") {
                Handle(TDataStd_Name) compName;
                if (compLabel.FindAttribute(TDataStd_Name::GetID(), compName)) {
                    child.name = TCollection_AsciiString(compName->Get()).ToCString();
                }
            }

            node.children.push_back(child);
        }

    } else {
        // --- LEAF PART ---
        node.isPart = true;
        TopoDS_Shape shape = shapeTool->GetShape(label);

        if (shape.IsNull()) {
            Logger::Warning("Null shape: " + node.name);
            return node;
        }

        // Use label entry as unique part ID for caching
        // This preserves instancing — same label = same mesh
        std::string cacheKey = LabelToString(label);

        size_t meshIdx = MeshConverter::ConvertAndCache(shape, cacheKey, model);
        node.partIndex = model.parts.size();
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