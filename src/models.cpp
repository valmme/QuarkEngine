#include "headers/models.h"
#include <unordered_set>

std::vector<ModelAsset> assets;

void load_models() {
    ModelAsset cube_asset;
    cube_asset.name = "Cube";
    cube_asset.type = CUBE;
    cube_asset.isProcedural = true;
    cube_asset.generator = [](int) { return LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f)); };
    assets.push_back(cube_asset);

    ModelAsset sphere_asset;
    sphere_asset.name = "Sphere";
    sphere_asset.type = SPHERE;
    sphere_asset.isProcedural = true;
    sphere_asset.generator = [](int seg) { return LoadModelFromMesh(GenMeshSphere(1.0f, seg, seg)); };
    assets.push_back(sphere_asset);

    ModelAsset cone_asset;
    cone_asset.name = "Cone";
    cone_asset.type = CONE;
    cone_asset.isProcedural = true;
    cone_asset.generator = [](int seg) { return LoadModelFromMesh(GenMeshCone(1.0f, 1.0f, seg)); };
    assets.push_back(cone_asset);

    ModelAsset cylinder_asset;
    cylinder_asset.name = "Cylinder";
    cylinder_asset.type = CYLINDER;
    cylinder_asset.isProcedural = true;
    cylinder_asset.generator = [](int seg) { return LoadModelFromMesh(GenMeshCylinder(1.0f, 1.0f, seg)); };
    assets.push_back(cylinder_asset);

    ModelAsset hemisphere_asset;
    hemisphere_asset.name = "HemiSphere";
    hemisphere_asset.type = HEMISPHERE;
    hemisphere_asset.isProcedural = true;
    hemisphere_asset.generator = [](int seg) { return LoadModelFromMesh(GenMeshHemiSphere(1.0f, seg, seg)); };
    assets.push_back(hemisphere_asset);

    ModelAsset torus_asset;
    torus_asset.name = "Torus";
    torus_asset.type = TORUS;
    torus_asset.isProcedural = true;
    torus_asset.generator = [](int seg) { return LoadModelFromMesh(GenMeshTorus(1.0f, 1.0f, seg, seg)); };
    assets.push_back(torus_asset);
}

void update_model(Entity* e) {
    if (!e || !e->asset || !e->asset->isProcedural || !e->asset->generator) return;
    if (e->model.meshCount > 0) UnloadModel(e->model);
    if (e->segments < 3) e->segments = 3;
    if (e->segments > 125) e->segments = 125;
    e->model = e->asset->generator(e->segments);
}

void unload_models() {
    std::unordered_set<void*> released_meshes;

    for (auto& asset : assets) {
        if (!asset.isProcedural && asset.loadedModel.meshCount > 0 && asset.loadedModel.meshes) {
            void* mesh_ptr = static_cast<void*>(asset.loadedModel.meshes);
            if (released_meshes.insert(mesh_ptr).second) {
                UnloadModel(asset.loadedModel);
            }
        }
        asset.loadedModel = {0};
    }

    assets.clear();
}
