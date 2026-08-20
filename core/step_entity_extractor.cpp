#include "step_entity_extractor.h"
#include <XSControl_WorkSession.hxx>
#include <XSControl_TransferReader.hxx>
#include <StepRepr_ProductDefinitionShape.hxx>
#include <StepBasic_ProductDefinition.hxx>
#include <StepBasic_ProductDefinitionFormation.hxx>
#include <StepBasic_Product.hxx>
#include <TCollection_HAsciiString.hxx>

ProductInfo StepEntityExtractor::GetProductInfo(STEPCAFControl_Reader& reader, const TopoDS_Shape& shape) {
    ProductInfo info;

    Handle(XSControl_WorkSession) WS = reader.Reader().WS();
    if (WS.IsNull()) return info;

    Handle(XSControl_TransferReader) TR = WS->TransferReader();
    if (TR.IsNull()) return info;

    Handle(Standard_Transient) ent = TR->EntityFromShapeResult(shape, -1);
    if (ent.IsNull()) return info;

    // EntityFromShapeResult can return either a ProductDefinitionShape or, for
    // some representation paths, a ProductDefinition directly — try both rather
    // than assuming one.
    Handle(StepBasic_ProductDefinition) prodDef;

    Handle(StepRepr_ProductDefinitionShape) pds = Handle(StepRepr_ProductDefinitionShape)::DownCast(ent);
    if (!pds.IsNull()) {
        prodDef = Handle(StepBasic_ProductDefinition)::DownCast(pds->Definition().ProductDefinition());
    } else {
        prodDef = Handle(StepBasic_ProductDefinition)::DownCast(ent);
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