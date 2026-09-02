#include "editable_mesh.h"
#include "QuarkCore/QuarkCore.hpp"
#include <memory>

using namespace qc;

void rebuild_mesh_from_editable(Model& model, EditableMesh& editable) {
    if (model.meshCount <= 0) {
        model.meshCount = 1;
        model.meshes = new Mesh[1];
        model.meshes[0] = {};
    }

    Mesh& mesh = model.meshes[0];

    if (mesh.vaoId > 0 || mesh.vertices || mesh.normals || mesh.texcoords || mesh.indices) {
        UnloadMesh(mesh);
        mesh = {};
    }

    mesh.vertexCount   = (int)editable.vertices.size();
    mesh.triangleCount = (int)editable.triangles.size();

    if (mesh.vertexCount == 0 || mesh.triangleCount == 0)
        return;

    mesh.vertices  = new float[mesh.vertexCount * 3];
    mesh.normals   = new float[mesh.vertexCount * 3];
    mesh.texcoords = new float[mesh.vertexCount * 2];
    mesh.indices   = new unsigned short[mesh.triangleCount * 3];

    for (int i = 0; i < mesh.vertexCount; i++) {
        Vec3 p = editable.vertices[i].position;
        mesh.vertices[i * 3 + 0] = p.x;
        mesh.vertices[i * 3 + 1] = p.y;
        mesh.vertices[i * 3 + 2] = p.z;

        mesh.normals[i * 3 + 0] = 0;
        mesh.normals[i * 3 + 1] = 1;
        mesh.normals[i * 3 + 2] = 0;

        mesh.texcoords[i * 2 + 0] = editable.vertices[i].u;
        mesh.texcoords[i * 2 + 1] = editable.vertices[i].v;
    }

    for (int i = 0; i < mesh.triangleCount; i++) {
        EditableTriangle& tri = editable.triangles[i];
        mesh.indices[i * 3 + 0] = (unsigned short)tri.a;
        mesh.indices[i * 3 + 1] = (unsigned short)tri.b;
        mesh.indices[i * 3 + 2] = (unsigned short)tri.c;
    }

    for (int i = 0; i < mesh.vertexCount * 3; i++)
        mesh.normals[i] = 0.0f;

    for (int i = 0; i < mesh.triangleCount; i++) {
        int ia = mesh.indices[i * 3 + 0];
        int ib = mesh.indices[i * 3 + 1];
        int ic = mesh.indices[i * 3 + 2];

        Vec3 a = { mesh.vertices[ia*3], mesh.vertices[ia*3+1], mesh.vertices[ia*3+2] };
        Vec3 b = { mesh.vertices[ib*3], mesh.vertices[ib*3+1], mesh.vertices[ib*3+2] };
        Vec3 c = { mesh.vertices[ic*3], mesh.vertices[ic*3+1], mesh.vertices[ic*3+2] };

        Vec3 n =
                (b - a)
                .cross(c - a)
                .normalized();

        for (int v : {ia, ib, ic}) {
            mesh.normals[v*3+0] += n.x;
            mesh.normals[v*3+1] += n.y;
            mesh.normals[v*3+2] += n.z;
        }
    }

    for (int i = 0; i < mesh.vertexCount; i++) {
        Vec3 n = Vec3(
            mesh.normals[i * 3 + 0],
            mesh.normals[i * 3 + 1],
            mesh.normals[i * 3 + 2]
        ).normalized();

        mesh.normals[i*3+0] = n.x;
        mesh.normals[i*3+1] = n.y;
        mesh.normals[i*3+2] = n.z;
    }

    UploadMesh(&mesh, true);

    if (model.materialCount <= 0) {
        model.materialCount = 1;
        model.materials = new Material[1];
        model.materials[0] = LoadMaterialDefault();
    }

    if (!model.meshMaterial)
        model.meshMaterial = new int[1];

    model.meshMaterial[0] = 0;
}
