#include "models.h"
#include "scene.h"
#include "text_mesh.h"
#include "editor/editor_utils.h"
#include <algorithm>
#include <cctype>
#include <cstring>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#if defined(_MSC_VER)
#include <excpt.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <csignal>
#include <csetjmp>
#include <cstdlib>
#endif

std::vector<ModelAsset> assets;

static std::string lowercase_copy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

#if defined(__unix__) || defined(__APPLE__)
static sigjmp_buf g_model_load_env;
static volatile sig_atomic_t g_model_load_guard_active = 0;
static struct sigaction g_prev_sigsegv = {};
static struct sigaction g_prev_sigbus = {};
static struct sigaction g_prev_sigabrt = {};

static void model_load_signal_handler(int signal_code) {
    if (g_model_load_guard_active) siglongjmp(g_model_load_env, signal_code);
    std::_Exit(128 + signal_code);
}

static void install_model_load_signal_guards() {
    struct sigaction action = {};
    sigemptyset(&action.sa_mask);
    action.sa_handler = model_load_signal_handler;
    action.sa_flags = 0;

    sigaction(SIGSEGV, &action, &g_prev_sigsegv);
#ifdef SIGBUS
    sigaction(SIGBUS, &action, &g_prev_sigbus);
#endif
    sigaction(SIGABRT, &action, &g_prev_sigabrt);
    g_model_load_guard_active = 1;
}

static void restore_model_load_signal_guards() {
    g_model_load_guard_active = 0;
    sigaction(SIGSEGV, &g_prev_sigsegv, nullptr);
#ifdef SIGBUS
    sigaction(SIGBUS, &g_prev_sigbus, nullptr);
#endif
    sigaction(SIGABRT, &g_prev_sigabrt, nullptr);
}
#endif

static Vec3 safe_normalize(Vec3 value) {
    const float length = sqrtf(value.x*value.x + value.y*value.y + value.z*value.z);
    if (length <= 0.000001f) return { 0.0f, 1.0f, 0.0f };
    return { value.x / length, value.y / length, value.z / length };
}

bool get_mesh_triangle_vertex_indices(const Mesh& mesh, int triangle_index, int out_indices[3]) {
    if (!out_indices || triangle_index < 0 || triangle_index >= mesh.triangleCount) return false;

    if (mesh.indices) {
        const int base = triangle_index * 3;
        out_indices[0] = static_cast<int>(mesh.indices[base + 0]);
        out_indices[1] = static_cast<int>(mesh.indices[base + 1]);
        out_indices[2] = static_cast<int>(mesh.indices[base + 2]);
    } else {
        const int base = triangle_index * 3;
        out_indices[0] = base + 0;
        out_indices[1] = base + 1;
        out_indices[2] = base + 2;
    }

    for (int i = 0; i < 3; i++) {
        if (out_indices[i] < 0 || out_indices[i] >= mesh.vertexCount) return false;
    }

    return true;
}

void rebuild_mesh_normals(Mesh& mesh) {
    if (!mesh.vertices || !mesh.normals || mesh.vertexCount <= 0) return;

    for (int i = 0; i < mesh.vertexCount * 3; i++) {
        mesh.normals[i] = 0.0f;
    }

    for (int triangle_index = 0; triangle_index < mesh.triangleCount; triangle_index++) {
        int indices[3] = {};
        if (!get_mesh_triangle_vertex_indices(mesh, triangle_index, indices)) continue;

        const Vec3 a = {
            mesh.vertices[indices[0] * 3 + 0],
            mesh.vertices[indices[0] * 3 + 1],
            mesh.vertices[indices[0] * 3 + 2]
        };
        const Vec3 b = {
            mesh.vertices[indices[1] * 3 + 0],
            mesh.vertices[indices[1] * 3 + 1],
            mesh.vertices[indices[1] * 3 + 2]
        };
        const Vec3 c = {
            mesh.vertices[indices[2] * 3 + 0],
            mesh.vertices[indices[2] * 3 + 1],
            mesh.vertices[indices[2] * 3 + 2]
        };

        const Vec3 ab = b - a;
        const Vec3 ac = c - a;
        const Vec3 normal = safe_normalize(ab.cross(ac));

        for (int i = 0; i < 3; i++) {
            mesh.normals[indices[i] * 3 + 0] += normal.x;
            mesh.normals[indices[i] * 3 + 1] += normal.y;
            mesh.normals[indices[i] * 3 + 2] += normal.z;
        }
    }

    for (int vertex_index = 0; vertex_index < mesh.vertexCount; vertex_index++) {
        const Vec3 accumulated = {
            mesh.normals[vertex_index * 3 + 0],
            mesh.normals[vertex_index * 3 + 1],
            mesh.normals[vertex_index * 3 + 2]
        };
        const Vec3 normalized = safe_normalize(accumulated);
        mesh.normals[vertex_index * 3 + 0] = normalized.x;
        mesh.normals[vertex_index * 3 + 1] = normalized.y;
        mesh.normals[vertex_index * 3 + 2] = normalized.z;
    }
}

void clear_mesh_overrides(Entity& entity) {
    MeshComponent* mesh = entity.get_mesh_component();
    if (!mesh) return;
    mesh->mesh_vertex_overrides.clear();
    mesh->mesh_triangles_detached = false;
}

bool entity_has_mesh_overrides(const Entity& entity) {
    const MeshComponent* mesh = entity.get_mesh_component();
    return mesh && !mesh->mesh_vertex_overrides.empty();
}

void capture_mesh_overrides_from_model(Entity& entity) {
    MeshComponent* mesh_component = entity.get_mesh_component();
    if (!mesh_component) return;

    const bool triangles_detached = mesh_component->mesh_triangles_detached;
    mesh_component->mesh_vertex_overrides.clear();
    mesh_component->mesh_triangles_detached = triangles_detached;
    if (mesh_component->model.meshCount <= 0 || !mesh_component->model.meshes) return;

    mesh_component->mesh_vertex_overrides.reserve(mesh_component->model.meshCount);
    for (int mesh_index = 0; mesh_index < mesh_component->model.meshCount; mesh_index++) {
        const Mesh& mesh = mesh_component->model.meshes[mesh_index];
        if (!mesh.vertices || mesh.vertexCount <= 0) {
            mesh_component->mesh_vertex_overrides.emplace_back();
            continue;
        }

        const float* begin = mesh.vertices;
        const float* end = mesh.vertices + mesh.vertexCount * 3;
        mesh_component->mesh_vertex_overrides.emplace_back(begin, end);
    }
}

bool apply_mesh_overrides(Entity& entity) {
    MeshComponent* mesh_component = entity.get_mesh_component();
    if (!mesh_component) return false;
    if (!entity_has_mesh_overrides(entity)) return false;
    if (mesh_component->model.meshCount <= 0 || !mesh_component->model.meshes) return false;

    if (mesh_component->mesh_triangles_detached) {
        detach_mesh_triangles(entity);
    }

    bool applied_any = false;

    for (int mesh_index = 0; mesh_index < mesh_component->model.meshCount; mesh_index++) {
        if (mesh_index >= static_cast<int>(mesh_component->mesh_vertex_overrides.size())) break;

        Mesh& mesh = mesh_component->model.meshes[mesh_index];
        std::vector<float>& override_vertices = mesh_component->mesh_vertex_overrides[mesh_index];
        if (!mesh.vertices || mesh.vertexCount <= 0) continue;
        if (override_vertices.size() != static_cast<size_t>(mesh.vertexCount * 3)) continue;

        memcpy(mesh.vertices, override_vertices.data(), override_vertices.size() * sizeof(float));
        UpdateMeshBuffer(mesh, 0, mesh.vertices, mesh.vertexCount * 3 * sizeof(float), 0);

        if (mesh.normals) {
            rebuild_mesh_normals(mesh);
            UpdateMeshBuffer(mesh, 2, mesh.normals, mesh.vertexCount * 3 * sizeof(float), 0);
        }

        applied_any = true;
    }

    return applied_any;
}

static bool detach_single_mesh_triangles(Mesh& mesh) {
    if (!mesh.vertices || mesh.vertexCount <= 0 || mesh.triangleCount <= 0) return false;
    if (!mesh.indices) return true;

    const int detached_vertex_count = mesh.triangleCount * 3;
    std::vector<float> vertices(detached_vertex_count * 3);
    std::vector<float> texcoords;
    std::vector<float> normals;
    std::vector<unsigned char> colors;

    if (mesh.texcoords) texcoords.resize(detached_vertex_count * 2);
    if (mesh.normals) normals.resize(detached_vertex_count * 3);
    if (mesh.colors) colors.resize(detached_vertex_count * 4);

    for (int triangle_index = 0; triangle_index < mesh.triangleCount; triangle_index++) {
        int source_indices[3] = {};
        if (!get_mesh_triangle_vertex_indices(mesh, triangle_index, source_indices)) return false;

        for (int corner = 0; corner < 3; corner++) {
            const int src = source_indices[corner];
            const int dst = triangle_index * 3 + corner;

            vertices[dst * 3 + 0] = mesh.vertices[src * 3 + 0];
            vertices[dst * 3 + 1] = mesh.vertices[src * 3 + 1];
            vertices[dst * 3 + 2] = mesh.vertices[src * 3 + 2];

            if (mesh.texcoords) {
                texcoords[dst * 2 + 0] = mesh.texcoords[src * 2 + 0];
                texcoords[dst * 2 + 1] = mesh.texcoords[src * 2 + 1];
            }

            if (mesh.normals) {
                normals[dst * 3 + 0] = mesh.normals[src * 3 + 0];
                normals[dst * 3 + 1] = mesh.normals[src * 3 + 1];
                normals[dst * 3 + 2] = mesh.normals[src * 3 + 2];
            }

            if (mesh.colors) {
                colors[dst * 4 + 0] = mesh.colors[src * 4 + 0];
                colors[dst * 4 + 1] = mesh.colors[src * 4 + 1];
                colors[dst * 4 + 2] = mesh.colors[src * 4 + 2];
                colors[dst * 4 + 3] = mesh.colors[src * 4 + 3];
            }
        }
    }

    Mesh detached = mesh;
    detached.vertexCount = detached_vertex_count;
    detached.vertices = new float[vertices.size()];
    detached.texcoords = mesh.texcoords ? new float[texcoords.size()] : nullptr;
    detached.normals = mesh.normals ? new float[normals.size()] : nullptr;
    detached.colors = mesh.colors ? new unsigned char[colors.size()] : nullptr;
    detached.indices = nullptr;
    detached.animVertices = nullptr;
    detached.animNormals = nullptr;
    detached.boneWeights = nullptr;
    detached.boneCount = 0;
    detached.vaoId = 0;
    detached.vboId = 0;

    if (!detached.vertices ||
        (mesh.texcoords && !detached.texcoords) ||
        (mesh.normals && !detached.normals) ||
        (mesh.colors && !detached.colors)) {
        delete[] detached.vertices;
        delete[] detached.texcoords;
        delete[] detached.normals;
        delete[] detached.colors;
        return false;
    }

    memcpy(detached.vertices, vertices.data(), vertices.size() * sizeof(float));
    if (detached.texcoords) memcpy(detached.texcoords, texcoords.data(), texcoords.size() * sizeof(float));
    if (detached.normals) memcpy(detached.normals, normals.data(), normals.size() * sizeof(float));
    if (detached.colors) memcpy(detached.colors, colors.data(), colors.size() * sizeof(unsigned char));

    rebuild_mesh_normals(detached);
    UploadMesh(&detached, false);
    UnloadMesh(mesh);
    mesh = detached;
    return true;
}

bool detach_mesh_triangles(Entity& entity) {
    MeshComponent* mesh_component = entity.get_mesh_component();
    if (!mesh_component || mesh_component->model.meshCount <= 0 || !mesh_component->model.meshes) return false;

    bool detached_any = false;
    for (int mesh_index = 0; mesh_index < mesh_component->model.meshCount; mesh_index++) {
        Mesh& mesh = mesh_component->model.meshes[mesh_index];
        if (!mesh.indices) continue;
        if (!detach_single_mesh_triangles(mesh)) continue;
        detached_any = true;
    }

    if (detached_any) {
        mesh_component->mesh_triangles_detached = true;
    } else if (!mesh_component->mesh_triangles_detached) {
        mesh_component->mesh_triangles_detached = true;
    }

    return detached_any;
}

static std::vector<std::filesystem::path> collect_model_paths(const std::filesystem::path& resource_dir) {
    namespace fs = std::filesystem;

    std::vector<fs::path> result;
    std::error_code ec;
    fs::recursive_directory_iterator it(resource_dir, fs::directory_options::skip_permission_denied, ec);
    if (ec) return result;

    for (const auto& entry : it) {
        if (!entry.is_regular_file(ec) || ec) {
            ec.clear();
            continue;
        }

        if (is_model_file(entry.path())) {
            result.push_back(entry.path());
        }
    }

    return result;
}

bool is_model_file(const std::filesystem::path& p) {
    std::string ext = lowercase_copy(p.extension().string());

    return ext == ".obj" || ext == ".glb" || ext == ".gltf" || ext == ".iqm";
}

bool entity_owns_model(const Entity& entity) {
    const MeshComponent* mesh = entity.get_mesh_component();
    return mesh && mesh->owns_model_instance;
}

bool ensure_model_asset_loaded(ModelAsset& asset) {
    if (asset.is_procedural) {
        return static_cast<bool>(asset.generator);
    }

    if (asset.loaded_model.meshCount > 0 && asset.loaded_model.meshes) {
        return true;
    }

    if (asset.filepath.empty()) {
        return false;
    }

    asset.loaded_model = LoadModel(asset.filepath.c_str());
    return asset.loaded_model.meshCount > 0 && asset.loaded_model.meshes != nullptr;
}

bool load_model_instance(const ModelAsset& asset, Model& model) {
    if (asset.is_procedural) {
        if (!asset.generator) {
            model = {};
            return false;
        }

        model = asset.generator(32);
        return model.meshCount > 0 && model.meshes != nullptr;
    }

    if (asset.filepath.empty()) {
        model = {};
        return false;
    }

    model = LoadModel(asset.filepath.c_str());
    return model.meshCount > 0 && model.meshes != nullptr;
}

void load_models() {
    ModelAsset cube_asset;
    cube_asset.name = "Cube";
    cube_asset.type = CUBE;
    cube_asset.is_procedural = true;
    cube_asset.generator = [](int) { return LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f)); };
    assets.push_back(cube_asset);

    ModelAsset sphere_asset;
    sphere_asset.name = "Sphere";
    sphere_asset.type = SPHERE;
    sphere_asset.is_procedural = true;
    sphere_asset.generator = [](int seg) { return LoadModelFromMesh(GenMeshSphere(1.0f, seg, seg)); };
    assets.push_back(sphere_asset);

    ModelAsset cone_asset;
    cone_asset.name = "Cone";
    cone_asset.type = CONE;
    cone_asset.is_procedural = true;
    cone_asset.generator = [](int seg) { return LoadModelFromMesh(GenMeshCone(1.0f, 1.0f, seg)); };
    assets.push_back(cone_asset);

    ModelAsset cylinder_asset;
    cylinder_asset.name = "Cylinder";
    cylinder_asset.type = CYLINDER;
    cylinder_asset.is_procedural = true;
    cylinder_asset.generator = [](int seg) { return LoadModelFromMesh(GenMeshCylinder(1.0f, 1.0f, seg)); };
    assets.push_back(cylinder_asset);

    ModelAsset hemisphere_asset;
    hemisphere_asset.name = "HemiSphere";
    hemisphere_asset.type = HEMISPHERE;
    hemisphere_asset.is_procedural = true;
    hemisphere_asset.generator = [](int seg) { return LoadModelFromMesh(GenMeshHemiSphere(1.0f, seg, seg)); };
    assets.push_back(hemisphere_asset);

    ModelAsset torus_asset;
    torus_asset.name = "Torus";
    torus_asset.type = TORUS;
    torus_asset.is_procedural = true;
    torus_asset.generator = [](int seg) { return LoadModelFromMesh(GenMeshTorus(1.0f, 1.0f, seg, seg)); };
    assets.push_back(torus_asset);

    ModelAsset text_asset;
    text_asset.name = "3D Text";
    text_asset.type = CUBE;
    text_asset.is_procedural = true;
    text_asset.generator = [](int) {
        std::string fp = get_default_font_path();
        return generate_text_mesh("Text", 0.3f, 0.15f, 0.2f, fp);
    };
    
    assets.push_back(text_asset);
}

void update_model(Entity* e)
{
    if (!e) return;

    MeshComponent* mesh = e->get_mesh_component();

    if (!mesh) return;

    if (mesh->model.meshCount > 0 && entity_owns_model(*e)) UnloadModel(mesh->model);

    if (auto* tc = e->get_components()->get_component_of_type<Text3DComponent>().get()) {
        std::string fp = tc->font_path.empty() ? get_default_font_path() : tc->font_path;
        mesh->model = generate_text_mesh(tc->text, tc->size, tc->thickness, tc->letter_spacing, fp);
        mesh->owns_model_instance = true;
        return;
    }

    if (mesh->is_editable_mesh)
    {
        mesh->model = {};

        rebuild_mesh_from_editable(
            mesh->model,
            mesh->editable_mesh
        );

        mesh->owns_model_instance = true;
        apply_negative_scale_winding(e);

        return;
    }

    if (!mesh->asset || !mesh->asset->is_procedural || !mesh->asset->generator)
        return;
    

    int max_seg = 125;

    if (mesh->type == SPHERE || mesh->type == HEMISPHERE)
        max_seg = 100;
    

    if (mesh->segments < 3)
        mesh->segments = 3;

    if (mesh->segments > max_seg)
        mesh->segments = max_seg;

    mesh->model = mesh->asset->generator(mesh->segments);
    mesh->owns_model_instance = true;

    apply_negative_scale_winding(e);
}

void unload_models() {
    std::unordered_set<void*> released_meshes;

    for (auto& asset : assets) {
        if (!asset.is_procedural && asset.loaded_model.meshCount > 0 && asset.loaded_model.meshes) {
            void* mesh_ptr = static_cast<void*>(asset.loaded_model.meshes);
            if (released_meshes.insert(mesh_ptr).second) {
                UnloadModel(asset.loaded_model);
            }
        }
        asset.loaded_model;
    }

    assets.clear();
}

void load_external_models(std::string project_path) {
    namespace fs = std::filesystem;

    fs::path resource_dir = fs::path(project_path) / "resources";
    if (!fs::exists(resource_dir)) fs::create_directories(resource_dir);

    for (const auto& path : collect_model_paths(resource_dir)) {
        const std::string asset_name = fs::relative(path, resource_dir).generic_string();
        bool already_loaded = false;
        for (const auto& existing : assets) {
            if (!existing.is_procedural && existing.name == asset_name) {
                already_loaded = true;
                break;
            }
        }
        if (already_loaded) continue;

        ModelAsset asset;
        asset.name = asset_name;
        asset.type = CUBE;
        asset.is_procedural = false;
        asset.filepath = path.string();

        if (!ensure_model_asset_loaded(asset)) continue;

        assets.push_back(asset);
    }
}

void refresh_models(std::string project_path, Scene& scene) {
    namespace fs = std::filesystem;

    std::unordered_map<std::string, Model> old;

    for (auto& a : assets) {
        if (!a.is_procedural && a.loaded_model.meshCount > 0)
            old[a.name] = a.loaded_model;
    }

    std::vector<ModelAsset> next;

    for (auto& a : assets)
        if (a.is_procedural)
            next.push_back(a);

    fs::path resource_dir = fs::path(project_path) / "resources";
    if (!fs::exists(resource_dir)) fs::create_directories(resource_dir);

    for (const auto& path : collect_model_paths(resource_dir)) {
        std::string name = fs::relative(path, resource_dir).generic_string();

        ModelAsset a;
        a.name = name;
        a.type = CUBE;
        a.is_procedural = false;
        a.filepath = path.string();

        if (old.count(name)) {
            a.loaded_model = old[name];
            old.erase(name);
        } 
        
        else {
            if (!ensure_model_asset_loaded(a)) continue;
        }

        next.push_back(a);
    }

    for (auto& [_, m] : old) {
        UnloadModel(m);
    }

    assets = std::move(next);

    for (auto& entity : scene.entities) {
        MeshComponent* mesh = entity.get_mesh_component();
        if (!mesh || mesh->asset_name.empty()) continue;
        mesh->asset = nullptr;
        for (auto& a : assets) {
            if (a.name == mesh->asset_name) {
                mesh->asset = &a;
                break;
            }
        }
    }
}