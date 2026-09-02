#ifndef __EDITABLE_MESH_H__
#define __EDITABLE_MESH_H__

#include "QuarkCore/QuarkCore.hpp"
using namespace qc;
#include <vector>

struct EditableVertex {
    Vec3 position;
    float u = 0.0f;
    float v = 0.0f;
};

struct EditableTriangle {
    int a;
    int b;
    int c;
};

struct EditableMesh {
    std::vector<EditableVertex> vertices;
    std::vector<EditableTriangle> triangles;
};

void rebuild_mesh_from_editable(Model& model, EditableMesh& editable);

#endif // __EDITABLE_MESH_H__
