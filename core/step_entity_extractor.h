#pragma once
#include <string>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <STEPCAFControl_Reader.hxx>
struct ProductInfo {
    bool found = false;
    std::string partNumber;   // STEP PRODUCT.Id() — the real human part number,
                               // distinct from the descriptive Name already captured
                               // (e.g. "F6137-M83461-1-014", not "O RING SEAL")
    std::string description;  // STEP PRODUCT.Description() — free text, often carries
                               // material/finish/revision-note hints not present
                               // anywhere else (e.g. "JOINT ETHYLENE PROPYLENE
                               // (POUR SKYDROL)" on an O-ring with no structured
                               // material data)
};
// Reverse-maps a resulting TopoDS_Shape back to the STEP entity that produced it,
// then walks the standard AP214/242 chain:
//   ProductDefinitionShape -> ProductDefinition -> ProductDefinitionFormation -> Product
//
// CAVEAT: the shape->entity reverse lookup (EntityFromShapeResult) is the least
// version-stable OCCT API used anywhere in this project — more so than the PMI
// code. Test against F6137-D24907000.stp first, where the expected values are
// already known (see conversation), before trusting this on new files. If it
// returns found=false for everything, that call is the first thing to check
// against your OCCT version's actual XSControl_TransferReader API.
class StepEntityExtractor {
public:
    static ProductInfo GetProductInfo(STEPCAFControl_Reader& reader, const TopoDS_Shape& shape);
    static std::string GetFaceBreadcrumb(STEPCAFControl_Reader& reader, const TopoDS_Face& face);
};