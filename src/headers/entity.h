#pragma once
#include "raylib.h"
#include "rlgl.h"
#include <string>
#include <functional>

enum ObjectType { CUBE, SPHERE, CONE, CYLINDER, HEMISPHERE, TORUS };

struct ModelAsset {
    std::string name;
    ObjectType type;
    bool isProcedural;
    std::function<Model(int)> generator;
    Model loadedModel = {0};
};

struct Entity {
    int id;
    std::string name;
    
    Vector3 position;
    Vector3 rotation;
    Vector3 scale;

    Texture2D texture = {0};

    bool auto_uv;
    bool texture_stretch = true; 

    Model model = {0};
    ModelAsset* asset;

    float texture_repeat_u;
    float texture_repeat_v;
    Vector2 uv_scale_vec;
    float uv_scale;
    std::vector<std::vector<float>> original_texcoords;

    int segments;

    ObjectType type;

    Color color;
    Color outline_color;

    Entity()
        : id(0),
          position{0, 0, 0},
          rotation{0, 0, 0},
          scale{1, 1, 1},
          auto_uv(false),
          texture_repeat_u(1.0f),
          texture_repeat_v(1.0f),
          uv_scale(1.0f),
          uv_scale_vec{1, 1},
          color(WHITE),
          outline_color(LIGHTGRAY),
          asset(nullptr),
          segments(16),
          type(CUBE)
    {}

    Entity(int _id)
        : id(_id),
          position{0, 0, 0},
          rotation{0, 0, 0},
          scale{1, 1, 1},
          auto_uv(false),
          texture_repeat_u(1.0f),
          texture_repeat_v(1.0f),
          uv_scale(1.0f),
          uv_scale_vec{1, 1},
          color(WHITE),
          outline_color(LIGHTGRAY),
          asset(nullptr),
          segments(16),
          type(CUBE)
    {
        name = "Object " + std::to_string(id);
    }
};
