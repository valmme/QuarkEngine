#pragma once
#include "raylib.h"
#include "entity.h"
#include <functional>

extern std::vector<ModelAsset> assets;

void load_models();
void update_model(Entity* e);
void unload_models();
