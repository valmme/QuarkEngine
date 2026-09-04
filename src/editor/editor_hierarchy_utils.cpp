#include "editor/editor_hierarchy_utils.h"
#include "imgui.h"
#include "ImGuizmo.h"
#include <algorithm>
#include <cmath>

static Mat4 compose_local_transform(const Entity& entity) {
    const TransformComponent* transform = entity.get_transform_component();
    if (!transform) return Mat4::identity();

    return Mat4::translation(transform->position.x, transform->position.y, transform->position.z) *
        Mat4::rotationX(transform->rotation.x * DEG2RAD) *
        Mat4::rotationY(transform->rotation.y * DEG2RAD) *
        Mat4::rotationZ(transform->rotation.z * DEG2RAD) *
        Mat4::scale(transform->scale.x, transform->scale.y, transform->scale.z);
}

static Mat4 compose_world_transform(const Scene& scene, int entity_index, std::vector<int>& stack) {
    if (entity_index < 0 || entity_index >= static_cast<int>(scene.entities.size())) return Mat4::identity();
    if (std::find(stack.begin(), stack.end(), entity_index) != stack.end())
        return compose_local_transform(scene.entities[entity_index]);

    stack.push_back(entity_index);
    const Entity& entity = scene.entities[entity_index];
    Mat4 world = compose_local_transform(entity);
    if (entity.parent_id >= 0 && entity.parent_id < static_cast<int>(scene.entities.size()))
        world = compose_world_transform(scene, entity.parent_id, stack) * world;
    stack.pop_back();
    return world;
}

static Mat4 compose_world_transform(const Scene& scene, int entity_index) {
    std::vector<int> stack;
    return compose_world_transform(scene, entity_index, stack);
}

static void decompose_transform(const Mat4& matrix, TransformComponent& transform) {
    float components[3][3] = {};
    float values[16] = {};
    std::copy(std::begin(matrix.m), std::end(matrix.m), std::begin(values));
    ImGuizmo::DecomposeMatrixToComponents(
        values, components[0], components[1], components[2]);
    transform.position = {components[0][0], components[0][1], components[0][2]};
    transform.rotation = {components[1][0], components[1][1], components[1][2]};
    transform.scale = {components[2][0], components[2][1], components[2][2]};
}

std::vector<int> get_entity_children(const Scene& scene, int parent_id) {
    std::vector<int> children;
    for (int i = 0; i < static_cast<int>(scene.entities.size()); i++) {
        if (scene.entities[i].parent_id == parent_id) {
            children.push_back(i);
        }
    }
    return children;
}

std::vector<int> get_entity_descendants(const Scene& scene, int entity_id) {
    std::vector<int> descendants;
    std::vector<int> to_process = get_entity_children(scene, entity_id);
    
    while (!to_process.empty()) {
        int current = to_process.back();
        to_process.pop_back();
        
        descendants.push_back(current);
        
        auto children = get_entity_children(scene, current);
        for (int child : children) {
            to_process.push_back(child);
        }
    }
    
    return descendants;
}

void move_entity_to_parent(Scene& scene, int entity_id, int new_parent_id) {
    if (entity_id < 0 || entity_id >= static_cast<int>(scene.entities.size())) return;
    if (new_parent_id == entity_id) return;
    
    if (new_parent_id >= 0) {
        auto descendants = get_entity_descendants(scene, entity_id);
        for (int desc : descendants) {
            if (desc == new_parent_id) return;
        }
    }
    
    const Mat4 world_transform = compose_world_transform(scene, entity_id);
    const Vec3 world_position = world_transform * Vec3{0.0f, 0.0f, 0.0f};
    const Mat4 parent_transform = new_parent_id >= 0
        ? compose_world_transform(scene, new_parent_id)
        : Mat4::identity();
    const Mat4 inverse_parent_transform = parent_transform.inverted();
    const Mat4 local_transform = inverse_parent_transform * world_transform;

    scene.entities[entity_id].parent_id = new_parent_id;
    if (TransformComponent* transform = scene.entities[entity_id].get_transform_component()) {
        decompose_transform(local_transform, *transform);
        transform->position = inverse_parent_transform * world_position;
    }
}

int create_group(Scene& scene, const std::string& name, int parent_id) {
    Entity group;
    group.id = static_cast<int>(scene.entities.size());
    group.name = name;
    group.parent_id = parent_id;
    group.is_group = true;
    
    scene.entities.push_back(group);
    return group.id;
}

void delete_group(Scene& scene, int group_id, bool reparent_to_parent) {
    if (group_id < 0 || group_id >= static_cast<int>(scene.entities.size())) return;
    
    Entity& group = scene.entities[group_id];
    if (!group.is_group) return;
    
    int parent_of_group = group.parent_id;
    
    if (reparent_to_parent) {
        auto children = get_entity_children(scene, group_id);
        for (int child : children) {
            scene.entities[child].parent_id = parent_of_group;
        }
    }
    
    scene.entities.erase(scene.entities.begin() + group_id);
    
    for (int i = group_id; i < static_cast<int>(scene.entities.size()); i++) {
        scene.entities[i].id = i;
    }
}

bool is_entity_group(const Entity& entity) {
    return entity.is_group;
}

std::vector<int> get_root_entities(const Scene& scene) {
    return get_entity_children(scene, -1);
}
