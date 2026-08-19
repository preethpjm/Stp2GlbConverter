#include "step_parser.h"
#include "mesh_converter.h"
#include <STEPCAFControl_Reader.hxx>
#include <XCAFApp_Application.hxx>
#include <XCAFDoc_DocumentTool.hxx>
#include <XCAFDoc_ShapeTool.hxx>
#include <XCAFDoc_Area.hxx>
#include <XCAFDoc_Volume.hxx>
#include <XCAFDoc_Centroid.hxx>
#include <XCAFDoc_Dimension.hxx>
#include <XCAFDoc_GeomTolerance.hxx>
#include <XCAFDimTolObjects_DimensionObject.hxx>
#include <XCAFDimTolObjects_GeomToleranceObject.hxx>
#include <TDataStd_Name.hxx>
#include <TDF_LabelSequence.hxx>
#include <TCollection_AsciiString.hxx>
#include <TDF_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopAbs_ShapeEnum.hxx>
#include <Quantity_Color.hxx>
#include <Utils/logger.h>
#include <sstream>

static std::string LabelToString(const TDF_Label& label) {
    TCollection_AsciiString entry;
    TDF_Tool::Entry(label, entry);
    return entry.ToCString();
}

bool StepParser::Load(const std::string& stepFile, ModelData& outModel, HierarchyMode mode) {
    Logger::Info("Parsing STEP file: " + stepFile);

    Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
    Handle(TDocStd_Document) doc;
    app->NewDocument("MDTV-XCAF", doc);

    STEPCAFControl_Reader reader;
    reader.SetNameMode(Standard_True);
    reader.SetColorMode(Standard_True);
    reader.SetLayerMode(Standard_True);
    reader.SetMatMode(Standard_True);    // materials (MATERIAL_DESIGNATION)
    reader.SetGDTMode(Standard_True);    // PMI / GD&T (semantic)
    reader.SetPropsMode(Standard_True);  // validation properties (area/volume/centroid)

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
    Handle(XCAFDoc_ColorTool) colorTool =
        XCAFDoc_DocumentTool::ColorTool(doc->Main());
    Handle(XCAFDoc_MaterialTool) materialTool =
        XCAFDoc_DocumentTool::MaterialTool(doc->Main());
    Handle(XCAFDoc_DimTolTool) dimTolTool =
        XCAFDoc_DocumentTool::DimTolTool(doc->Main());

    TDF_LabelSequence freeShapes;
    shapeTool->GetFreeShapes(freeShapes);

    if (freeShapes.IsEmpty()) {
        Logger::Error("No shapes found in STEP file");
        return false;
    }

    Logger::Info("Free shapes found: " + std::to_string(freeShapes.Length()));

    outModel.root.name = "Root";
    outModel.root.id   = "root";
    outModel.hierarchyAvailable = true;

    for (Standard_Integer i = 1; i <= freeShapes.Length(); ++i) {
        const TDF_Label& rootLabel = freeShapes.Value(i);

        bool detectedHierarchy = TreeHasRealStructure(shapeTool, rootLabel);
        bool useHierarchyPath = detectedHierarchy;
        bool wasForced = false;

        if (mode == HierarchyMode::ForceHierarchy && !detectedHierarchy) {
            Logger::Warning("ForceHierarchy set, but this root has no real "
                             "structure. Forcing anyway — expect a near-empty "
                             "or single-node tree for this root.");
            useHierarchyPath = true;
            wasForced = true;
        } else if (mode == HierarchyMode::ForceFlat && detectedHierarchy) {
            Logger::Info("ForceFlat set: ignoring real structure found in "
                         "this root, using geometry-split path instead.");
            useHierarchyPath = false;
            wasForced = true;
        }

        std::string rootId = LabelToString(rootLabel);
        std::string rootName;
        Handle(TDataStd_Name) nameAttr;
        if (rootLabel.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
            rootName = TCollection_AsciiString(nameAttr->Get()).ToCString();
        }

        Logger::Info("Root shape " + std::to_string(i) + "/" +
                     std::to_string(freeShapes.Length()) + " (\"" +
                     (rootName.empty() ? rootId : rootName) + "\"): " +
                     (useHierarchyPath ? "HIERARCHY path" : "GEOMETRY-SPLIT path") +
                     (wasForced ? " [forced]" : " [auto-detected]"));

        AssemblyNode child;
        if (useHierarchyPath) {
            child = BuildAssemblyTree(shapeTool, colorTool, materialTool,
                                       rootLabel, gp_Trsf(), outModel);
        } else {
            TopoDS_Shape shape = shapeTool->GetShape(rootLabel);
            child = BuildFromGeometrySplit(shape, colorTool, outModel);
        }
        outModel.root.children.push_back(child);

        RootShapeInfo report;
        report.rootId = rootId;
        report.rootName = rootName.empty() ? rootId : rootName;
        report.hierarchyAvailable = useHierarchyPath;
        report.wasForced = wasForced;
        outModel.rootReports.push_back(report);

        if (!useHierarchyPath) outModel.hierarchyAvailable = false;
    }

    ExtractPmi(dimTolTool, outModel);

    Logger::Info("Parsing complete.");
    Logger::Info("  Parts:         " + std::to_string(outModel.parts.size()));
    Logger::Info("  Unique meshes: " + std::to_string(outModel.uniqueMeshes.size()));
    for (const auto& r : outModel.rootReports) {
        Logger::Info("  Root \"" + r.rootName + "\": " +
                     (r.hierarchyAvailable ? "hierarchy" : "geometry-split") +
                     (r.wasForced ? " (forced)" : ""));
    }
    if (!outModel.pmi.empty()) {
        Logger::Info("  PMI entries:   " + std::to_string(outModel.pmi.size()));
    }
    return true;
}

bool StepParser::TreeHasRealStructure(const Handle(XCAFDoc_ShapeTool)& shapeTool,
                                       const TDF_Label& label) const {
    if (shapeTool->IsAssembly(label)) {
        TDF_LabelSequence components;
        shapeTool->GetComponents(label, components, Standard_False);
        if (components.Length() > 0) return true;

        Logger::Warning("Label flagged as assembly but has 0 components — "
                         "treating as flat for routing purposes.");
        return false;
    }

    if (shapeTool->IsComponent(label)) return true;

    TDF_Label referred;
    if (XCAFDoc_ShapeTool::GetReferredShape(label, referred)) {
        return TreeHasRealStructure(shapeTool, referred);
    }

    return false;
}

void StepParser::ExtractColor(const Handle(XCAFDoc_ColorTool)& colorTool,
                               const TDF_Label& label, ColorInfo& outColor) const {
    if (colorTool.IsNull()) return;
    Quantity_Color col;
    if (colorTool->GetColor(label, XCAFDoc_ColorSurf, col) ||
        colorTool->GetColor(label, XCAFDoc_ColorGen, col) ||
        colorTool->GetColor(label, XCAFDoc_ColorCurv, col)) {
        outColor.hasColor = true;
        outColor.r = (float)col.Red();
        outColor.g = (float)col.Green();
        outColor.b = (float)col.Blue();
    }
}

void StepParser::ExtractMaterial(const Handle(XCAFDoc_MaterialTool)& materialTool,
                                  const TDF_Label& label, MaterialInfo& outMaterial) const {
    if (materialTool.IsNull()) return;

    Handle(TCollection_HAsciiString) name, desc, densName, densValType;
    Standard_Real density = 0.0;

    // Verify this overload against your OCCT version if it doesn't compile —
    // XCAFDoc_MaterialTool::GetMaterial has shifted slightly across releases.
    if (materialTool->GetMaterial(label, name, desc, density, densName, densValType)) {
        if (!name.IsNull() && name->Length() > 0) {
            outMaterial.hasMaterial = true;
            outMaterial.name = name->ToCString();
            if (!desc.IsNull())     outMaterial.description     = desc->ToCString();
            if (!densName.IsNull()) outMaterial.densityUnitName = densName->ToCString();
            outMaterial.density = density;
        }
    }
}

void StepParser::ExtractValidationProps(const TDF_Label& label, ValidationProps& outProps) const {
    Handle(XCAFDoc_Area) areaAttr;
    if (label.FindAttribute(XCAFDoc_Area::GetID(), areaAttr)) {
        outProps.hasArea = true;
        outProps.area = areaAttr->Get();
    }
    Handle(XCAFDoc_Volume) volAttr;
    if (label.FindAttribute(XCAFDoc_Volume::GetID(), volAttr)) {
        outProps.hasVolume = true;
        outProps.volume = volAttr->Get();
    }
    Handle(XCAFDoc_Centroid) centAttr;
    if (label.FindAttribute(XCAFDoc_Centroid::GetID(), centAttr)) {
        outProps.hasCentroid = true;
        gp_Pnt p = centAttr->Get();
        outProps.centroid[0] = p.X();
        outProps.centroid[1] = p.Y();
        outProps.centroid[2] = p.Z();
    }
}

void StepParser::ExtractPmi(const Handle(XCAFDoc_DimTolTool)& dimTolTool, ModelData& model) const {
    if (dimTolTool.IsNull()) return;

    TDF_LabelSequence dimTolLabels;
    dimTolTool->GetDimTolLabels(dimTolLabels);

    if (dimTolLabels.Length() == 0) return;

    Logger::Info("PMI/GD&T entities found: " + std::to_string(dimTolLabels.Length()));

    for (Standard_Integer i = 1; i <= dimTolLabels.Length(); ++i) {
        const TDF_Label& dtLabel = dimTolLabels.Value(i);
        PmiEntry entry;

        Handle(TDataStd_Name) nameAttr;
        if (dtLabel.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
            entry.text = TCollection_AsciiString(nameAttr->Get()).ToCString();
        }

        Handle(XCAFDoc_Dimension) dimAttr;
        Handle(XCAFDoc_GeomTolerance) tolAttr;

        if (dtLabel.FindAttribute(XCAFDoc_Dimension::GetID(), dimAttr)) {
            entry.kind = "Dimension";
        } else if (dtLabel.FindAttribute(XCAFDoc_GeomTolerance::GetID(), tolAttr)) {
            entry.kind = "GeometricTolerance";
        } else {
            entry.kind = "Datum";
        }

        model.pmi.push_back(entry);
    }
}

AssemblyNode StepParser::BuildFromGeometrySplit(
    const TopoDS_Shape& shape,
    const Handle(XCAFDoc_ColorTool)& colorTool,
    ModelData& model)
{
    Logger::Warning("No assembly structure found in source STEP file — "
                     "falling back to geometry split by disjoint solids "
                     "+ outer-surface color grouping.");

    AssemblyNode container;
    container.id = "geomsplit_root";
    container.name = "Ungrouped Solids (no source hierarchy)";

    int solidIndex = 0;
    TopExp_Explorer exp(shape, TopAbs_SOLID);
    for (; exp.More(); exp.Next(), ++solidIndex) {
        const TopoDS_Shape& solid = exp.Current();

        ColorInfo color;
        if (!colorTool.IsNull()) {
            Quantity_Color col;
            if (colorTool->GetColor(solid, XCAFDoc_ColorSurf, col) ||
                colorTool->GetColor(solid, XCAFDoc_ColorGen, col)) {
                color.hasColor = true;
                color.r = (float)col.Red();
                color.g = (float)col.Green();
                color.b = (float)col.Blue();
            }
        }

        std::string colorTag;
        if (color.hasColor) {
            colorTag = "_c" + std::to_string((int)(color.r * 255)) + "-" +
                              std::to_string((int)(color.g * 255)) + "-" +
                              std::to_string((int)(color.b * 255));
        }

        AssemblyNode node;
        node.id = "solid_" + std::to_string(solidIndex);
        node.isPart = true;
        node.name = "Part_" + std::to_string(solidIndex) + colorTag;

        std::string cacheKey = "geomsplit_" + std::to_string(solidIndex);
        size_t meshIdx = MeshConverter::ConvertAndCache(solid, cacheKey, model);

        PartNode part;
        part.id = node.id;
        part.name = node.name;
        part.meshIndex = meshIdx;
        part.instanceCount = 1;
        part.color = color;
        part.hasGeometry = true;
        // No material lookup here — a flattened compound has no XCAF
        // product structure to hang a material attribute off. Color is the
        // only usable per-part signal in this path.

        node.partIndex = model.parts.size();
        model.parts.push_back(part);
        model.partIndexCache[cacheKey] = node.partIndex;
        container.children.push_back(node);
    }

    if (solidIndex == 0) {
        Logger::Warning("No individual SOLIDs found either — treating entire shape as one part.");
        AssemblyNode node;
        node.id = "solid_0";
        node.isPart = true;
        node.name = "Part_0";

        std::string cacheKey = "geomsplit_0";
        size_t meshIdx = MeshConverter::ConvertAndCache(shape, cacheKey, model);

        PartNode part;
        part.id = node.id;
        part.name = node.name;
        part.meshIndex = meshIdx;
        part.hasGeometry = true;

        node.partIndex = model.parts.size();
        model.parts.push_back(part);
        model.partIndexCache[cacheKey] = node.partIndex;
        container.children.push_back(node);
    }

    return container;
}

AssemblyNode StepParser::BuildAssemblyTree(
    const Handle(XCAFDoc_ShapeTool)& shapeTool,
    const Handle(XCAFDoc_ColorTool)& colorTool,
    const Handle(XCAFDoc_MaterialTool)& materialTool,
    const TDF_Label& label,
    const gp_Trsf& parentTransform,
    ModelData& model)
{
    AssemblyNode node;
    node.id = LabelToString(label);

    Handle(TDataStd_Name) nameAttr;
    if (label.FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
        node.name = TCollection_AsciiString(nameAttr->Get()).ToCString();
    }
    if (node.name.empty()) node.name = "Part_" + node.id;

    TopLoc_Location loc = shapeTool->GetLocation(label);
    node.transform = loc.IsIdentity() ? gp_Trsf() : loc.Transformation();

    if (shapeTool->IsAssembly(label)) {
        TDF_LabelSequence components;
        shapeTool->GetComponents(label, components, Standard_False);

        for (Standard_Integer i = 1; i <= components.Length(); ++i) {
            TDF_Label compLabel = components.Value(i);

            TopLoc_Location compLoc = shapeTool->GetLocation(compLabel);
            gp_Trsf compTrsf = compLoc.IsIdentity() ? gp_Trsf() : compLoc.Transformation();

            TDF_Label referred;
            TDF_Label targetLabel = compLabel;
            if (XCAFDoc_ShapeTool::GetReferredShape(compLabel, referred)) {
                targetLabel = referred;
            }

            AssemblyNode child = BuildAssemblyTree(
                shapeTool, colorTool, materialTool, targetLabel, compTrsf, model
            );
            child.transform = compTrsf;

            if (child.name.empty() || child.name.substr(0, 5) == "Part_") {
                Handle(TDataStd_Name) compName;
                if (compLabel.FindAttribute(TDataStd_Name::GetID(), compName)) {
                    child.name = TCollection_AsciiString(compName->Get()).ToCString();
                }
            }

            if (child.isPart && child.partIndex < model.parts.size()) {
                PartNode& partRef = model.parts[child.partIndex];
                if (!partRef.color.hasColor) {
                    ExtractColor(colorTool, compLabel, partRef.color);
                }
            }

            node.children.push_back(child);
        }

    } else {
        node.isPart = true;
        TopoDS_Shape shape = shapeTool->GetShape(label);
        std::string cacheKey = LabelToString(label);

        if (shape.IsNull()) {
            // Expected and common — BOM-only items (consumables, reference
            // placeholders like the blank-named PRODUCT_DEFINITIONs seen in
            // the Safran seat file) legitimately have no geometry. Still
            // record a real PartNode so downstream consumers don't fall
            // back to a garbage index-0 lookup.
            Logger::Info("No geometry for BOM-only item: " + node.name);

            PartNode part;
            part.id = node.id;
            part.name = node.name;
            part.hasGeometry = false;

            node.partIndex = model.parts.size();
            model.parts.push_back(part);
            return node;
        }

        auto existing = model.partIndexCache.find(cacheKey);
        if (existing != model.partIndexCache.end()) {
            size_t idx = existing->second;
            model.parts[idx].instanceCount++;
            node.partIndex = idx;
            MeshConverter::ConvertAndCache(shape, cacheKey, model); // cache-hit no-op
            return node;
        }

        size_t meshIdx = MeshConverter::ConvertAndCache(shape, cacheKey, model);

        PartNode part;
        part.id = node.id;
        part.name = node.name;
        part.meshIndex = meshIdx;
        part.instanceCount = 1;
        part.hasGeometry = true;

        ExtractColor(colorTool, label, part.color);
        ExtractMaterial(materialTool, label, part.material);
        ExtractValidationProps(label, part.validation);

        size_t newIdx = model.parts.size();
        model.parts.push_back(part);
        model.partIndexCache[cacheKey] = newIdx;
        node.partIndex = newIdx;
    }

    return node;
}

int StepParser::CountNodes(const AssemblyNode& node) const {
    int count = 1;
    for (const auto& child : node.children)
        count += CountNodes(child);
    return count;
}