#include "core/asset_manager.h"
#include "core/renderer.h"
#include "loaders/obj_loader.h"
#include "math/math_utils.h"
#include "platform/sdl_window.h"
#include "scene/entity.h"
#include "scene/material.h"
#include "scene/scene.h"
#include "shading/pbr.h"
#include "shading/texture.h"
#include <iostream>
#include <string>
#include <vector>

int main(int, char *[])
{
    const int WIDTH = 800;
    const int HEIGHT = 600;
    SDLWindow window("swr — software renderer", WIDTH, HEIGHT);
    Renderer renderer(WIDTH, HEIGHT);
    AssetManager assets;
    Scene scene;

    // --- Camera ---
    scene.camera.position = {0.f, 4.f, 10.f};
    scene.camera.yaw = -MathUtils::HALF_PI;
    scene.camera.pitch = MathUtils::toRadians(-15.f);
    scene.camera.fovY = MathUtils::toRadians(60.f);
    scene.camera.aspect = (float)WIDTH / (float)HEIGHT;
    scene.camera.zNear = 0.1f;
    scene.camera.zFar = 100.f;

    // --- Lights ---
    Light key;
    key.type = LightType::Directional;
    key.direction = Vec3{-1.f, -1.f, -1.f}.normalized();
    key.color = {1.f, 1.f, 1.f};
    key.intensity = 2.f;
    key.castsShadow = true;
    scene.lights.add(key);
    Light fill;
    fill.type = LightType::Directional;
    fill.direction = Vec3{1.f, -0.5f, 0.f}.normalized();
    fill.color = {0.8f, 0.8f, 1.f};
    fill.intensity = 0.5f;
    scene.lights.add(fill);
    Light flash;
    flash.type = LightType::Point;
    flash.color = {1.f, 0.95f, 0.85f};
    flash.intensity = 0.f;
    flash.attConstant = 1.f;
    flash.attLinear = 0.22f;
    flash.attQuadratic = 0.2f;
    scene.lights.add(flash);

    scene.shadowMap.width = 1024;
    scene.shadowMap.height = 1024;
    scene.shadowMap.bias = 0.012f;
    scene.shadowMap.setup(key.direction, {0.f, 0.f, 0.f}, 12.f, 0.1f, 40.f);

    // --- Shaders & Materials ---
    PBRShader centerPBR;
    centerPBR.albedoMap = assets.getTexture("assets/textures/gabagool.jpg");
    centerPBR.normalMap = assets.getTexture("assets/textures/cube_normal.png");
    centerPBR.metallic = 0.3f;
    centerPBR.roughness = 0.4f;

    PBRShader blockPBR;
    blockPBR.albedoMap = assets.getTexture("assets/textures/gospelv1.png");
    blockPBR.metallic = 0.0f;
    blockPBR.roughness = 0.9f;

    PBRShader groundPBR;
    groundPBR.albedo = {0.4f, 0.38f, 0.35f};
    groundPBR.roughness = 0.85f;
    PBRShader pillarPBR;
    pillarPBR.albedo = {0.35f, 0.3f, 0.3f};
    pillarPBR.roughness = 0.7f;

    PBRShader gold;
    gold.albedo = {1.f, 0.85f, 0.4f};
    gold.metallic = 1.0f;
    gold.roughness = 0.15f;
    PBRShader plastic;
    plastic.albedo = {0.1f, 0.5f, 0.9f};
    plastic.metallic = 0.0f;
    plastic.roughness = 0.1f;
    PBRShader matte;
    matte.albedo = {0.8f, 0.2f, 0.2f};
    matte.metallic = 0.0f;
    matte.roughness = 0.9f;

    Material groundMat = {&groundPBR, CullMode::None, false}, centerMat = {&centerPBR, CullMode::Back, true},
             pillarMat = {&pillarPBR, CullMode::Back, true};
    Material blockMat = {&blockPBR, CullMode::Back, true}, matGold = {&gold, CullMode::Back, true},
             matPlastic = {&plastic, CullMode::Back, true}, matMatte = {&matte, CullMode::Back, true};

    // --- Entity Hierarchy ---
    Entity *ground = scene.createEntity("ground");
    ground->mesh = assets.getMesh("assets/models/ground.obj");
    ground->material = &groundMat;
    ground->transform.scaleVec = {2.f, 1.f, 2.f};

    Entity *center = scene.createEntity("center");
    center->mesh = assets.getMesh("assets/models/cube.obj");
    center->material = &centerMat;
    center->transform.position = {0.f, 1.5f, 0.f};

    float pDist = 4.0f;
    Vec3 pPos[4] = {{pDist, 2.f, pDist}, {-pDist, 2.f, pDist}, {pDist, 2.f, -pDist}, {-pDist, 2.f, -pDist}};
    for (int i = 0; i < 4; ++i)
    {
        Entity *p = scene.createEntity("pillar_" + std::to_string(i));
        p->mesh = assets.getMesh("assets/models/cube.obj");
        p->material = &pillarMat;
        p->transform.position = pPos[i];
        p->transform.scaleVec = {0.8f, 4.0f, 0.8f};
    }

    Material *oMats[3] = {&matGold, &matPlastic, &matMatte};
    std::vector<Entity *> orbiters;
    for (int i = 0; i < 3; ++i)
    {
        Entity *o = scene.createEntity("orbiter_" + std::to_string(i));
        o->mesh = assets.getMesh("assets/models/cube.obj");
        o->material = oMats[i];
        o->transform.scaleVec = {0.4f, 0.4f, 0.4f};
        center->addChild(o);
        orbiters.push_back(o);
    }

    Entity *block = scene.createEntity("block");
    block->mesh = assets.getMesh("assets/models/cube.obj");
    block->material = &blockMat;
    block->transform.position = {0.f, 5.0f, 0.f};

    // --- Loop ---
    float angle = 0.f;
    uint32_t last = SDL_GetTicks();
    while (window.pollEvents())
    {
        uint32_t now = SDL_GetTicks();
        float dt = (now - last) / 1000.f;
        last = now;
        angle += 1.5f * dt;
        const uint8_t *keys = SDL_GetKeyboardState(nullptr);
        if (keys[SDL_SCANCODE_1])
            Shader::globalDebugMode = DebugMode::None;
        if (keys[SDL_SCANCODE_2])
            Shader::globalDebugMode = DebugMode::Normals;
        if (keys[SDL_SCANCODE_3])
            Shader::globalDebugMode = DebugMode::UVs;
        if (keys[SDL_SCANCODE_4])
            Shader::globalDebugMode = DebugMode::Shadows;
        if (keys[SDL_SCANCODE_5])
            Shader::globalDebugMode = DebugMode::Tangents;

        scene.lights.lights[2].position = scene.camera.position;
        scene.lights.lights[2].intensity = keys[SDL_SCANCODE_F] ? 5.0f : 0.0f;
        center->transform.rotation.y = angle;
        center->transform.rotation.x = angle * 0.5f;
        for (int i = 0; i < 3; ++i)
        {
            float off = angle + (i * MathUtils::TWO_PI / 3.f);
            orbiters[i]->transform.position = {std::cos(off) * 2.5f, std::sin(angle * 2.f + i) * 0.5f,
                                               std::sin(off) * 2.5f};
            orbiters[i]->transform.rotation.x += 1.0f * dt;
        }
        const InputState &in = window.input();
        scene.camera.update(dt, in.w, in.s, in.a, in.d, in.e, in.q, in.shift, in.mouseDX, in.mouseDY);
        renderer.render(scene);
        window.present(renderer.pixelData());
    }
    return 0;
}