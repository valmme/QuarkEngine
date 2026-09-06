#ifndef __EDITOR_ENTITY_H__
#define __EDITOR_ENTITY_H__

#include <filesystem>
#include "scene.h"
#include "entity.h"

namespace fs = std::filesystem;

Entity make_entity_from_asset(Scene& scene, ModelAsset& asset);

Entity make_light_entity(Scene& scene, int parent_index);

void assign_entity_name(Entity& entity, const char* new_name);

void make_prefab(Entity entity, const fs::path path);
Entity make_entity_from_prefab(Scene& scene, const fs::path filename);

#endif // __EDITOR_ENTITY_H__
