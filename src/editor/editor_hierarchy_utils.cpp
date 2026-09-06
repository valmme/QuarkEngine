#include "editor/editor_hierarchy_utils.h"
#include "imgui.h"
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

static Vec3 normalize_or_forward(const Vec3& v) {
    const float length = v.length();
    if (length < 1e-9f) return Vec3{0.0f, 1.0f, 0.0f};
    return v * (1.0f / length);
}

static Vec3 matrix_column(const Mat4& matrix, int column) {
    return Vec3{matrix.m[column * 4], matrix.m[column * 4 + 1], matrix.m[column * 4 + 2]};
}

static float vec3_dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float vec3_squared_length(const Vec3& v) {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

static Mat4 polar_rotation(const Mat4& matrix) {
    Mat4 q = matrix;
    for (int iteration = 0; iteration < 24; ++iteration) {
        const Mat4 inverted = q.inverted();
        const Mat4 inverted_transpose = Mat4Transpose(inverted);
        for (int i = 0; i < 16; ++i) q.m[i] = 0.5f * (q.m[i] + inverted_transpose.m[i]);
    }
    for (int column = 0; column < 3; ++column) {
        const Vec3 axis = normalize_or_forward(matrix_column(q, column));
        q.m[column * 4] = axis.x;
        q.m[column * 4 + 1] = axis.y;
        q.m[column * 4 + 2] = axis.z;
    }
    return q;
}

static void decompose_transform(const Mat4& parent_transform, const Mat4& world_transform, TransformComponent& transform) {
    const Mat4 local = parent_transform.inverted() * world_transform;
    transform.position = {local.m[12], local.m[13], local.m[14]};

    Mat4 parent_3x3{};
    Mat4 world_3x3{};
    for (int column = 0; column < 3; ++column) {
        for (int row = 0; row < 3; ++row) {
            parent_3x3.m[column * 4 + row] = parent_transform.m[column * 4 + row];
            world_3x3.m[column * 4 + row] = world_transform.m[column * 4 + row];
        }
    }

    Vec3 scale = {
        matrix_column(local, 0).length(),
        matrix_column(local, 1).length(),
        matrix_column(local, 2).length()
    };
    if (scale.x < 1e-6f) scale.x = 1.0f;
    if (scale.y < 1e-6f) scale.y = 1.0f;
    if (scale.z < 1e-6f) scale.z = 1.0f;

    for (int iteration = 0; iteration < 16; ++iteration) {
        Mat4 inverse_scale{};
        inverse_scale.m[0] = (scale.x > 1e-6f) ? 1.0f / scale.x : 1.0f;
        inverse_scale.m[5] = (scale.y > 1e-6f) ? 1.0f / scale.y : 1.0f;
        inverse_scale.m[10] = (scale.z > 1e-6f) ? 1.0f / scale.z : 1.0f;

        Mat4 rotation = polar_rotation(Mat4Transpose(parent_3x3) * world_3x3 * inverse_scale);

        const Mat4 scaled_rotation = parent_3x3 * rotation;
        for (int column = 0; column < 3; ++column) {
            const Vec3 a = matrix_column(scaled_rotation, column);
            const Vec3 b = matrix_column(world_3x3, column);
            const float denominator = vec3_squared_length(a);
            float next = (denominator > 1e-6f) ? vec3_dot(a, b) / denominator : 0.0f;
            if (next < 0.0f) next = 0.0f;
            if (column == 0) scale.x = next;
            else if (column == 1) scale.y = next;
            else scale.z = next;
        }

        if (iteration == 15) {
            const Vec3 right = matrix_column(rotation, 0);
            const Vec3 up = matrix_column(rotation, 1);
            const Vec3 dir = matrix_column(rotation, 2);
            transform.rotation = {
                atan2f(-dir.y, dir.z) * RAD2DEG,
                asinf(dir.x) * RAD2DEG,
                atan2f(-up.x, right.x) * RAD2DEG
            };
        }
    }
    transform.scale = scale;

    constexpr float kEpsilon = 0.0001f;
    auto cleanup = [](float& value) {
        if (fabsf(value) < kEpsilon) value = 0.0f;
        if (fabsf(value - 1.0f) < kEpsilon) value = 1.0f;
    };
    cleanup(transform.position.x); cleanup(transform.position.y); cleanup(transform.position.z);
    cleanup(transform.rotation.x); cleanup(transform.rotation.y); cleanup(transform.rotation.z);
    cleanup(transform.scale.x); cleanup(transform.scale.y); cleanup(transform.scale.z);
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
    const Mat4 parent_transform = new_parent_id >= 0
        ? compose_world_transform(scene, new_parent_id)
        : Mat4::identity();

    scene.entities[entity_id].parent_id = new_parent_id;
    if (TransformComponent* transform = scene.entities[entity_id].get_transform_component()) {
        decompose_transform(parent_transform, world_transform, *transform);
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
