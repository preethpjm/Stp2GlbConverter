#include "step_entity_extractor.h"
#include <XSControl_WorkSession.hxx>
#include <XSControl_TransferReader.hxx>
#include <Interface_Graph.hxx>
#include <Interface_EntityIterator.hxx>
#include <Interface_InterfaceModel.hxx>
#include <StepShape_ShapeDefinitionRepresentation.hxx>
#include <StepRepr_ProductDefinitionShape.hxx>
#include <StepRepr_PropertyDefinition.hxx>
#include <StepBasic_ProductDefinition.hxx>
#include <StepBasic_ProductDefinitionFormation.hxx>
#include <StepBasic_Product.hxx>
#include <TCollection_HAsciiString.hxx>
#include <Utils/logger.h>
#include <vector>
#include <set>
#include <memory>

// Interface_Graph is a plain value type in OCCT, NOT derived from
// Standard_Transient — it can't use Handle(...), only ordinary C++
// ownership (std::shared_ptr here).
static Handle(XSControl_WorkSession) s_cachedWS;
static std::shared_ptr<Interface_Graph> s_cachedGraph;

static Interface_Graph& GetOrBuildGraph(const Handle(XSControl_WorkSession)& WS) {
    if (s_cachedWS != WS || !s_cachedGraph) {
        s_cachedGraph = std::make_shared<Interface_Graph>(WS->Model());
        s_cachedWS = WS;
    }
    return *s_cachedGraph;
}

static Handle(StepShape_ShapeDefinitionRepresentation) FindOwningSDR(
    Interface_Graph& graph,
    const Handle(Standard_Transient)& startEnt)
{
    std::vector<Handle(Standard_Transient)> frontier{startEnt};
    std::set<Standard_Integer> visited;
    const Handle(Interface_InterfaceModel)& model = graph.Model();
    const int maxDepth = 5;

    for (int depth = 0; depth < maxDepth && !frontier.empty(); ++depth) {
        std::vector<Handle(Standard_Transient)> next;
        for (auto& ent : frontier) {
            Standard_Integer num = model->Number(ent);
            if (num == 0 || visited.count(num)) continue;
            visited.insert(num);

            Handle(StepShape_ShapeDefinitionRepresentation) sdr =
                Handle(StepShape_ShapeDefinitionRepresentation)::DownCast(ent);
            if (!sdr.IsNull()) return sdr;

            Interface_EntityIterator sharings = graph.Sharings(ent);
            for (sharings.Start(); sharings.More(); sharings.Next()) {
                next.push_back(sharings.Value());
            }
        }
        frontier = next;
    }
    return nullptr;
}

static Handle(StepShape_ShapeDefinitionRepresentation) FindOwningSDR(
    const Interface_Graph& graph,
    const Handle(Standard_Transient)& startEnt)
{
    std::vector<Handle(Standard_Transient)> frontier{startEnt};
    std::set<Standard_Integer> visited;
    const Handle(Interface_InterfaceModel)& model = graph.Model();
    const int maxDepth = 5;

    for (int depth = 0; depth < maxDepth && !frontier.empty(); ++depth) {
        std::vector<Handle(Standard_Transient)> next;
        for (auto& ent : frontier) {
            Standard_Integer num = model->Number(ent);
            if (num == 0 || visited.count(num)) continue;
            visited.insert(num);

            Handle(StepShape_ShapeDefinitionRepresentation) sdr =
                Handle(StepShape_ShapeDefinitionRepresentation)::DownCast(ent);
            if (!sdr.IsNull()) return sdr;

            Interface_EntityIterator sharings = graph.Sharings(ent);
            for (sharings.Start(); sharings.More(); sharings.Next()) {
                next.push_back(sharings.Value());
            }
        }
        frontier = next;
    }
    return nullptr;
}

ProductInfo StepEntityExtractor::GetProductInfo(STEPCAFControl_Reader& reader, const TopoDS_Shape& shape) {
    ProductInfo info;

    Handle(XSControl_WorkSession) WS = reader.Reader().WS();
    if (WS.IsNull()) return info;

    Handle(XSControl_TransferReader) TR = WS->TransferReader();
    if (TR.IsNull()) return info;

    Handle(Standard_Transient) ent;
    for (Standard_Integer mode = 1; mode <= 4 && ent.IsNull(); ++mode) {
        ent = TR->EntityFromShapeResult(shape, mode);
    }
    if (ent.IsNull()) {
        Logger::Warning("EntityFromShapeResult found nothing for this shape (tried modes 1-4).");
        return info;
    }

    Interface_Graph& graph = GetOrBuildGraph(WS);
    Handle(StepShape_ShapeDefinitionRepresentation) sdr = FindOwningSDR(graph, ent);

    if (sdr.IsNull()) {
        Logger::Warning("Could not walk up to a ShapeDefinitionRepresentation "
                         "from entity type: " + std::string(ent->DynamicType()->Name()));
        return info;
    }

    Handle(StepRepr_PropertyDefinition) propDef = sdr->Definition().PropertyDefinition();
    if (propDef.IsNull()) {
        Logger::Warning("SDR.Definition() is not a PropertyDefinition — "
                         "likely a ShapeAspect instead, no Product to reach here.");
        return info;
    }

    Handle(StepRepr_ProductDefinitionShape) pds =
        Handle(StepRepr_ProductDefinitionShape)::DownCast(propDef);
    if (pds.IsNull()) {
        Logger::Warning("PropertyDefinition is not a ProductDefinitionShape — type is: " +
                         std::string(propDef->DynamicType()->Name()));
        return info;
    }

    Handle(StepBasic_ProductDefinition) prodDef =
        Handle(StepBasic_ProductDefinition)::DownCast(pds->Definition().ProductDefinition());
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