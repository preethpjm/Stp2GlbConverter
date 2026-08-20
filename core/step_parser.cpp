#include "step_parser.h"
#include "mesh_converter.h"
#include "step_header_parser.h"
#include "step_entity_extractor.h"
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
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Face.hxx>
#include <BRep_Builder.hxx>
#include <map>

static std::string LabelToString(const TDF_Label& label) {
    TCollection_AsciiString entry;
    TDF_Tool::Entry(label, entry);
    return entry.ToCString();
}

std::vector<StepParser::FaceGroup> StepParser::SplitByFaceStyling(
    const TopoDS_Shape& solid,
    const Handle(XCAFDoc_ColorTool)& colorTool,
    const Handle(XCAFDoc_LayerTool)& layerTool) const
{
    std::map<std::string, std::vector<TopoDS_Face>> groups;
    std::map<std::string, ColorInfo> groupColor;
    std::map<std::string, std::string> groupLayer;

    TopExp_Explorer exp(solid, TopAbs_FACE);
    for (; exp.More(); exp.Next()) {
        TopoDS_Face face = TopoDS::Face(exp.Current());

        ColorInfo color;
        if (!colorTool.IsNull()) {
            Quantity_Color col;
            // Color lookup is shape-based in OCCT, not label-based, so this
            // works at face granularity exactly like it does at solid
            // granularity elsewhere in this file.
            if (colorTool->GetColor(face, XCAFDoc_ColorSurf, col) ||
                colorTool->GetColor(face, XCAFDoc_ColorGen, col)) {
                color.hasColor = true;
                color.r = (float)col.Red();
                color.g = (float)col.Green();
                color.b = (float)col.Blue();
            }
        }

        std::string layerName;
        if (!layerTool.IsNull()) {
            TDF_LabelSequence layerLabels;
            layerTool->GetLayers(face, layerLabels);
            if (layerLabels.Length() > 0) {
                Handle(TDataStd_Name) nameAttr;
                if (layerLabels.Value(1).FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
                    layerName = TCollection_AsciiString(nameAttr->Get()).ToCString();
                }
            }
        }

        // Prefer color as the grouping signal (finer-grained, more commonly
        // populated); fall back to layer; if neither exists, everything
        // lands in one bucket — same as the old whole-solid behavior.
        std::string sig;
        if (color.hasColor) {
            sig = "c:" + std::to_string((int)(color.r * 255)) + "-" +
                          std::to_string((int)(color.g * 255)) + "-" +
                          std::to_string((int)(color.b * 255));
        } else if (!layerName.empty()) {
            sig = "l:" + layerName;
        } else {
            sig = "ungrouped";
        }

        groups[sig].push_back(face);
        groupColor[sig] = color;
        groupLayer[sig] = layerName;
    }

    std::vector<FaceGroup> result;
    for (auto& kv : groups) {
        TopoDS_Compound comp;
        BRep_Builder builder;
        builder.MakeCompound(comp);
        for (auto& f : kv.second) builder.Add(comp, f);

        FaceGroup fg;
        fg.shape = comp;
        fg.color = groupColor[kv.first];
        fg.layerTag = groupLayer[kv.first];
        result.push_back(fg);
    }
    return result;
}


bool StepParser::Load(const std::string& stepFile, ModelData& outModel, HierarchyMode mode) {
    Logger::Info("Parsing STEP file: " + stepFile);

    outModel.documentMetadata = StepHeaderParser::Parse(stepFile);

    Handle(XCAFApp_Application) app = XCAFApp_Application::GetApplication();
    Handle(TDocStd_Document) doc;
    app->NewDocument("MDTV-XCAF", doc);

    STEPCAFControl_Reader reader;
    reader.SetNameMode(Standard_True);
    reader.SetColorMode(Standard_True);
    reader.SetLayerMode(Standard_True);
    reader.SetMatMode(Standard_True);
    reader.SetGDTMode(Standard_True);
    reader.SetPropsMode(Standard_True);

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
    Handle(XCAFDoc_LayerTool) layerTool =
        XCAFDoc_DocumentTool::LayerTool(doc->Main());

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
            useHierarchyPath = true;
            wasForced = true;
        } else if (mode == HierarchyMode::ForceFlat && detectedHierarchy) {
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
            child = BuildAssemblyTree(reader, shapeTool, colorTool, materialTool, layerTool,
                                       rootLabel, gp_Trsf(), outModel);
        } else {
            TopoDS_Shape shape = shapeTool->GetShape(rootLabel);
            child = BuildFromGeometrySplit(shape, colorTool, layerTool, outModel);
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
        Logger::Warning("Label flagged as assembly but has 0 components — treating as flat.");
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

void StepParser::ExtractLayers(const Handle(XCAFDoc_LayerTool)& layerTool,
                                const TDF_Label& label, std::vector<std::string>& outLayers) const {
    if (layerTool.IsNull()) return;
    TDF_LabelSequence layerLabels;
    layerTool->GetLayers(label, layerLabels);
    for (Standard_Integer i = 1; i <= layerLabels.Length(); ++i) {
        Handle(TDataStd_Name) nameAttr;
        if (layerLabels.Value(i).FindAttribute(TDataStd_Name::GetID(), nameAttr)) {
            outLayers.push_back(TCollection_AsciiString(nameAttr->Get()).ToCString());
        }
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
    const Handle(XCAFDoc_LayerTool)& layerTool,
    ModelData& model)
{
    Logger::Warning("No assembly structure found — falling back to geometry split "
                     "by disjoint solids, with face-level color/layer splitting "
                     "for any solid that turns out to be internally fused.");

    AssemblyNode container;
    container.id = "geomsplit_root";
    container.name = "Ungrouped Solids (no source hierarchy)";

    int solidIndex = 0;
    TopExp_Explorer solidExp(shape, TopAbs_SOLID);
    for (; solidExp.More(); solidExp.Next(), ++solidIndex) {
        const TopoDS_Shape& solid = solidExp.Current();

        auto faceGroups = SplitByFaceStyling(solid, colorTool, layerTool);

        if (faceGroups.size() <= 1) {
            // No internal styling distinction found — same as the original
            // behavior: treat the whole solid as one part.
            ColorInfo color;
            if (!faceGroups.empty()) {
                color = faceGroups[0].color;
            } else if (!colorTool.IsNull()) {
                Quantity_Color col;
                if (colorTool->GetColor(solid, XCAFDoc_ColorSurf, col)) {
                    color.hasColor = true;
                    color.r = (float)col.Red();
                    color.g = (float)col.Green();
                    color.b = (float)col.Blue();
                }
            }

            std::string colorTag = color.hasColor
                ? ("_c" + std::to_string((int)(color.r * 255)) + "-" +
                          std::to_string((int)(color.g * 255)) + "-" +
                          std::to_string((int)(color.b * 255)))
                : "";

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

            node.partIndex = model.parts.size();
            model.parts.push_back(part);
            model.partIndexCache[cacheKey] = node.partIndex;
            container.children.push_back(node);

        } else {
            // Internal color/layer boundaries found within a fused solid —
            // split into separately selectable regions. NOTE: these sub-parts
            // are open face shells, not closed solids — fine for mesh/GLB
            // selection purposes, but don't expect meaningful volume/mass if
            // you ever compute validation properties on them.
            Logger::Info("Solid " + std::to_string(solidIndex) + " has " +
                         std::to_string(faceGroups.size()) +
                         " internally distinct styled regions — splitting.");

            for (size_t g = 0; g < faceGroups.size(); ++g) {
                const auto& fg = faceGroups[g];

                std::string colorTag = fg.color.hasColor
                    ? ("_c" + std::to_string((int)(fg.color.r * 255)) + "-" +
                              std::to_string((int)(fg.color.g * 255)) + "-" +
                              std::to_string((int)(fg.color.b * 255)))
                    : "";
                std::string layerTag = !fg.layerTag.empty() ? ("_L-" + fg.layerTag) : "";

                AssemblyNode node;
                node.id = "solid_" + std::to_string(solidIndex) + "_region_" + std::to_string(g);
                node.isPart = true;
                node.name = "Part_" + std::to_string(solidIndex) + "_" +
                            std::to_string(g) + colorTag + layerTag;

                std::string cacheKey = "geomsplit_" + std::to_string(solidIndex) +
                                        "_" + std::to_string(g);
                size_t meshIdx = MeshConverter::ConvertAndCache(fg.shape, cacheKey, model);

                PartNode part;
                part.id = node.id;
                part.name = node.name;
                part.meshIndex = meshIdx;
                part.instanceCount = 1;
                part.color = fg.color;
                part.hasGeometry = true;
                if (!fg.layerTag.empty()) part.layers.push_back(fg.layerTag);

                node.partIndex = model.parts.size();
                model.parts.push_back(part);
                model.partIndexCache[cacheKey] = node.partIndex;
                container.children.push_back(node);
            }
        }
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
        part.id = node.id; part.name = node.name; part.meshIndex = meshIdx;
        part.hasGeometry = true;
        node.partIndex = model.parts.size();
        model.parts.push_back(part);
        model.partIndexCache[cacheKey] = node.partIndex;
        container.children.push_back(node);
    }

    return container;
}

AssemblyNode StepParser::BuildAssemblyTree(
    STEPCAFControl_Reader& reader,
    const Handle(XCAFDoc_ShapeTool)& shapeTool,
    const Handle(XCAFDoc_ColorTool)& colorTool,
    const Handle(XCAFDoc_MaterialTool)& materialTool,
    const Handle(XCAFDoc_LayerTool)& layerTool,
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
                reader, shapeTool, colorTool, materialTool, layerTool,
                targetLabel, compTrsf, model
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
            MeshConverter::ConvertAndCache(shape, cacheKey, model);
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
        ExtractLayers(layerTool, label, part.layers);

        ProductInfo prodInfo = StepEntityExtractor::GetProductInfo(reader, shape);
        if (prodInfo.found) {
            part.partNumber = prodInfo.partNumber;
            part.descriptionText = prodInfo.description;
        }

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