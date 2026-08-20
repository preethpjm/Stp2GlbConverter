#include "step_entity_extractor.h"
#include <XSControl_WorkSession.hxx>
#include <XSControl_TransferReader.hxx>
#include <StepRepr_ProductDefinitionShape.hxx>
#include <StepBasic_ProductDefinition.hxx>
#include <StepBasic_ProductDefinitionFormation.hxx>
#include <StepBasic_Product.hxx>
#include <TCollection_HAsciiString.hxx>
#include <Utils/logger.h>
#include <Standard_Type.hxx>

ProductInfo StepEntityExtractor::GetProductInfo(STEPCAFControl_Reader& reader, const TopoDS_Shape& shape) {
    ProductInfo info;

    Handle(XSControl_WorkSession) WS = reader.Reader().WS();
    if (WS.IsNull()) return info;

    Handle(XSControl_TransferReader) TR = WS->TransferReader();
    if (TR.IsNull()) return info;

    // Mode is 1-4 in OCCT's API (different binder-map search strategies) —
    // -1 is not valid and silently returns null every time, which is the
    // likely cause of description/partNumber never coming through. Try
    // each mode in order rather than hardcoding one we can't verify here.
    Handle(Standard_Transient) ent;
    for (Standard_Integer mode = 1; mode <= 4 && ent.IsNull(); ++mode) {
        ent = TR->EntityFromShapeResult(shape, mode);
    }
    if (ent.IsNull()) {
        Logger::Warning("EntityFromShapeResult found nothing for this shape "
                         "(tried modes 1-4) — no partNumber/description available.");
        return info;
    }

    Handle(StepBasic_ProductDefinition) prodDef;

    Handle(StepRepr_ProductDefinitionShape) pds = Handle(StepRepr_ProductDefinitionShape)::DownCast(ent);
    if (!pds.IsNull()) {
        prodDef = Handle(StepBasic_ProductDefinition)::DownCast(pds->Definition().ProductDefinition());
    } else {
        prodDef = Handle(StepBasic_ProductDefinition)::DownCast(ent);
    }
    if (prodDef.IsNull()) {
        Logger::Warning("Resolved entity is neither ProductDefinitionShape nor "
                         "ProductDefinition — type is: " + std::string(ent->DynamicType()->Name()));
        return info;
    }

    Handle(StepBasic_ProductDefinitionFormation) formation = prodDef->Formation();
    if (formation.IsNull()) return info;

    Handle(StepBasic_Product) product = formation->OfProduct();
    if (product.IsNull()) return info;

    info.found = true;
    if (!product->Id().IsNull())          info.partNumber  = product->Id()->ToCString();
    if (!product->Description().IsNull()) info.description = product->Description()->ToCString();

    return info;
}