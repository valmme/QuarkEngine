#ifndef __SCENE_H__
#define __SCENE_H__
#include <vector>
#include "entity.h"
#include "lighting.h"
#include <memory>
#include <string>

struct Scene {
    std::vector<Entity> entities;

    int selected = -1;
    std::vector<int> selected_entities;

    Entity* get_selected();
    bool is_selected(int entity_index) const;
    void select_entity(int entity_index, bool additive);
    std::string make_unique_name(const std::string& base_name) const;
    std::string make_default_name_for(const Entity& entity) const;
    void release_resources();
};

#endif // __SCENE_H__
