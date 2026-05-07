#include "mesh_converter.h"
#include <BRepMesh_IncrementalMesh.hxx>
#include <BRep_Tool.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <Poly_Triangulation.hxx>
#include <gp_Vec.hxx>
#include <gp_Pnt.hxx>
#include <functional>
#include <cmath>
#include <climits>
#include <Utils/logger.h>

size_t MeshConverter::ConvertAndCache(
    const TopoDS_Shape& shape,
    const std::string& partId,
    ModelData& model)
{
    // Use partId as the cache key — this is unique per part
    // NOT shape hash which was causing merging
    auto it = model.meshCache.find(partId);
    if (it != model.meshCache.end()) {
        // This is a real instance of an existing mesh
        if (!model.parts.empty())
            model.parts.back().instanceCount++;
        return it->second;
    }

    // Adaptive deflection based on shape size
    Bnd_Box bbox;
    BRepBndLib::Add(shape, bbox);
    double xmin, ymin, zmin, xmax, ymax, zmax;
    double deflection = 0.5; // default

    if (!bbox.IsVoid()) {
        bbox.Get(xmin, ymin, zmin, xmax, ymax, zmax);
        double diagLen = std::sqrt(
            (xmax-xmin)*(xmax-xmin) +
            (ymax-ymin)*(ymax-ymin) +
            (zmax-zmin)*(zmax-zmin)
        );
        // Scale deflection to model size — 0.1% of diagonal
        deflection = std::max(0.01, diagLen * 0.001);
    }

    BRepMesh_IncrementalMesh mesher(shape, deflection, Standard_False, 0.5);
    mesher.Perform();

    Mesh mesh = ExtractMeshData(shape);

    if (mesh.vertices.empty()) {
        Logger::Warning("Empty mesh for part: " + partId);
    }

    size_t idx = model.uniqueMeshes.size();
    model.uniqueMeshes.push_back(std::move(mesh));
    model.meshCache[partId] = idx;
    return idx;
}

Mesh MeshConverter::ExtractMeshData(const TopoDS_Shape& shape) {
    Mesh m;

    TopExp_Explorer exp;
    for (exp.Init(shape, TopAbs_FACE); exp.More(); exp.Next()) {
        const TopoDS_Face& face = TopoDS::Face(exp.Current());
        TopLoc_Location loc;
        Handle(Poly_Triangulation) tri = BRep_Tool::Triangulation(face, loc);
        if (tri.IsNull()) continue;

        bool reversed = (face.Orientation() == TopAbs_REVERSED);
        gp_Trsf trsf  = loc.IsIdentity() ? gp_Trsf() : loc.Transformation();

        Standard_Integer nNodes = tri->NbNodes();
        Standard_Integer nTris  = tri->NbTriangles();
        size_t base = m.vertices.size() / 3;

        // Extract vertices
        for (Standard_Integer i = 1; i <= nNodes; ++i) {
            gp_Pnt p = tri->Node(i).Transformed(trsf);
            m.vertices.push_back((float)p.X());
            m.vertices.push_back((float)p.Y());
            m.vertices.push_back((float)p.Z());
            // Initialize normals to zero — accumulated below
            m.normals.push_back(0.0f);
            m.normals.push_back(0.0f);
            m.normals.push_back(0.0f);
        }

        // Extract triangles
        for (Standard_Integer i = 1; i <= nTris; ++i) {
            Poly_Triangle t = tri->Triangle(i);
            Standard_Integer n1, n2, n3;
            t.Get(n1, n2, n3);

            if (reversed) std::swap(n2, n3);

            unsigned int i1 = (unsigned int)(base + n1 - 1);
            unsigned int i2 = (unsigned int)(base + n2 - 1);
            unsigned int i3 = (unsigned int)(base + n3 - 1);

            m.indices.push_back(i1);
            m.indices.push_back(i2);
            m.indices.push_back(i3);

            // Compute face normal
            gp_Pnt p1(m.vertices[i1*3],   m.vertices[i1*3+1], m.vertices[i1*3+2]);
            gp_Pnt p2(m.vertices[i2*3],   m.vertices[i2*3+1], m.vertices[i2*3+2]);
            gp_Pnt p3(m.vertices[i3*3],   m.vertices[i3*3+1], m.vertices[i3*3+2]);

            gp_Vec v1(p1, p2);
            gp_Vec v2(p1, p3);
            gp_Vec normal = v1.Crossed(v2);

            double len = normal.Magnitude();
            if (len > 1e-10) normal /= len;

            // Accumulate into vertices
            for (unsigned int vi : {i1, i2, i3}) {
                m.normals[vi*3]   += (float)normal.X();
                m.normals[vi*3+1] += (float)normal.Y();
                m.normals[vi*3+2] += (float)normal.Z();
            }
        }
    }

    // Normalize all normals
    for (size_t i = 0; i + 2 < m.normals.size(); i += 3) {
        float nx = m.normals[i];
        float ny = m.normals[i+1];
        float nz = m.normals[i+2];
        float len = std::sqrt(nx*nx + ny*ny + nz*nz);
        if (len > 1e-6f) {
            m.normals[i]   /= len;
            m.normals[i+1] /= len;
            m.normals[i+2] /= len;
        } else {
            // Default normal if degenerate
            m.normals[i]   = 0.0f;
            m.normals[i+1] = 1.0f;
            m.normals[i+2] = 0.0f;
        }
    }

    m.vertexCount = m.vertices.size() / 3;
    return m;
}