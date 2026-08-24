#include "step_entity_extractor.h"
#include <XSControl_WorkSession.hxx>
#include <XSControl_TransferReader.hxx>
#include <Interface_InterfaceModel.hxx>
#include <StepShape_ShapeDefinitionRepresentation.hxx>
#include <StepRepr_Representation.hxx>
#include <StepRepr_RepresentationItem.hxx>
#include <StepRepr_HArray1OfRepresentationItem.hxx>
#include <StepRepr_ProductDefinitionShape.hxx>
#include <StepRepr_PropertyDefinition.hxx>
#include <StepRepr_ShapeRepresentationRelationship.hxx>
#include <StepShape_AdvancedFace.hxx>
#include <StepBasic_ProductDefinition.hxx>
#include <StepBasic_ProductDefinitionFormation.hxx>
#include <StepBasic_Product.hxx>
#include <TCollection_HAsciiString.hxx>
#include <TopoDS_Face.hxx>
#include <Utils/logger.h>
#include <map>
#include <memory>

// Which mode actually resolves an entity is a property of how this file's
// transfer was built — almost always the same mode succeeds every time
// within one file. Remembering it and trying it first turns "try up to 4
// lookups per shape" into "try 1" for the common case, instead of
// re-guessing on every single new part.
static Standard_Integer s_lastSuccessfulMode = 1;

static Handle(Standard_Transient) FindEntityFromShape(const Handle(XSControl_TransferReader)& TR, const TopoDS_Shape& shape) {
    Handle(Standard_Transient) ent = TR->EntityFromShapeResult(shape, s_lastSuccessfulMode);
    if (!ent.IsNull()) return ent;
    for (Standard_Integer mode = 1; mode <= 4; ++mode) {
        if (mode == s_lastSuccessfulMode) continue;
        ent = TR->EntityFromShapeResult(shape, mode);
        if (!ent.IsNull()) { s_lastSuccessfulMode = mode; return ent; }
    }
    return ent;
}

std::string StepEntityExtractor::GetFaceBreadcrumb(STEPCAFControl_Reader& reader, const TopoDS_Face& face) {
    Handle(XSControl_WorkSession) WS = reader.Reader().WS();
    if (WS.IsNull()) return "";
    Handle(XSControl_TransferReader) TR = WS->TransferReader();
    if (TR.IsNull()) return "";

    Handle(Standard_Transient) ent = FindEntityFromShape(TR, face);
    if (ent.IsNull()) return "";
    Handle(StepShape_AdvancedFace) af = Handle(StepShape_AdvancedFace)::DownCast(ent);
    if (af.IsNull() || af->Name().IsNull()) return "";

    std::string name = af->Name()->ToCString();
    if (name.empty() || name == " ") return "";
    return name;
}

// --- SDR lookup map (used by GetProductInfo) ------------------------------

static Handle(XSControl_WorkSession) s_cachedWS;
static std::shared_ptr<std::map<Standard_Integer, Handle(StepShape_ShapeDefinitionRepresentation)>> s_cachedMap;

static void CollectItemsRecursive(
    const Handle(Interface_InterfaceModel)& model,
    const Handle(StepRepr_Representation)& rep,
    const Handle(StepShape_ShapeDefinitionRepresentation)& owningSDR,
    std::map<Standard_Integer, Handle(StepShape_ShapeDefinitionRepresentation)>& outMap,
    int depth = 0)
{
    if (rep.IsNull() || depth > 4) return;

    Standard_Integer repNum = model->Number(rep);
    if (repNum != 0) outMap[repNum] = owningSDR;

    Handle(StepRepr_HArray1OfRepresentationItem) items = rep->Items();
    if (items.IsNull()) return;

    for (Standard_Integer i = items->Lower(); i <= items->Upper(); ++i) {
        Handle(StepRepr_RepresentationItem) item = items->Value(i);
        if (item.IsNull()) continue;

        Standard_Integer itemNum = model->Number(item);
        if (itemNum != 0) outMap[itemNum] = owningSDR;

        Handle(StepRepr_Representation) nestedRep = Handle(StepRepr_Representation)::DownCast(item);
        if (!nestedRep.IsNull()) {
            CollectItemsRecursive(model, nestedRep, owningSDR, outMap, depth + 1);
        }
    }
}


static std::map<Standard_Integer, Handle(StepShape_ShapeDefinitionRepresentation)>&
GetOrBuildSDRMap(const Handle(XSControl_WorkSession)& WS) {
    if (s_cachedWS != WS || !s_cachedMap) {
        s_cachedMap = std::make_shared<std::map<Standard_Integer, Handle(StepShape_ShapeDefinitionRepresentation)>>();
        s_cachedWS = WS;

        Handle(Interface_InterfaceModel) model = WS->Model();
        Standard_Integer nb = model->NbEntities();

        int sdrCount = 0;
        std::vector<Handle(StepRepr_ShapeRepresentationRelationship)> relationships;

        // Single full-model pass collecting BOTH SDRs and relationships,
        // instead of 1 pass for SDRs + 3 more full passes for
        // relationships (4 total full-file scans). On a large file,
        // NbEntities() can be in the tens of millions — dominated by raw
        // geometry (B-spline curves, points) that will never match either
        // type. Scanning once instead of four times is the real fix for
        // this being slow specifically on large files, since this whole
        // setup step runs once per file but its cost scales with total
        // entity count, not part count.
        for (Standard_Integer i = 1; i <= nb; ++i) {
            Handle(Standard_Transient) ent = model->Value(i);

            Handle(StepShape_ShapeDefinitionRepresentation) sdr =
                Handle(StepShape_ShapeDefinitionRepresentation)::DownCast(ent);
            if (!sdr.IsNull()) {
                ++sdrCount;
                Handle(StepRepr_Representation) usedRep = sdr->UsedRepresentation();
                CollectItemsRecursive(model, usedRep, sdr, *s_cachedMap);
                continue;
            }

            Handle(StepRepr_ShapeRepresentationRelationship) rel =
                Handle(StepRepr_ShapeRepresentationRelationship)::DownCast(ent);
            if (!rel.IsNull()) {
                relationships.push_back(rel);
            }
        }

        // Propagation passes now only iterate the (much smaller) list of
        // actual relationship entities found above — hundreds, typically,
        // not tens of millions — instead of re-scanning the whole model.
        const int maxPasses = 3;
        for (int pass = 0; pass < maxPasses; ++pass) {
            bool changed = false;
            for (auto& rel : relationships) {
                Handle(StepRepr_Representation) rep1 = rel->Rep1();
                Handle(StepRepr_Representation) rep2 = rel->Rep2();
                if (rep1.IsNull() || rep2.IsNull()) continue;

                Standard_Integer num1 = model->Number(rep1);
                Standard_Integer num2 = model->Number(rep2);

                auto it1 = s_cachedMap->find(num1);
                auto it2 = s_cachedMap->find(num2);

                if (it1 != s_cachedMap->end() && it2 == s_cachedMap->end()) {
                    CollectItemsRecursive(model, rep2, it1->second, *s_cachedMap);
                    changed = true;
                } else if (it2 != s_cachedMap->end() && it1 == s_cachedMap->end()) {
                    CollectItemsRecursive(model, rep1, it2->second, *s_cachedMap);
                    changed = true;
                }
            }
            if (!changed) break;
        }

        Logger::Info("Built SDR lookup map: " + std::to_string(sdrCount) +
                     " ShapeDefinitionRepresentations, " + std::to_string(relationships.size()) +
                     " relationships, " + std::to_string(s_cachedMap->size()) + " mapped geometry entities.");
    }
    return *s_cachedMap;
}

// --- GetProductInfo --------------------------------------------------------

ProductInfo StepEntityExtractor::GetProductInfo(STEPCAFControl_Reader& reader, const TopoDS_Shape& shape) {
    ProductInfo info;

    Handle(XSControl_WorkSession) WS = reader.Reader().WS();
    if (WS.IsNull()) return info;

    Handle(XSControl_TransferReader) TR = WS->TransferReader();
    if (TR.IsNull()) return info;

    Handle(Standard_Transient) ent = FindEntityFromShape(TR, shape);
    if (ent.IsNull()) {
        Logger::Warning("EntityFromShapeResult found nothing for this shape (tried modes 1-4).");
        return info;
    }

    auto& sdrMap = GetOrBuildSDRMap(WS);
    Standard_Integer entNum = WS->Model()->Number(ent);

    auto it = sdrMap.find(entNum);
    Handle(StepShape_ShapeDefinitionRepresentation) sdr;
    Handle(StepBasic_ProductDefinition) directProdDef;

    if (it == sdrMap.end()) {
        // Document-only / geometry-less entries (consumables, harness
        // reference items) often resolve directly to a ProductDefinition
        // rather than through the SDR map at all.
        directProdDef = Handle(StepBasic_ProductDefinition)::DownCast(ent);
        if (directProdDef.IsNull()) {
            Logger::Warning("Entity type " + std::string(ent->DynamicType()->Name()) +
                             " (#" + std::to_string(entNum) + ") not found in SDR map "
                             "and is not directly a ProductDefinition either.");
            return info;
        }
        Logger::Info("Entity #" + std::to_string(entNum) +
                     " resolved directly as ProductDefinition (no SDR needed).");
    } else {
        sdr = it->second;
    }

    Handle(StepBasic_ProductDefinition) prodDef = directProdDef;

    if (prodDef.IsNull() && !sdr.IsNull()) {
        Handle(StepRepr_PropertyDefinition) propDef = sdr->Definition().PropertyDefinition();
        if (!propDef.IsNull()) {
            Handle(StepRepr_ProductDefinitionShape) pds =
                Handle(StepRepr_ProductDefinitionShape)::DownCast(propDef);
            if (!pds.IsNull()) {
                prodDef = Handle(StepBasic_ProductDefinition)::DownCast(pds->Definition().ProductDefinition());
            } else {
                Logger::Warning("PropertyDefinition is not a ProductDefinitionShape — type is: " +
                                 std::string(propDef->DynamicType()->Name()));
            }
        } else {
            Logger::Warning("SDR.Definition() is not a PropertyDefinition.");
        }
    }
    if (prodDef.IsNull()) return info;

    Handle(StepBasic_ProductDefinitionFormation) formation = prodDef->Formation();
    if (formation.IsNull()) return info;

    Handle(StepBasic_Product) product = formation->OfProduct();
    if (product.IsNull()) return info;

    info.found = true;
    if (!product->Id().IsNull())          info.partNumber  = product->Id()->ToCString();
    if (!product->Description().IsNull()) info.description = product->Description()->ToCString();

    return info;
}