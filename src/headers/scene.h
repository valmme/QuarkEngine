#pragma once
#include <vector>
#include "entity.h"
#include <memory>

struct Scene {
    std::vector<Entity> entities;
    int selected = -1;

    void add_object(Model model, ObjectType type);
    Entity* get_selected();
    void release_resources();
};
